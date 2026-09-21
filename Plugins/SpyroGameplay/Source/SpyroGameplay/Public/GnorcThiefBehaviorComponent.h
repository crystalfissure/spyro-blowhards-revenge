#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GnorcThiefBehaviorComponent.generated.h"
class ACharacter;
class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;
class UBoxComponent;
class UPrimitiveComponent;
class USphereComponent;
UENUM(BlueprintType)
enum class EGnorcThiefState : uint8 { Idle, Alert, Flee, HitRoll, FinalRoll, Dead };

/** SCUS-94228 class 339 behavior, retaining the project's damage/save/drop contracts. */
UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API UGnorcThiefBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UGnorcThiefBehaviorComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* IdleAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* AlertAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* RunAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* AlternateRunAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* HitAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* FinalAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") USkeletalMesh* FinalMesh = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") float MeshForwardYaw = -90.f;
    /** FBX vertices match decoded PS1 coordinates; model scale=1 means two world units per vertex unit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Route", meta=(ClampMin="0.001")) float WorldUnitsPerOriginalUnit = 0.146104f;
    /** Spawn-relative PS1 coordinates, rotated by the placed actor's yaw. Z is projected onto UE terrain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Route") TArray<FVector> RoutePoints;
    /** Optional explicit target; otherwise uses the player pawn. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Thief|Route") AActor* Pursuer = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Audio") TArray<USoundBase*> OriginalSounds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Audio") USoundAttenuation* SoundAttenuation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Audio") USoundAttenuation* AlertSoundAttenuation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Audio") float AlertVolumeMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Audio") float SoundVolume = 1.f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") EGnorcThiefState State = EGnorcThiefState::Idle;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") int32 RemainingHits = 3;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") int32 GemsSpawned = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") int32 CurrentRouteNode = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") int32 CurrentClip = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") int32 CurrentFrame = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") TArray<int32> SoundCueHistory;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") int32 SimulationTicks = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") float HeadingDegrees = 0.f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") float SlideDisplacement = 0.f;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") FVector LastStepDelta = FVector::ZeroVector;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") UAudioComponent* VoiceAudio = nullptr;
    UAnimSequence* AnimationForClip(int32 Clip) const;
    void GetPoseInputs(UAnimSequence*& A, UAnimSequence*& B, float& TimeA, float& TimeB, float& Alpha) const;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
private:
    UFUNCTION() void OnAcceptedDamage();
    UFUNCTION() void OnDropperReset();
    UFUNCTION() void OnChargeSensorOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    void BindContracts();
    void TakeMovementControl();
    void EnterState(EGnorcThiefState NewState);
    void SelectClip(int32 Clip, bool bBlend);
    bool AdvanceAnimation();
    void EmitFrameSound();
    void StopSounds();
    void StepOriginal();
    bool FollowRoute();
    void GroundMove(float OriginalDistance, float Direction);
    FVector FloorPosition(AActor* Actor) const;
    FVector RoutePosition(int32 Index) const;
    float RoamCenterLimit() const;
    FVector ConstrainRoamMove(const FVector& Start, const FVector& Delta) const;
    void UpdateRoamPreview();
    float OriginalDistanceTo(const FVector& Position) const;
    void FaceSpyro();
    void DropGemRange(int32 First, int32 Count);
    UPROPERTY(Transient) ACharacter* Character = nullptr;
    UPROPERTY(Transient) USkeletalMeshComponent* Mesh = nullptr;
    UPROPERTY(Transient) USkeletalMesh* MainMesh = nullptr;
    UPROPERTY(Transient) UActorComponent* Damageable = nullptr;
    UPROPERTY(Transient) UActorComponent* WalkingAI = nullptr;
    UPROPERTY(Transient) UActorComponent* Dropper = nullptr;
    UPROPERTY(Transient) UBoxComponent* BodyCollision = nullptr;
    UPROPERTY(Transient) UBoxComponent* ChargeSensor = nullptr;
    UPROPERTY(Transient) TArray<UAudioComponent*> PlayingSounds;
    FTransform RouteOrigin;
    float Accumulator = 0.f;
    float FinalSlideHeading = 0.f;
    int32 NextClip = 0, NextFrame = 1, Progress = 0, ProgressPerStep = 32;
    int32 RunPhase = 2;
    bool bFirstTick = true;
    TSet<int32> ReleasedGemIndices;
public:
    /** Horizontal outer boundary in centimetres, centered on the starting position. Applies to running and both hit rolls; does not change alert distance or speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Roaming", meta=(ClampMin="300.0", UIMin="300.0", Units="cm")) float RoamRadius = 1200.f;
    /** Enable the spawn-centered boundary and fit the original route inside it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Roaming") bool bLimitRoaming = true;
    UFUNCTION(BlueprintPure, Category="Thief|Roaming") FVector GetRoamCenter() const;
private:
#if WITH_EDITORONLY_DATA
    UPROPERTY(Transient) USphereComponent* RoamPreview = nullptr;
#endif
};
