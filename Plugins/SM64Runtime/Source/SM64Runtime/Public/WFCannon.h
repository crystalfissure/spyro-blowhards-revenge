#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "WFCannon.generated.h"

UENUM(BlueprintType)
enum class EWFCannonState : uint8
{
    Closed = 0,
    UnlockCamera = 1,
    OpeningLid = 2,
    OpenIdle = 3,
    Raising = 4,
    YawPresentation = 5,
    PitchPresentation = 6,
    Aiming = 7,
    PostLaunch = 8,
    ResetAfterLaunch = 9
};

/** Cannon trap door, base, barrel, unlock persistence, and launch hand-off. */
UCLASS(Blueprintable)
class SM64RUNTIME_API AWFCannon : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    AWFCannon();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* LidMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* LidCollisionMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* BaseMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* BarrelMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* InteractionSphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultLidMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* DefaultLidCollisionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    bool bAllowLidRenderCollisionFallback = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultBaseMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultBarrelMesh = nullptr;

    /** oBhvParams2ndByte << 8 converted to degrees by the placement bridge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Cannon")
    float InitialBarrelYawDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Cannon")
    float InteractionDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Cannon")
    float LaunchSpeedPerFrame = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Cannon")
    float LaunchMuzzleOffset = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Cannon")
    bool bStartUnlockedWithoutSave = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Cannon")
    bool bHideOpenCannonBeyondInteractionDistance = true;

    /** Native fallback: entering the open cannon does not require a course BP graph. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Cannon")
    bool bAutoEnterOnPawnOverlap = true;

    /** Reads keyboard/gamepad aim and launch input while the cannon owns Spyro. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Cannon")
    bool bReadDirectPlayerInput = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Cannon")
    AActor* LoadedRider = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Cannon")
    float AimPitchDegrees = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Cannon")
    float AimYawOffsetDegrees = 0.0f;

    UFUNCTION(BlueprintPure, Category = "SM64|Cannon")
    EWFCannonState GetCannonState() const;

    UFUNCTION(BlueprintPure, Category = "SM64|Cannon")
    bool IsCannonUnlocked() const;

    /** Called by the Bob-omb Buddy dialogue flow. */
    UFUNCTION(BlueprintCallable, Category = "SM64|Cannon")
    void UnlockCannon(AActor* UnlockingActor);

    UFUNCTION(BlueprintCallable, Category = "SM64|Cannon")
    bool RequestEnterCannon(AActor* RiderActor);

    /** Normalized axis input; source clamps pitch [0,0x38E3] and yaw +/-0x4000. */
    UFUNCTION(BlueprintCallable, Category = "SM64|Cannon")
    void AddAimInput(float NormalizedYawInput, float NormalizedPitchInput);

    UFUNCTION(BlueprintCallable, Category = "SM64|Cannon")
    bool LaunchLoadedRider();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Cannon")
    void OnCannonUnlockStarted(AActor* UnlockingActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Cannon")
    void OnCannonUnlockCameraFrame();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Cannon")
    void OnCannonOpened();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Cannon")
    void OnRequestAttachRider(AActor* RiderActor, FVector CannonSeatLocation);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Cannon")
    void OnCannonAimReady(AActor* RiderActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Cannon")
    void OnCannonAimChanged(float PitchDegrees, float WorldYawDegrees);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Cannon")
    void OnRequestLaunchRider(AActor* RiderActor, FVector LaunchLocation, FVector LaunchVelocity);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Cannon")
    void OnCannonPresentationSound(int32 SoundStage);

protected:
    UFUNCTION()
    void OnInteractionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void EnterCannonState(EWFCannonState NewState);
    void SetCannonVisible(bool bVisible);
    void MarkCannonUnlockedInProgress();
    void RestoreComponentTransforms();

    FTransform LidInitialRelativeTransform;
    FTransform BaseInitialRelativeTransform;
    FTransform BarrelInitialRelativeTransform;
    bool bTransformsCaptured = false;
    float PresentationPhaseDegrees = 0.0f;
};
