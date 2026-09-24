#include <zap/actor/ActorSpawnerEx.h>
#include <actor/ActorMgr.h>
#include <actor/MapActor.h>
#include <map/SwitchFlagMgr.h>
#include <red/utility/SpriteUtil.h>
#include <zap/Zap.h>
#include <red/event/ResourceLoadEvent.h>
#include <map/CourseData.h>
#include <game_info/CourseInfo.h>
#include <red/profile/MapActorMgr.h>
#include <zap/actor/StringBank.h>
#include <zap/actor/NybbleBank.h>

SEAD_RTTI_OVERRIDE_IMPL(zap::ActorSpawnerEx, Actor)

const ActorCreateInfo zap::ActorSpawnerEx::cCreateInfo = {
    .offset_x = 0, .offset_y = 0,
    .spawn_range = {
        .offset_x = 0, .offset_y = 0,
        .half_size_x = 0, .half_size_y = 0
    },
    .cull_range = { 
        .up = 0, .down = 0, .left = 0, .right = 0
    },
    .flag = ActorCreateInfo::cFlag_IgnoreSpawnRange
};

Profile* zap::ActorSpawnerEx::sProfile = zap::getRegistrar()->newProfile<zap::ActorSpawnerEx>("actor_spawner_ex")
    .createInfo(cCreateInfo)
    .build();

zap::ActorSpawnerEx::ActorSpawnerEx(const ActorCreateParam& param)
    : Actor(param)
    , mSpawnProfileID(0xFFFF)
    , mSpawnEventID(0)
    , mSpawned(false)
    , mPrevFrameEvent(false)
    , mStringScanAttempt(0)
    , mNybbleScanAttempt(0)
    , mStartPos(mPos)
{ }

ActorBase::Result zap::ActorSpawnerEx::create() {
    mSpawnEventID = red::SpriteUtil::getNybbleRange(this, 1, 2);
    if (mSpawnEventID == 0) {
        tk::fatal("ActorSpawnerEx: Event ID was unset.");
        return cResult_Failed;
    }
    mSpawnEventID -= 1;  // events are 0-indexed

    // Setting: Multi-use
    mMultiUse = red::SpriteUtil::getNybble9(this);

    // Setting: ID type
    switch (red::SpriteUtil::getNybble5(this)) {
        case 0: { // Sprite ID
            mSpawnProfileID = MapActor::cProfileID[red::SpriteUtil::getNybbleRange(this, 6, 8)];
            break;
        }
        
        case 1: { // Profile ID
            mSpawnProfileID = red::SpriteUtil::getNybbleRange(this, 6, 8);
            break;
        }
        
        default: { // String ID
            // We must perform some scans to find the string bank and then lookup the ID from that. Do it in execute() instead.
            break;
        }
    }

    execute();

    return cResult_Success;
}

bool zap::ActorSpawnerEx::execute() {
    if (mSpawnProfileID == 0xFFFF) {
        if (!scanString()) {
            return true;
        }
    }
    
    if (!mNybbleBank.isValid()) {
        if (!scanNybble()) {
            return true;
        }
    }
    
    if (mSpawned && !mMultiUse)
        return true;

    bool event = SwitchFlagMgr::instance()->isActivated(mSpawnEventID);
    
    if (event && !mPrevFrameEvent) {
        ActorCreateParam child;
        child.profile = red::ProfileEx::get(mSpawnProfileID);

        const ActorCreateInfo& info = child.profile->getActorCreateInfo();
        child.position = mPos - sead::Vector3f(info.offset_x, info.offset_y, 0.0f); // account for spawn offs
        
        const Actor* nybbleBank = ActorMgr::instance()->getActorPtr<Actor>(mNybbleBank);
        if (nybbleBank == nullptr) [[unlikely]] {
            tk::fatal("ActorSpawnerEx: Nybble Bank was lost!");
        }
        
        child.param_0 = nybbleBank->getParam0();
        child.param_1 = nybbleBank->getParam1();
        child.param_ex_0.course.switch_flag_0 = nybbleBank->getSwitchFlag0();
        child.param_ex_0.course.switch_flag_1 = nybbleBank->getSwitchFlag1();
        child.param_ex_0.course.layer = nybbleBank->getLayer();
        child.param_ex_1.course.movement_id = nybbleBank->getParamEx().course.movement_id;
        child.param_ex_1.course.link_id = nybbleBank->getParamEx().course.link_id;
        child.param_ex_1.course.init_state_flag = nybbleBank->getParamEx().course.init_state_flag;
        
        ActorMgr::instance()->createImmediately(child);
        mSpawned = true;
    }
    
    mPrevFrameEvent = event;

    return true;
}

bool zap::ActorSpawnerEx::scanString() {
    // Setting: String Bank ID
    const u8 stringBankID = red::SpriteUtil::getNybble6(this);
    
    mStringScanAttempt++;
    if (mStringScanAttempt > 5) {
        tk::fatal("ActorSpawnerEx: Failed to find String Bank with ID %i after 5 attempts...", stringBankID);
        return false;
    }
    
    ActorMgr* actorMgr = ActorMgr::instance();
    
    for (auto it = actorMgr->getActorBegin(); it != actorMgr->getActorEnd(); it++) {
        if (StringBank* bank = sead::DynamicCast<StringBank>(*it)) {
            if (bank->getBankID() == stringBankID && bank->getType() == StringBank::Type::Primary) {
                mSpawnProfileID = red::ProfileEx::get(bank->getString())->getID();
                return true;
            }
        }
    }
    
    return false;
}

bool zap::ActorSpawnerEx::scanNybble() {
    if (mNybbleBank.isValid())
        return true;

    mNybbleScanAttempt++;
    if (mNybbleScanAttempt > 5) {
        tk::fatal("ActorSpawnerEx: Failed to find Nybble Bank after 5 attempts...\nDid you place it one tile to the right of this actor?");
        return false;
    }

    ActorMgr* actorMgr = ActorMgr::instance();

    for (auto it = actorMgr->getActorBegin(); it != actorMgr->getActorEnd(); it++) {
        if (NybbleBank* bank = sead::DynamicCast<NybbleBank>(*it)) {
            if (bank->getPos().x == mStartPos.x + 16.0f && bank->getPos().y == mStartPos.y) {
                mNybbleBank = bank->getActorUniqueID();
                tk::println("Found nybble bank");
                return true;
            } else {
                sead::Vector3f delta = bank->getPos() - mStartPos;
                tk::println("Nybble bank found, but not at the right position. Delta: %f, %f, %f", delta.x, delta.y, delta.z);
            }
        }
    }
    
    tk::println("Nybble bank not found");
    return false;
}

//! TODO: Preload resources, but it's more complicated here due to having to also scan/link stringbank chains
