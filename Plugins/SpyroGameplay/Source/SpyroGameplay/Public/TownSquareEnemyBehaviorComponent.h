#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpyroEnemyEvents.h"
#include "TownSquareEnemyBehaviorComponent.generated.h"
class ACharacter;
class UAnimSequence;
class USkeletalMeshComponent;
class UBoxComponent;
class USphereComponent;
class USplineComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ETownSquareEnemyState : uint8 { Idle, Pursuit, Turn, React, Attack, Inverting, Stuck, Dying, Dead, Returning };

UENUM(BlueprintType)
enum class EBullMovementPattern : uint8
{
    FromPlacement UMETA(Hidden),
    BackAndForth,
    Loop,
    ChaseSpyro UMETA(Hidden)
};

/** Shared implementation exclusively for the Town Square enemies. No navigation or proximity pairing. */
UCLASS(Abstract, ClassGroup=(Spyro))
class SPYROGAMEPLAY_API UTownSquareEnemyBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTownSquareEnemyBehaviorComponent();
    /** Optional feedback, dispatched after native updates; Detail is clip/stage/recovery/node. */
    UPROPERTY(BlueprintAssignable, Category="Spyro|Enemy Events") FSpyroEnemySignalEvent OnEnemySignal;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Town Square|Reference") TArray<UAnimSequence*> Animations;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Town Square|Reference") TArray<USoundBase*> OriginalSounds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Town Square|Reference") USoundAttenuation* SoundAttenuation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Debug") bool bDrawMovementDebug = false;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Town Square|Target") AActor* Pursuer = nullptr;
    /** World conversion independently checked against both decoded original meshes. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Town Square|Reference") float WorldUnitsPerOriginalUnit = 0.146104f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Town Square|Reference") float MeshForwardYaw = -90.f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") ETownSquareEnemyState State = ETownSquareEnemyState::Idle;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") int32 CurrentClip = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") int32 CurrentFrame = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") int32 SimulationTicks = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") int32 CurrentRouteNode = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") int32 GemsSpawned = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") bool bDefeated = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") bool bPairConflict = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") float HeadingDegrees = 0.f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") float RouteFitScale = 1.f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") int32 BlockedTicks = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") int32 RecoveryCount = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") TArray<int32> SoundCueHistory;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") FVector LastStepDelta = FVector::ZeroVector;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") FVector RequestedStepDelta = FVector::ZeroVector;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") bool bFollowingRunPath = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug", meta=(Units="cm")) float RunPathDistance = 0.f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug", meta=(Units="cm")) float RunPathLength = 0.f;
    UFUNCTION(BlueprintPure, Category="Town Square|Territory") FVector GetRoamCenter() const;
    UFUNCTION(BlueprintPure, Category="Town Square|Pair") AActor* GetPartner() const;
    UFUNCTION(BlueprintPure, Category="Town Square|Placement") FString ValidatePlacement() const;
    void GetPoseInputs(UAnimSequence*& A, UAnimSequence*& B, float& TimeA, float& TimeB, float& Alpha) const;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
protected:
    virtual bool IsBull() const { return false; }
private:
    FSpyroEnemyEventQueue PendingEnemyEvents;
    UFUNCTION() void OnAcceptedDamage();
    UFUNCTION() void OnDropperReset();
    UFUNCTION() void OnChargeSensorOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep, const FHitResult& Hit);
    void BindContracts();
    void TakeMovementControl();
    void ReconcilePair();
    void SetPairMovementIgnored(UTownSquareEnemyBehaviorComponent* Other, bool bIgnore);
    void UpdateBullPatrol();
    void SetStandaloneBullPatrol();
    bool CanBullGore(float Cone) const;
    void StepOriginal();
    void StepBull(bool Complete);
    void StepToreador(bool Complete);
    void SelectClip(int32 Clip, bool Blend = true);
    bool AdvanceAnimation();
    void EmitFrameSound();
    void StopSounds();
    void Defeat(bool Flame);
    void FinishCorpse();
    void DropGemRange(int32 First, int32 Count);
    void HitPlayer(float Range, float Cone);
    bool Face(const FVector& Point, float TurnUnits, float WithinUnits = 256.f);
    void FollowRoute(float Speed, bool bBackAndForth = false);
    void CaptureRunPath(const USplineComponent* Source);
    void ResetRunPathProgress();
    void FollowRunPath(float Speed);
    void GroundMove(float Distance, float Direction);
    bool SweepPlayerBody(const FVector& Delta, FHitResult& Hit, bool& Initial) const;
    bool ProjectGroundMove(const FVector& Delta, FVector& GroundDelta);
    FVector ConstrainRoamMove(const FVector& Start, const FVector& Delta) const;
    float RoamCenterLimit() const;
    float CalculateRouteFit() const;
    FVector RoutePosition(int32 Index) const;
    FVector FloorPosition(AActor* Actor) const;
    float OriginalDistanceTo(const FVector& Point) const;
    void UpdateRoamPreview();
    void DrawMovementDebug() const;
    UTownSquareEnemyBehaviorComponent* TerritoryOwner() const;
    UPROPERTY(Transient) ACharacter* Character = nullptr;
    UPROPERTY(Transient) USkeletalMeshComponent* Mesh = nullptr;
    UPROPERTY(Transient) UActorComponent* Damageable = nullptr;
    UPROPERTY(Transient) UActorComponent* WalkingAI = nullptr;
    UPROPERTY(Transient) UActorComponent* Dropper = nullptr;
    UPROPERTY(Transient) UBoxComponent* BodyCollision = nullptr;
    UPROPERTY(Transient) UBoxComponent* ChargeSensor = nullptr;
    UPROPERTY(Transient) TArray<UAudioComponent*> PlayingSounds;
    UPROPERTY(Transient) UTownSquareEnemyBehaviorComponent* Partner = nullptr;
    // Unattached world-space snapshot: moving or destroying the Toreador cannot move the circuit.
    UPROPERTY(Transient) USplineComponent* RuntimeRunPath = nullptr;
    bool bJoiningRunPath = true;
    FTransform RouteOrigin;
    // A Bull keeps its patrol even if the actor that supplied it is destroyed.
    FTransform BullPatrolOrigin;
    bool bBullPatrolInitialized = false;
    UPROPERTY(Transient) UTownSquareEnemyBehaviorComponent* BullPatrolSource = nullptr;
    // Runtime route data is never an editable route on the Bull.
    TArray<FVector> ActiveRoutePoints;
    float ActiveRouteYaw = 0.f, ActiveRoamRadius = 1800.f;
    FVector MeshRelativeLocation = FVector::ZeroVector;
    FVector PreviousLocation = FVector::ZeroVector;
    float Accumulator = 0.f, PreviousHeading = 0.f, SlideDisplacement = 0.f, DeathVerticalSpeed = 0.f, DeathLift = 0.f;
    int32 NextClip = 0, NextFrame = 1, Progress = 0, ProgressPerStep = 32;
    int32 StateTicks = 0, Cooldown = 0, RouteDirection = 1, LastReachedRouteNode = 0;
    bool bFirstTick = true, bHitThisAttack = false, bCorpseFinished = false;
    FRandomStream Random;
    TSet<int32> ReleasedGemIndices;
    // Movement telemetry used by the collision solver and optional debug display.
    FVector LastPlayerContactNormal;
    FName LastPlayerContactComponent, LastFloorActor, LastFloorComponent;
    float LastFloorHeight = 0.f;
    bool bPlayerBlocked = false, bStartedOverlappingPlayer = false, bBoundaryClipped = false, bTerrainBlocked = false, bFloorRejected = false;
    bool bLimitRoaming = true;
#if WITH_EDITORONLY_DATA
    UPROPERTY(Transient) USphereComponent* RoamPreview = nullptr;
    UPROPERTY(Transient) USplineComponent* RoutePreview = nullptr;
#endif
};

UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API UBullBehaviorComponent : public UTownSquareEnemyBehaviorComponent
{
    GENERATED_BODY()
public:
    UBullBehaviorComponent();
    /** Total length of the standalone straight patrol, centred on placement and aligned with its forward arrow. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Patrol", meta=(ClampMin="200", Units="cm")) float PatrolDistance = 1200.f;
    /** Extra contact impact, played only when a horn hit actually damages Spyro. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Gore") USoundBase* GoreImpactSound = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Gore", meta=(ClampMin="0", ClampMax="2")) float GoreImpactVolume = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Gore", meta=(ClampMin=".5", ClampMax="2")) float GoreImpactPitch = 1.05f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") int32 GoreImpactCount = 0;
    // Retained only to load previously saved assets. The explicit pair now owns this decision.
    UPROPERTY() EBullMovementPattern MovementPattern = EBullMovementPattern::FromPlacement;
    /** An unpaired Bull patrols; a linked Toreador supplies its loop. */
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Town Square|Debug") EBullMovementPattern ActiveMovementPattern = EBullMovementPattern::BackAndForth;
protected:
    virtual bool IsBull() const override { return true; }
};

UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API UToreadorBehaviorComponent : public UTownSquareEnemyBehaviorComponent
{
    GENERATED_BODY()
public:
    UToreadorBehaviorComponent();
    // Names are preserved so existing Toreador waypoint placements deserialize unchanged.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Route", meta=(EditCondition="!bUseRunPath", EditConditionHides)) TArray<FVector> RoutePoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Route", meta=(Units="deg", EditCondition="!bUseRunPath", EditConditionHides)) float RouteYaw = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Territory", meta=(ClampMin="300", Units="cm", EditCondition="!bUseRunPath", EditConditionHides)) float RoamRadius = 1800.f;
    /** Edit the RunPath spline's points/tangents like an Egg Thief. A closed curve overrides the legacy waypoint route. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Route") bool bUseRunPath = false;
    UFUNCTION(BlueprintPure, Category="Town Square|Route") USplineComponent* GetRunPath() const;
    /** Supplies this circuit to one Bull. Without a live Bull, idle and slap nearby Spyro. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Town Square|Pair") AActor* LinkedBull = nullptr;
};
