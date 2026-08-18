#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SM64Types.h"
#include "SM64Interactable.generated.h"

UINTERFACE(Blueprintable)
class SM64RUNTIME_API USM64Interactable : public UInterface
{
    GENERATED_BODY()
};

class SM64RUNTIME_API ISM64Interactable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SM64|Interaction")
    bool HandleSM64Attack(
        ESM64AttackType AttackType,
        AActor* InstigatorActor,
        FVector ImpactPoint,
        FVector ImpactDirection);
};
