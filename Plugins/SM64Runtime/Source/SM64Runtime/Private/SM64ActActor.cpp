#include "SM64ActActor.h"

ASM64ActActor::ASM64ActActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

bool ASM64ActActor::IsEnabledForAct(int32 TestAct) const
{
    if (TestAct < 1 || TestAct > 6)
    {
        return false;
    }
    return (ActMask & (1 << (TestAct - 1))) != 0;
}

void ASM64ActActor::SetCurrentAct(int32 NewAct)
{
    bActEnabled = IsEnabledForAct(NewAct);
    SetActorHiddenInGame(!bActEnabled);
    SetActorEnableCollision(bActEnabled);
    SetActorTickEnabled(bActEnabled);
}
