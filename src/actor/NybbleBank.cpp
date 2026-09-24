#include <zap/actor/NybbleBank.h>
#include <zap/Zap.h>

SEAD_RTTI_OVERRIDE_IMPL(zap::NybbleBank, Actor)

const ActorCreateInfo zap::NybbleBank::cCreateInfo = {
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

Profile* zap::NybbleBank::sProfile = zap::getRegistrar()->newProfile<zap::NybbleBank>("nybble_bank")
    .createInfo(cCreateInfo)
    .build();

zap::NybbleBank::NybbleBank(const ActorCreateParam& param)
    : Actor(param)
{ }
