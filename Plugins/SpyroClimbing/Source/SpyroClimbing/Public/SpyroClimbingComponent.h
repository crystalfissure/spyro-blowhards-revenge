#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "SpyroClimbSurface.h"
#include "SpyroClimbingComponent.generated.h"

class ACharacter;
class ASpyroClimbSurface;
class UCharacterMovementComponent;
class UDamageType;
class USkeletalMeshComponent;

UENUM(BlueprintType, meta = (DisplayName = "Spyro Climb Direction"))
enum class ESpyroClimbDirection : uint8
{
    Idle,
    Up,
    Down,
    Left,
    Right
};

UENUM(BlueprintType)
enum class ESpyroClimbExitReason : uint8
{
    Cancelled,
    Jumped,
    Damaged,
    SurfaceLost,
    Interrupted,
    Obstacle,
    DroppedOff UMETA(DisplayName = "Dropped Off")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpyroClimbStarted, ASpyroClimbSurface*, Surface);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpyroClimbEnded, ESpyroClimbExitReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpyroClimbTransfer, ASpyroClimbSurface*, TargetSurface);

/** Add to an existing ACharacter Blueprint; no reparenting or replacement movement component.
 * Single player only. Movement/input/animation Blueprint logic must respect IsClimbing().
 */
UCLASS(ClassGroup = (Spyro), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SPYROCLIMBING_API USpyroClimbingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USpyroClimbingComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Deactivate() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Attempts a spatial query and grabs the nearest valid panel. False leaves normal movement intact. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Climbing")
    bool TryStartClimbingNearby();

    UFUNCTION(BlueprintCallable, Category = "Spyro|Climbing")
    bool TryStartClimbing(ASpyroClimbSurface* Surface);

    /** Call every input frame. X = right, Y = up, values -1..1. Consumed and cleared each climbing tick.
     * Axis events can each add one axis: (MoveRight,0) and (0,MoveForward).
     * Only the stronger axis is used; equal magnitudes select vertical movement.
     */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Climbing")
    void AddClimbInput(FVector2D Input);

    /** Returns true if the jump was handled. Branch on IsClimbing BEFORE the normal Jump/glide logic. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Climbing")
    bool JumpOff();

    /** Use on Jump Pressed while IsClimbing. X = current MoveRight, Y = current MoveForward.
     * Stronger axis wins (vertical on ties). Analog strength scales reach. Neutral/no target jumps off.
     * With Drop Off On Down Jump enabled, down releases without a launch instead of transferring.
     * Repeated presses during a transfer are consumed. IsClimbing stays true throughout a transfer.
     */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Climbing")
    bool JumpOrTransfer(FVector2D Input);

    /** Explicit bridge for custom damage components. Call when damage is accepted, before knockback. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Climbing")
    void NotifyDamaged();

    UFUNCTION(BlueprintCallable, Category = "Spyro|Climbing")
    void CancelClimbing();

    /** Use for death, swimming, cutscenes, charge states etc. False also exits a current climb. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Climbing")
    void SetClimbingAllowed(bool bAllowed);

    UFUNCTION(BlueprintPure, Category = "Spyro|Climbing")
    bool IsClimbing() const { return bClimbing; }

    UFUNCTION(BlueprintPure, Category = "Spyro|Climbing")
    bool IsTransferring() const { return bTransferring; }

    /** True only during the longer, descending fallback transfer. Ready before OnTransferStarted. */
    UFUNCTION(BlueprintPure, Category = "Spyro|Climbing|Large Transfer")
    bool IsLargeTransfer() const { return bTransferring && bLargeTransfer; }

    UFUNCTION(BlueprintPure, Category = "Spyro|Climbing")
    ASpyroClimbSurface* GetTransferTarget() const { return TransferTarget.Get(); }

    UFUNCTION(BlueprintPure, Category = "Spyro|Climbing")
    ASpyroClimbSurface* GetCurrentSurface() const;

    UPROPERTY(BlueprintAssignable, Category = "Spyro|Climbing")
    FSpyroClimbStarted OnClimbStarted;

    UPROPERTY(BlueprintAssignable, Category = "Spyro|Climbing")
    FSpyroClimbEnded OnClimbEnded;

    UPROPERTY(BlueprintAssignable, Category = "Spyro|Climbing")
    FSpyroClimbTransfer OnTransferStarted;

    UPROPERTY(BlueprintAssignable, Category = "Spyro|Climbing")
    FSpyroClimbTransfer OnTransferLanded;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spyro|Climbing|Transfer")
    float TransferProgress = 0.f;

    /** Actual duration of the current/last transfer, in seconds. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spyro|Climbing|Transfer")
    float TransferDuration = 0.f;

    /** Reserved capsule centre at landing, valid while IsTransferring. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spyro|Climbing|Transfer")
    FVector TransferTargetLocation = FVector::ZeroVector;

    /** Actual velocity projected onto the current wall frame (cm/s). Mesh corrections can affect both axes.
     * Use ClimbDirection for animation transitions and ClimbAnimationSpeed for playback rate.
     */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spyro|Climbing")
    FVector2D ClimbVelocity = FVector2D::ZeroVector;

    /** Selected four-way input from the most recent tick; one axis is zero, the other is clamped to -1..1. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spyro|Climbing")
    FVector2D ClimbInput = FVector2D::ZeroVector;

    /** Stable wall-movement animation direction, chosen from the selected input axis and actual progress.
     * Idle when stopped/blocked, outside climbing or transferring. Use IsTransferring for the jump pose.
     */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spyro|Climbing|Animation")
    ESpyroClimbDirection ClimbDirection = ESpyroClimbDirection::Idle;

    /** Non-negative progress along the selected input direction in cm/s, excluding off-axis mesh correction.
     * Zero while Idle or transferring. Useful for animation playback rate, not state selection.
     */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spyro|Climbing|Animation")
    float ClimbAnimationSpeed = 0.f;

    /** Aligns the character mesh with the wall's tilt, preserving its authored offset. Capsule stays upright.
     * Blends between wall orientations during transfers and restores the mesh on exit.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Animation")
    bool bOrientMeshToSurface = true;

    /** Leave off until existing gameplay states are gated with SetClimbingAllowed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing")
    bool bAutoGrab = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spyro|Climbing")
    bool bClimbingAllowed = true;

    /** Up/down speed in cm/s. The serialized name is retained for existing Blueprint values and nodes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "0", DisplayName = "Vertical Climbing Speed"))
    float ClimbSpeed = 100.f;

    /** Left/right speed in cm/s, independent of vertical climbing speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "0", DisplayName = "Horizontal Climbing Speed"))
    float HorizontalClimbingSpeed = 100.f;

    /** Side-edge inset as a fraction of the scaled capsule radius: 0 = centre at edge,
     * 0.5 = halfway from centre to capsule edge, 1 = full capsule inside (original behaviour).
     * Applies to grabbing, climbing and transfer landings. Physical collision and vertical limits stay full-size.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
    float HorizontalEdgeMargin = .5f;

    /** Maximum gap between the front of the capsule and the panel when grabbing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "0"))
    float GrabDistance = 65.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "1"))
    float WallGap = 3.f;

    /** Dot with the direction toward the wall. 0.5 means a 60-degree approach cone. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "-1", ClampMax = "1"))
    float MinimumFacingDot = .5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "0"))
    float JumpAwaySpeed = 260.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "0"))
    float JumpUpSpeed = 220.f;

    /** All exits prevent immediate reattachment. Damage also starts this cooldown when not climbing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing", meta = (ClampMin = "0"))
    float RegrabDelay = .4f;

    /** Also listen to UE's Apply Damage events. Custom Damageable_Com needs NotifyDamaged. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing")
    bool bCancelOnAnyDamage = true;

    /** Maximum travel along the wall at full input. Partial stick input scales it directly. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "1", Units = "cm"))
    float HorizontalJumpDistance = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "1", Units = "cm"))
    float VerticalJumpDistance = 250.f;

    /** Down + Jump drops from rest instead of transferring down. Adds no launch or impulse.
     * Uses Jump Input Threshold and four-way direction selection. Ordinary down climbing is unchanged.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer")
    bool bDropOffOnDownJump = true;

    /** Smaller inputs count as neutral and jump off, preventing stick drift from selecting a transfer. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "0", ClampMax = "1"))
    float JumpInputThreshold = .15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "0.05", Units = "s"))
    float MinTransferDuration = .18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "0.05", Units = "s"))
    float MaxTransferDuration = .55f;

    /** Peak distance OUT from the wall at full reach; shorter hops use smaller arcs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "0", Units = "cm"))
    float TransferArcHeight = 65.f;

    /** Extra world-up lift at the midpoint of a full-reach left/right transfer. Shorter hops scale it down.
     * Added to the outward wall arc; 0 disables this lift. Up/down transfers are unaffected.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "0", Units = "cm"))
    float HorizontalTransferArcHeight = 50.f;

    /** Allows landing on a nearby panel recessed/protruding from the departure plane. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "0", Units = "cm"))
    float TransferDepthTolerance = 100.f;

    /** May shorten the requested jump by this much to find support within the edge margin (at most 25% of reach).
     * Never extends a jump past the distance requested by the stick. Zero requires an exact landing.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Transfer", meta = (ClampMin = "0", Units = "cm"))
    float LandingSearchTolerance = 40.f;

    /** After a normal left/right transfer fails, search farther across and lower down. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer")
    bool bEnableLargeTransfers = true;

    /** Full-input lateral reach. Must exceed Horizontal Jump Distance. Analog input scales reach/drop. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer", meta = (ClampMin = "1", Units = "cm"))
    float LargeTransferDistance = 600.f;

    /** Full-input world-height loss, measured down the source wall. Actual landing must be below takeoff. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer", meta = (ClampMin = "1", Units = "cm"))
    float LargeTransferHeightDrop = 150.f;

    /** Shortest large transfer; always at least 0.05 seconds longer than the normal maximum duration. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer", meta = (ClampMin = "0.05", Units = "s"))
    float LargeTransferMinDuration = .65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer", meta = (ClampMin = "0.05", Units = "s"))
    float LargeTransferMaxDuration = 1.05f;

    /** Full-reach outward arc, separate from the normal transfer's arc. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer", meta = (ClampMin = "0", Units = "cm"))
    float LargeTransferArcHeight = 85.f;

    /** Full-reach world-up arc above the descending line to the landing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer", meta = (ClampMin = "0", Units = "cm"))
    float LargeTransferUpwardArcHeight = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer", meta = (ClampMin = "0", Units = "cm"))
    float LargeTransferDepthTolerance = 100.f;

    /** Bounded shortening near edges, at most 25% of requested reach. Never falls back inside normal reach. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spyro|Climbing|Large Transfer", meta = (ClampMin = "0", Units = "cm"))
    float LargeTransferLandingSearchTolerance = 80.f;

private:
    // Reserved only while this component owns movement; normal movement ticking is suspended.
    static constexpr uint8 ClimbingCustomMode = 250;

    UFUNCTION()
    void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
        AController* InstigatedBy, AActor* DamageCauser);

    bool CanStart() const;
    bool OwnsMovementMode() const;
    float GetHorizontalSupportRadius() const;
    bool ValidateClimb();
    void UpdateClimbAnimation(float ProgressSpeed);
    bool UpdateMeshAlignment();
    void RestoreMeshAlignment();
    void TickMeshClimb(FVector2D DesiredVelocity, float DeltaTime);
    void TrackSurface(ASpyroClimbSurface* Surface);
    bool TryTransfer(FVector2D Input);
    bool TryTransferWithProfile(FVector2D Input, bool bLarge);
    float ConfigureTransferArc(float Travel, float FullTravel, bool bHorizontal, bool bLarge);
    void TickTransfer(float DeltaTime);
    FVector TransferPosition(float Alpha) const;
    bool TransferPathClear() const;
    bool PathClear(const FVector& From, const FVector& To) const;
    bool GetGrabTarget(ASpyroClimbSurface* Surface, FVector& OutTarget, FSpyroClimbFrame& OutFrame) const;
    void StopClimbing(ESpyroClimbExitReason Reason, bool bBroadcast = true);
    void StartRegrabDelay();

    UPROPERTY(Transient)
    ACharacter* Character = nullptr;

    UPROPERTY(Transient)
    UCharacterMovementComponent* Movement = nullptr;

    TWeakObjectPtr<ASpyroClimbSurface> CurrentSurface;
    FTransform SurfaceTransform;
    FSpyroClimbFrame ClimbFrame;
    TWeakObjectPtr<class UStaticMesh> ClimbingMesh;
    TWeakObjectPtr<USkeletalMeshComponent> ClimbingCharacterMesh;
    FTransform SavedMeshTransform;
    FTransform SavedMeshActorTransform;
    bool bMeshWasOriented = false;
    bool bClimbingMeshCollision = false;
    TWeakObjectPtr<ASpyroClimbSurface> TransferTarget;
    FSpyroClimbFrame TransferFrame;
    FVector TransferStart = FVector::ZeroVector;
    FVector TransferLastLocation = FVector::ZeroVector;
    float TransferElapsed = 0.f;
    float ActiveArcHeight = 0.f;
    float ActiveHorizontalArcHeight = 0.f;
    int32 TransferSteps = 32;
    bool bTransferring = false;
    bool bLargeTransfer = false;
    FVector2D PendingInput = FVector2D::ZeroVector;
    bool bClimbing = false;
    bool bTransitioning = false;
    bool bSavedMovementTick = false;
    bool bSavedControllerYaw = false;
    bool bSavedControllerPitch = false;
    bool bSavedControllerRoll = false;
    bool bSavedOrientToMovement = false;
    bool bSavedUseDesiredRotation = false;
    float NextGrabTime = 0.f;
    uint32 InterruptionSerial = 0;
};
