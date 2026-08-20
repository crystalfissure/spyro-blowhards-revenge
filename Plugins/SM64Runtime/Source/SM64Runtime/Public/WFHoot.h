#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "WFHoot.generated.h"

class UAnimSequence;

UENUM(BlueprintType)
enum class EWFHootState : uint8
{
    AsleepInTree = 0,
    WantsToTalk = 1,
    ReadyToFly = 2,
    Ascent = 3,
    Carry = 4,
    Tired = 5
};

UCLASS(Blueprintable)
class SM64RUNTIME_API AWFHoot : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    AWFHoot();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* WakeSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USkeletalMeshComponent* CharacterMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    USkeletalMesh* DefaultSkeletalMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Animation")
    UAnimSequence* FreeFlightAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Animation")
    UAnimSequence* CarryAnimation = nullptr;

    /** Source home offset (800,-150,300), mapped to UE (800,300,-150). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    FVector HomeOffset = FVector(800.0f, 300.0f, -150.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    FVector CourseOrigin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    float WakeRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    float AscentTargetHeight = 6500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    float TiredDialogHeight = 2700.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    bool bAutoCompleteIntroDialog = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    bool bAutoAttachOnWakeOverlap = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    bool bAutoCompleteTiredDialog = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    FVector RiderRelativeOffset = FVector(0.0f, 0.0f, -120.0f);

    /** Read keyboard/gamepad steering directly when no Blueprint supplies SetSteeringInput. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    bool bPollNativeSteeringInput = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hoot")
    TEnumAsByte<ECollisionChannel> FloorTraceChannel = ECC_WorldStatic;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Hoot")
    AActor* Rider = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Hoot")
    float FlightPitchDegrees = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Hoot")
    float SteeringInput = 0.0f;

    UFUNCTION(BlueprintPure, Category = "SM64|Hoot")
    EWFHootState GetHootState() const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Hoot")
    void CompleteIntroDialog();

    UFUNCTION(BlueprintCallable, Category = "SM64|Hoot")
    bool AttachRider(AActor* RiderActor);

    UFUNCTION(BlueprintCallable, Category = "SM64|Hoot")
    void SetSteeringInput(float NormalizedInput);

    UFUNCTION(BlueprintCallable, Category = "SM64|Hoot")
    void CompleteTiredDialog();

    UFUNCTION(BlueprintCallable, Category = "SM64|Hoot")
    void ReleaseRider();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Hoot")
    void OnRequestHootDialog(int32 DialogId);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Hoot")
    void OnRequestAttachToHoot(AActor* RiderActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Hoot")
    void OnHootCarryTransform(AActor* RiderActor, FTransform HootTransform);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Hoot")
    void OnRequestReleaseFromHoot(AActor* RiderActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Hoot")
    void OnHootWindStarted();

protected:
    UFUNCTION()
    void OnWakeOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void EnterHootState(EWFHootState NewState);
    void MoveHoot(float SpeedPerFrame, bool bFastWingOscillation);
    void TurnTowardPoint(const FVector& Point, float MaxYawDegreesPerFrame, float MaxPitchDegreesPerFrame);
    void ApplyFloorAndWorldBounds(const FVector& PreviousLocation);
    float FindFloorHeightAt(const FVector& WorldLocation) const;
    void PollNativeSteeringInput();

    FVector SpawnLocation = FVector::ZeroVector;
    FRotator SpawnRotation = FRotator::ZeroRotator;
    FVector HomeLocation = FVector::ZeroVector;
    bool bHomeCaptured = false;
    bool bIntroDialogRequested = false;
    bool bTiredDialogRequested = false;
};
