#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "SM64ActActor.h"
#include "SM64Interactable.h"
#include "WFKickableBoard.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API AWFKickableBoard : public ASM64ActActor, public ISM64Interactable
{
    GENERATED_BODY()

public:
    AWFKickableBoard();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool HandleSM64Attack_Implementation(
        ESM64AttackType AttackType,
        AActor* InstigatorActor,
        FVector ImpactPoint,
        FVector ImpactDirection) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* BoardMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* UprightMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* FelledMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    float SimulationHz = 30.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Motion")
    int32 BoardState = 0;

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Interaction")
    void OnBoardFelled(AActor* InstigatorActor);

protected:
    virtual void BeginPlay() override;
    void StepBoard();

    double StepAccumulator = 0.0;
    int32 StateTimer = 0;
    float RockPhaseUnits = 0.0f;
    float RockAmplitudeUnits = 1600.0f;
    float PitchVelocityUnits = 0.0f;
    FRotator HomeRotation;
    TWeakObjectPtr<AActor> LastInstigator;
};
