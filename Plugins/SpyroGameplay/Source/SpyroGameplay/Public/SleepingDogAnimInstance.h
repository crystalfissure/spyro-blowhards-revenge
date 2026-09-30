#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SleepingDogAnimInstance.generated.h"
UCLASS(Transient, Blueprintable)
class SPYROGAMEPLAY_API USleepingDogAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
