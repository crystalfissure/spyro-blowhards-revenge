#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "SM64ActActor.h"
#include "SM64Interactable.h"
#include "SM64BreakableActor.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64BreakableActor : public ASM64ActActor, public ISM64Interactable
{
    GENERATED_BODY()

public:
    ASM64BreakableActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Interaction")
    ESM64AttackType RequiredAttack = ESM64AttackType::Cannon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Interaction")
    bool bDestroyActorOnBreak = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Interaction")
    bool bBroken = false;

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual bool HandleSM64Attack_Implementation(
        ESM64AttackType AttackType,
        AActor* InstigatorActor,
        FVector ImpactPoint,
        FVector ImpactDirection) override;

    UFUNCTION(BlueprintCallable, Category = "SM64|Interaction")
    virtual void BreakObject(AActor* InstigatorActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Interaction")
    void OnBroken(AActor* InstigatorActor);
};
