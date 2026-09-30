#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ToastyAnimInstance.generated.h"
UCLASS(Transient, Blueprintable)
class SPYROGAMEPLAY_API UToastyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
