#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SleepingDogBehaviorComponent.generated.h"
class ACharacter;
class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;
class UBoxComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ESleepingDogState : uint8 { Sleeping, Singed, Aiming, Pouncing, Landing, Returning, Dying, Dead };

/** NTSC Toasty class 335. Uses the existing enemy damage/dropper/checkpoint contracts. */
UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API USleepingDogBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USleepingDogBehaviorComponent();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sleeping Dog|Reference") TArray<UAnimSequence*> Animations;
    /** Base, scorched and first-flame reaction geometry, in that order. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sleeping Dog|Reference") TArray<USkeletalMesh*> Meshes;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sleeping Dog|Reference") TArray<USoundBase*> OriginalSounds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sleeping Dog|Reference") USoundAttenuation* SoundAttenuation = nullptr;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Sleeping Dog|Target") AActor* Pursuer = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sleeping Dog|Reference") float WorldUnitsPerOriginalUnit = .146104f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") ESleepingDogState State = ESleepingDogState::Sleeping;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") int32 Health = 2;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") int32 CurrentClip = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") int32 CurrentFrame = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") int32 SimulationTicks = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") int32 GemsSpawned = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") int32 AcceptedPlayerHits = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") int32 PounceCount = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") bool bDefeated = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") bool bBlocked = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") FVector PounceTarget = FVector::ZeroVector;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Sleeping Dog|Debug") TArray<int32> SoundCueHistory;
    UFUNCTION(BlueprintPure, Category="Sleeping Dog") FVector GetHomeLocation() const { return Home.GetLocation(); }
    void GetPoseInputs(UAnimSequence*& A, UAnimSequence*& B, float& TimeA, float& TimeB, float& Alpha) const;
    void UpdateBodyCollision();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
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
    void BeginPounce();
    bool Face(const FVector& Point, float TurnUnits, float ToleranceUnits);
    float Distance(const FVector& Point) const;
    FVector Feet(AActor* Actor) const;
    bool GroundAt(const FVector& Point, FVector& Ground) const;
    FVector SweepMove(const FVector& Delta, bool PlayerBlocks);
    bool GroundMove(const FVector& Delta);
    void HitPlayer(bool SweptContact = false);
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
    FTransform Home;
    FVector MeshOffset, PreviousLocation, LastSafeGround;
    float Heading = 0, PreviousHeading = 0, Accumulator = 0, VerticalSpeed = 0, DeathSpeed = 0;
    int32 NextClip = 0, NextFrame = 1, Progress = 0, ProgressPerStep = 32;
    int32 Cooldown = 70, StateTicks = 0, ActiveMesh = -1;
    bool bFirstTick = true, bHitThisAttack = false, bCorpseFinished = false, bSlidingOffPlayer = false;
    TSet<int32> ReleasedGemIndices;
    FDelegateHandle BoneTransformsHandle;
};
