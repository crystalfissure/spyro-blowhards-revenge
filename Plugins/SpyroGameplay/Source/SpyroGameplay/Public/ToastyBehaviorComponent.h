#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpyroEnemyEvents.h"
#include "ToastyBehaviorComponent.generated.h"
class ACharacter;
class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;
class UBoxComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EToastyState : uint8 { Guarded, Idle, Attack, HitReact, Retreat, Transforming, Defeating, Dead };
USTRUCT(BlueprintType)
struct FToastyGuard
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toasty") AActor* Dog = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toasty", meta=(ClampMin="0",ClampMax="2")) int32 Stage = 0;
};

/** NTSC Toasty class 314. Uses the existing enemy damage/dropper/checkpoint contracts. */
UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API UToastyBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UToastyBehaviorComponent();
    /** Optional feedback, dispatched after native updates; Detail is clip/stage/recovery/node. */
    UPROPERTY(BlueprintAssignable, Category="Spyro|Enemy Events") FSpyroEnemySignalEvent OnEnemySignal;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Toasty|Reference") TArray<UAnimSequence*> Animations;
    /** Costume, taunt, costume collapse, and sheep geometry, in that order. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Toasty|Reference") TArray<USkeletalMesh*> Meshes;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Toasty|Reference") TArray<USoundBase*> OriginalSounds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Toasty|Reference") USoundAttenuation* SoundAttenuation = nullptr;
    /** Explicit dogs in each encounter stage. Empty groups permit standalone combat. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Toasty|Encounter") TArray<FToastyGuard> Guards;
    /** Three local centimetre positions; the first normally stays zero. No production coordinates. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toasty|Encounter") TArray<FVector> StageLocations;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toasty|Encounter", meta=(ClampMin="100")) float GuardAreaRadius = 730.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toasty|Encounter", meta=(ClampMin="0")) float ApproachDistance = 440.f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") bool bEngaged = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Toasty|Reference") UStaticMesh* BurntCostumeMesh = nullptr;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") int32 Stage = 0;
    UFUNCTION(BlueprintPure, Category="Toasty|Encounter") bool HasLivingGuards() const;
    UFUNCTION(BlueprintPure, Category="Toasty|Encounter") FString ValidateEncounter() const;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Toasty|Target") AActor* Pursuer = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Toasty|Reference") float WorldUnitsPerOriginalUnit = .146104f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") EToastyState State = EToastyState::Guarded;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") int32 Health = 3;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") int32 CurrentClip = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") int32 CurrentFrame = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") int32 SimulationTicks = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") int32 GemsSpawned = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") int32 AcceptedPlayerHits = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") bool bDefeated = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") bool bBlocked = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Toasty|Debug") TArray<int32> SoundCueHistory;
    UFUNCTION(BlueprintPure, Category="Toasty") FVector GetHomeLocation() const { return Home.GetLocation(); }
    void GetPoseInputs(UAnimSequence*& A, UAnimSequence*& B, float& TimeA, float& TimeB, float& Alpha) const;
    void UpdateBodyCollision();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    FSpyroEnemyEventQueue PendingEnemyEvents;
    UFUNCTION() void OnAcceptedDamage();
    UFUNCTION() void OnDropperReset();
    UFUNCTION() void OnChargeSensorOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep, const FHitResult& Hit);
    void TakeMovementControl();
    void StepOriginal();
    void SelectClip(int32 Clip, bool Blend = true);
    bool AdvanceAnimation();
    void EmitFrameSound();
    void PlaySound(int32 Slot);
    void StopSounds();
    FVector StagePosition(int32 Index) const;
    void BeginRetreat();
    void LeaveCostume();
    bool Face(const FVector& Point, float TurnUnits, float ToleranceUnits);
    float Distance(const FVector& Point) const;
    FVector Feet(AActor* Actor) const;
    bool GroundAt(const FVector& Point, FVector& Ground) const;
    FVector SweepMove(const FVector& Delta, bool PlayerBlocks);
    bool GroundMove(const FVector& Delta);
    bool ProjectGroundMove(const FVector& Delta, FVector& Move) const;
    bool FindTerrainObstacle(const FVector& Start, const FVector& Delta, FHitResult& Obstacle) const;
    void RecoverTerrainPenetration();
    FVector FindGroundDirection(const FVector& Preferred, float Step, const FVector* Center = nullptr) const;
    void HitPlayer();
    void PublishDefeat();
    void FinishCorpse();
    void DropGemRange(int32 First, int32 Count);
    UPROPERTY(Transient) ACharacter* Character = nullptr;
    UPROPERTY(Transient) USkeletalMeshComponent* Mesh = nullptr;
    UPROPERTY(Transient) UActorComponent* Damageable = nullptr;
    UPROPERTY(Transient) UActorComponent* WalkingAI = nullptr;
    UPROPERTY(Transient) UActorComponent* Dropper = nullptr;
    UPROPERTY(Transient) UBoxComponent* BodyCollision = nullptr;
    UPROPERTY(Transient) UBoxComponent* ChargeSensor = nullptr;
    UPROPERTY(Transient) TArray<UAudioComponent*> PlayingSounds;
    UPROPERTY(Transient) UStaticMeshComponent* CostumeRemains = nullptr;
    FTransform Home;
    FVector MeshOffset, PreviousLocation, LastSafeGround;
    float Heading = 0, PreviousHeading = 0, Accumulator = 0, OrbitDirection = 1;
    FVector AvoidDirection = FVector::ZeroVector;
    int32 AvoidTicks = 0;
    int32 NextClip = 0, NextFrame = 1, Progress = 0, ProgressPerStep = 32;
    int32 Cooldown = 0, StateTicks = 0, ActiveMesh = -1;
    int32 TauntLoops = 2;
    FRandomStream Random;
    int32 ObservedGuardStage = INDEX_NONE;
    bool bObservedLivingGuards = false;
    bool bFirstTick = true, bHitThisAttack = false, bCorpseFinished = false;
    TSet<int32> ReleasedGemIndices;
    FDelegateHandle BoneTransformsHandle;
};
