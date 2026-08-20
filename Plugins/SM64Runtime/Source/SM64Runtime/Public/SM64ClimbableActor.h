#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "SM64ClimbableActor.generated.h"

UENUM(BlueprintType)
enum class ESM64ClimbableType : uint8
{
    Tree,
    GenericPole,
    GiantWFPole
};

UENUM(BlueprintType)
enum class ESM64ClimbState : uint8
{
    Available,
    Holding,
    Climbing,
    AtTop
};

/** Configurable wrapper for bhvTree, bhvPoleGrabbing, and bhvGiantPole. */
UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64ClimbableActor : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    ASM64ClimbableActor();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UCapsuleComponent* ClimbVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    ESM64ClimbableType ClimbableType = ESM64ClimbableType::Tree;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    bool bApplyVanillaMetadataOnConstruction = true;

    /** bhvPoleGrabbing multiplies this source byte by ten. WF's helper passes 61. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable", meta = (ClampMin = "0", ClampMax = "255"))
    int32 BehaviorHeightByte = 61;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    float ClimbRadius = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    float ClimbHeight = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    float TopClearance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    float LowerInteractionPadding = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    float UpperInteractionPadding = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    float HitboxDownOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    float PushRadius = 70.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    int32 PushDelayFrames = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Climbable")
    bool bAllowTopTransition = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Climbable")
    AActor* Climber = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Climbable")
    float ClimbPosition = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Climbable")
    float VerticalClimbInput = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Climbable")
    float HorizontalClimbInput = 0.0f;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SM64|Climbable")
    void ApplyVanillaMetadata();

    UFUNCTION(BlueprintPure, Category = "SM64|Climbable")
    bool CanAttachClimber(AActor* Candidate) const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Climbable")
    bool AttachClimber(AActor* NewClimber);

    UFUNCTION(BlueprintCallable, Category = "SM64|Climbable")
    void SetClimbInput(float NormalizedVertical, float NormalizedHorizontal);

    UFUNCTION(BlueprintCallable, Category = "SM64|Climbable")
    void JumpOffClimbable();

    UFUNCTION(BlueprintCallable, Category = "SM64|Climbable")
    void DetachClimber(bool bReachedFloor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Climbable")
    void OnClimbableContactCandidate(AActor* Candidate, float SourceHeightAlongPole);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Climbable")
    void OnRequestAttachToClimbable(AActor* NewClimber, FVector AttachLocation, float SourceHeightAlongPole);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Climbable")
    void OnClimbTransformUpdated(AActor* CurrentClimber, FVector AttachLocation,
        float SourceHeightAlongPole, float FacingYawDeltaDegrees);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Climbable")
    void OnReachedClimbableTop(AActor* CurrentClimber, FVector TopLocation, bool bCanHandstand);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Climbable")
    void OnRequestDetachFromClimbable(AActor* ReleasedClimber, bool bJumpedOff, bool bReachedFloor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Climbable")
    void OnTreeLeafEffect(AActor* CurrentClimber);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Climbable")
    void OnGiantPoleTopBallRequested(FVector BallWorldLocation);

protected:
    UFUNCTION()
    void OnClimbVolumeBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void UpdateVolumeFromMetadata();

    FVector BaseLocation = FVector::ZeroVector;
    bool bBaseCaptured = false;
    bool bTopNotified = false;
};
