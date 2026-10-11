#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SnowGnorcAnimInstance.generated.h"
UCLASS(Transient, Blueprintable)
class SPYROGAMEPLAY_API USnowGnorcAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
