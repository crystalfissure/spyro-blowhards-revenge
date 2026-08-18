#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64ActActor.h"
#include "SM64WaterVolume.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSM64WaterActorEvent, AActor*, Actor);

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64WaterVolume : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64WaterVolume();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* WaterTrigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* SurfaceMesh;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Water")
    FSM64WaterActorEvent OnActorEnteredWater;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Water")
    FSM64WaterActorEvent OnActorExitedWater;

protected:
    UFUNCTION()
    void OnWaterBegin(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnWaterEnd(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);
};
