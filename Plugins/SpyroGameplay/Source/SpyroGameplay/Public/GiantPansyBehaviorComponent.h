#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpyroEnemyEvents.h"
#include "GiantPansyBehaviorComponent.generated.h"
class ACharacter;
class UAnimSequence;
class USkeletalMeshComponent;
class USkeletalMesh;
class UBoxComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class UPrimitiveComponent;
class UParticleSystem;
class UParticleSystemComponent;

UENUM(BlueprintType)
enum class EGiantPansyState : uint8 { Idle, Attacking, RoamingIdle, RoamingAttack, Dying, Chasing, Revealing, Returning, Dead };

/** Jacques' class 46, with the original NTSC animation/state clock. */
UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API UGiantPansyBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UGiantPansyBehaviorComponent();
    UPROPERTY(BlueprintAssignable, Category="Spyro|Enemy Events") FSpyroEnemySignalEvent OnEnemySignal;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Giant Pansy|Reference") TArray<UAnimSequence*> Animations;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Giant Pansy|Reference") TArray<USoundBase*> OriginalSounds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Giant Pansy|Reference") USoundAttenuation* SoundAttenuation = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Giant Pansy|Effects") UParticleSystem* DeathSmoke = nullptr;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Giant Pansy|Variant") bool bRoaming = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Giant Pansy|Reference") TArray<USkeletalMesh*> Meshes;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Giant Pansy|Target") AActor* Pursuer = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Giant Pansy|Reference") float WorldUnitsPerOriginalUnit = .146104f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") EGiantPansyState State = EGiantPansyState::Idle;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") int32 CurrentClip = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") int32 CurrentFrame = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") int32 SimulationTicks = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") int32 GemsSpawned = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") int32 AcceptedPlayerHits = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") int32 PunchCount = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") bool bDefeated = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Giant Pansy|Debug") TArray<int32> SoundCueHistory;
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
    void Face(float TurnUnits, float DeadZoneUnits = 0);
    FVector Feet(AActor* Actor) const;
    float Distance() const;
    bool ComparableHeight() const;
    void HitPlayer();
    void Defeat();
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
    UPROPERTY(Transient) UParticleSystemComponent* DeathSmokeInstance = nullptr;
    FTransform Home;
    FVector MeshOffset, PreviousLocation, DeathDirection;
    float Heading = 0, PreviousHeading = 0, Accumulator = 0, DeathSpeed = 0;
    int32 NextClip = 0, NextFrame = 1, Progress = 0, ProgressPerStep = 16;
    int32 Cooldown = 0, AwayTicks = 0, HeadingTicks = 0;
    float ChaseHeading = 0, DeathVertical = 0;
    bool bChaseAttacked = false;
    EGiantPansyState PreviousState = EGiantPansyState::Idle;
    void Attack();
    bool Walk(float Speed, bool Returning);
    bool PlayerLooking(float Tolerance) const;
    bool bFirstTick = true, bHitThisAttack = false, bCorpseFinished = false;
    TSet<int32> ReleasedGemIndices;
    FDelegateHandle BoneTransformsHandle;
};
