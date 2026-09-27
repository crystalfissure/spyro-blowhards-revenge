#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TownSquareEnemyAnimInstance.generated.h"
UCLASS(Transient)
class SPYROGAMEPLAY_API UTownSquareEnemyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
UCLASS(Transient)
class SPYROGAMEPLAY_API UBullAnimInstance : public UTownSquareEnemyAnimInstance { GENERATED_BODY() };
UCLASS(Transient)
class SPYROGAMEPLAY_API UToreadorAnimInstance : public UTownSquareEnemyAnimInstance { GENERATED_BODY() };
