#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "WFCheckerboardElevatorPair.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWFCheckerboardCycleCompleted);

UCLASS(Blueprintable)
class SM64RUNTIME_API AWFCheckerboardElevatorPair : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    AWFCheckerboardElevatorPair();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* PlatformRootA;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* PlatformRootB;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* RenderMeshA;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* RenderMeshB;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* CollisionMeshA;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* CollisionMeshB;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultRenderMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* DefaultCollisionMesh = nullptr;

    /** BPARAM1: variant 0 is WF's 145/7.0 pair; variant 1 uses 235/11.6. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Checkerboard", meta = (ClampMin = "0", ClampMax = "1"))
    int32 GroupVariant = 0;

    /** BPARAM2; source substitutes 65 when it is zero. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Checkerboard", meta = (ClampMin = "0", ClampMax = "255"))
    int32 MoveDurationParameter = 0;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Checkerboard")
    FWFCheckerboardCycleCompleted OnPairCycleCompleted;

    UFUNCTION(BlueprintPure, Category = "SM64|Checkerboard")
    int32 GetPlatformAction(int32 PlatformIndex) const;

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Checkerboard")
    void OnCheckerboardPlatformDelta(int32 PlatformIndex, FVector TranslationDelta,
        FRotator RotationDelta, int32 NewAction);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Checkerboard")
    void OnCheckerboardElevatorSound(bool bPlayerWithin1000Units);

protected:
    void ConfigureVariant();
    void StepPlatform(int32 Index, USceneComponent* PlatformRoot);
    void SetPlatformAction(int32 Index, int32 NewAction);

    int32 PlatformActions[2] = { 0, 0 };
    int32 PlatformTimers[2] = { 0, 0 };
    float PlatformPitchDegrees[2] = { 0.0f, 0.0f };
    FVector InitialRelativeLocations[2];
    FVector VariantScale = FVector::OneVector;
    float RotationRadius = 7.0f;
    int32 EffectiveMoveDuration = 65;
};
