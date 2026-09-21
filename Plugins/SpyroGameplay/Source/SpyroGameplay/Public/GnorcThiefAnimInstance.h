#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GnorcThiefAnimInstance.generated.h"
UCLASS(Transient)
class SPYROGAMEPLAY_API UGnorcThiefAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
