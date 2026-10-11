#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpyroEnemyEvents.h"
#include "SnowGnorcBehaviorComponent.generated.h"
class ACharacter;
class UAnimSequence;
class USkeletalMeshComponent;
class UBoxComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class UPrimitiveComponent;
class UParticleSystem;
class UParticleSystemComponent;

UENUM(BlueprintType)
enum class ESnowGnorcState : uint8 { Idle, Alert, Guarding, Punching, LoweringGuard, Dying, Recovering, Dead };

/** Ice Cavern's class 198, with the original NTSC animation/state clock. */
UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API USnowGnorcBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USnowGnorcBehaviorComponent();
    UPROPERTY(BlueprintAssignable, Category="Spyro|Enemy Events") FSpyroEnemySignalEvent OnEnemySignal;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snow Gnorc|Reference") TArray<UAnimSequence*> Animations;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snow Gnorc|Reference") TArray<USoundBase*> OriginalSounds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snow Gnorc|Reference") USoundAttenuation* SoundAttenuation = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snow Gnorc|Effects") UParticleSystem* DeathSmoke = nullptr;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Snow Gnorc|Target") AActor* Pursuer = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snow Gnorc|Reference") float WorldUnitsPerOriginalUnit = .146104f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") ESnowGnorcState State = ESnowGnorcState::Idle;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") int32 CurrentClip = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") int32 CurrentFrame = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") int32 SimulationTicks = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") int32 GemsSpawned = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") int32 AcceptedPlayerHits = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") int32 PunchCount = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") bool bDefeated = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Snow Gnorc|Debug") TArray<int32> SoundCueHistory;
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
    int32 IdleCycles = 0, RecoveryTicks = 0;
    bool bFirstTick = true, bHitThisAttack = false, bCorpseFinished = false;
    TSet<int32> ReleasedGemIndices;
    FDelegateHandle BoneTransformsHandle;
};
