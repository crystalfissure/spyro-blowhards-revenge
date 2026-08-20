#pragma once

#include "CoreMinimal.h"
#include "SM64ActActor.h"
#include "SM64FixedStepActor.generated.h"

/**
 * Actor base that advances SM64 behavior state at the original 30 Hz.
 * The accumulator is intentionally retained when a frame hitch exceeds the
 * per-render-frame catch-up budget, so periodic behavior never loses frames.
 */
UCLASS(Abstract, Blueprintable)
class SM64RUNTIME_API ASM64FixedStepActor : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64FixedStepActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Simulation", meta = (ClampMin = "1.0"))
    float SimulationRate = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Simulation", meta = (ClampMin = "1", ClampMax = "240"))
    int32 MaxCatchUpStepsPerTick = 240;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Simulation")
    int32 RandomSeed = 0x6405;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Simulation")
    int64 SimulationFrame = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Simulation")
    int32 ActionCode = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Simulation")
    int32 ActionTimer = 0;

    UFUNCTION(BlueprintNativeEvent, Category = "SM64|Simulation")
    void SimulateSM64Frame();
    virtual void SimulateSM64Frame_Implementation();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Simulation")
    void OnSimulationFrame(int64 FrameNumber);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Simulation")
    void OnActionChanged(int32 PreviousAction, int32 NewAction);

protected:
    void SetActionCode(int32 NewAction);
    void FinishActionFrame(int32 PreviousAction);
    int32 RandomIntegerExclusive(int32 MaxExclusive);
    float RandomFloat();
    FVector GetSM64ForwardVector() const;
    float GetSM64YawTowardLocation(const FVector& TargetLocation) const;
    void TurnSM64YawTowardLocation(const FVector& TargetLocation, float MaxDegreesPerFrame);

    double FixedStepAccumulator = 0.0;
    FRandomStream DeterministicRandom;
};
