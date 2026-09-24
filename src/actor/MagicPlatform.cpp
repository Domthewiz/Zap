#include <zap/actor/MagicPlatform.h>
#include <zap/Zap.h>
#include <game_info/CourseInfo.h>
#include <map/CourseData.h>
#include <red/utility/SpriteUtil.h>
#include <map/Bg.h>
#include <collision/ActorBgCollisionMgr.h>
#include <graphics/Renderer.h>

SEAD_RTTI_OVERRIDE_IMPL(zap::MagicPlatform, Actor)

using ACI = ActorCreateInfo;
const ActorCreateInfo zap::MagicPlatform::cCreateInfo = {
    .offset_x = 0, .offset_y = 0,
    .spawn_range = {
        .offset_x = 0, .offset_y = 0,
        .half_size_x = 0, .half_size_y = 0
    },
    .cull_range = { 
        .up = 0, .down = 0, .left = 0, .right = 0
    },
    .flag = ACI::cFlag_IgnoreSpawnRange | ACI::cFlag_MapObj
};

Profile* zap::MagicPlatform::sProfile = zap::getRegistrar()->newProfile<zap::MagicPlatform>("magicplatform")
    .createInfo(cCreateInfo)
    .build();

zap::MagicPlatform::MagicPlatform(const ActorCreateParam& param)
    : Actor(param)
    , mTileData(nullptr)
    , mTileSize(0, 0)
{ }

ActorBase::Result zap::MagicPlatform::create() {
    // Setting: Location ID
    u8 locationID = (red::SpriteUtil::getNybble11(this) << 4) | red::SpriteUtil::getNybble12(this);
    
    // find location
    const CourseDataFile* area = CourseData::instance()->getFile(CourseInfo::instance()->getFileNo());
    const Location* location = area->getLocation(nullptr, locationID);
    
    if (location == nullptr) {
        tk::fatal("MagicPlatform failed to get location");
        return cResult_Failed;
    }
    
    // init tile data
    u32 locX = location->offset.x & ~0xF;
    u32 locY = location->offset.y & ~0xF;
    mTileSize.x = (location->size.x + (location->offset.x & 0xF) + 0xF) / 16;
    mTileSize.y = (location->size.y + (location->offset.y & 0xF) + 0xF) / 16;
    
    if (mTileSize.x == 0 || mTileSize.y == 0) {
        tk::fatal("MagicPlatform failed to get tile size");
        return cResult_Failed;
    }
    
    // scan and copy tile data
    mTileData = new u16[mTileSize.x * mTileSize.y];
    for (u32 y = 0; y < mTileSize.y; y++) {
        for (u32 x = 0; x < mTileSize.x; x++) {
            u16* tile = Bg::getUnitCurrentCdFile(locX + x * 16, locY + y * 16, mLayer);
            mTileData[x + y * mTileSize.x] = tile ? *tile : 0;
        }
    }
    
    // Setting: Collision Type
    mCollisionType = red::SpriteUtil::getNybble10(this);
    if (mCollisionType >= cCollisionType_Max) {
        tk::fatal("MagicPlatform invalid collision type");
        return cResult_Failed;
    }
    
    // Setting: Collider Interaction Type
    const u8 interactionType = (red::SpriteUtil::getNybble7(this) << 4) | red::SpriteUtil::getNybble8(this);
    if (interactionType > BgCollision::cType_InvisibleBlock) {
        tk::fatal("MagicPlatform invalid interaction type");
        return cResult_Failed;
    }
    
    // Setting: Collider Surface Type
    const u8 surfaceType = red::SpriteUtil::getNybble9(this);
    if (surfaceType > BgUnitCode::cCarpet) {
        tk::fatal("MagicPlatform invalid surface type");
        return cResult_Failed;
    }
    
    switch (mCollisionType) {
        case cCollisionType_Solid: {
            mSolidCollider.set(this, {
                .pos_offset         = { 0.0f, 0.0f },
                .rot_pivot_offset   = { 0.0f, 0.0f },
                .left_top_offset    = { mTileSize.x * -8.0f, mTileSize.y *  8.0f },
                .right_under_offset = { mTileSize.x * 8.0f, mTileSize.y * -8.0f },
                .angle              = mAngle.z()
            });
            
            mSolidCollider.setType(static_cast<BgCollision::Type>(interactionType));
            mSolidCollider.setAttr(static_cast<BgUnitCode::Attr>(surfaceType));
            
            ActorBgCollisionMgr::instance()->entry(mSolidCollider);
            break;
        }
        
        case cCollisionType_Semisolid: {
            const sead::Vector2f points[2] = {
                { mTileSize.x * -8.0f, mTileSize.y * 8.0f },
                { mTileSize.x *  8.0f, mTileSize.y * 8.0f }
            };
            
            mSemisolidCollider.set(this, {
                .pos_offset       = { 0.0f, 0.0f },
                .rot_pivot_offset = { 0.0f, 0.0f },
                .points           = points,
                .angle            = mAngle.z()
            });
            
            mSemisolidCollider.setType(static_cast<BgCollision::Type>(interactionType));
            mSemisolidCollider.setAttr(static_cast<BgUnitCode::Attr>(surfaceType));
            
            ActorBgCollisionMgr::instance()->entry(mSemisolidCollider);
            
            break;
        }
    }
    
    // Setting: Movement Type
    const u8 movementType = red::SpriteUtil::getNybble20(this);
    if (movementType > ParentMovementType::cPos_KinokoLift) {
        tk::fatal("MagicPlatform invalid movement type");
        return cResult_Failed;
    }
    
    u32 movementMask = mMovementMgr.getTypeMask(static_cast<ParentMovementType>(movementType));
    
    // Setting: Movement ID
    mMovementMgr.link(mPos, movementMask, mParamEx.course.movement_id);
    
    return cResult_Success;
}

bool zap::MagicPlatform::execute() {
    mMovementMgr.execute();
    mPos = mMovementMgr.getPosition();
    mAngle.z() = mMovementMgr.getAngle();
    
    switch (mCollisionType) {
        case cCollisionType_Solid: {
            mSolidCollider.setAngle(mAngle.z());
            mSolidCollider.execute();
            break;
        }
        
        case cCollisionType_Semisolid: {
            mSemisolidCollider.setAngle(mAngle.z());
            mSemisolidCollider.execute();
            break;
        }
    }
    
    return true;
}

bool zap::MagicPlatform::draw() {
    f32 angleSin, angleCos;
    sead::Mathf::sinCosIdx(&angleSin, &angleCos, mAngle.z());
    
    for (u32 y = 0; y < mTileSize.y; y++) {
        for (u32 x = 0; x < mTileSize.x; x++) {
            const s32 offsetX  = x * 16 - mTileSize.x * 8 + 8;
            const s32 offsetY  = y * 16 - mTileSize.y * 8 + 8;
            const f32 rotatedX =  offsetX * angleCos + offsetY * angleSin;
            const f32 rotatedY = -offsetX * angleSin + offsetY * angleCos;
            
            const sead::Vector3f drawPos(mPos.x + rotatedX, mPos.y - rotatedY, mPos.z);
            
            Renderer::instance()->drawActorBgUnit(static_cast<UnitID>(mTileData[y * mTileSize.x + x]), drawPos, mAngle.z(), mScale);
        }
    }
    
    return true;
}
