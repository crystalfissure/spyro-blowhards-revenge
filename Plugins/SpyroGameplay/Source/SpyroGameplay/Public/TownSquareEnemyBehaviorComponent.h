#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

/** Shared implementation exclusively for the Town Square enemies. No navigation or proximity pairing. */
UCLASS(Abstract, ClassGroup=(Spyro))
class SPYROGAMEPLAY_API UTownSquareEnemyBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTownSquareEnemyBehaviorComponent();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Town Square|Reference") TArray<UAnimSequence*> Animations;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Town Square|Reference") TArray<USoundBase*> OriginalSounds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Town Square|Reference") USoundAttenuation* SoundAttenuation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Route") TArray<FVector> RoutePoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Route", meta=(Units="deg")) float RouteYaw = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Town Square|Territory", meta=(ClampMin="300", Units="cm")) float RoamRadius = 1800.f;
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
    UFUNCTION() void OnAcceptedDamage();
    UFUNCTION() void OnDropperReset();
    UFUNCTION() void OnChargeSensorOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep, const FHitResult& Hit);
    void BindContracts();
    void TakeMovementControl();
    void ReconcilePair();
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
    void FollowRoute(float Speed);
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
    FTransform RouteOrigin;
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
protected:
    virtual bool IsBull() const override { return true; }
};

UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API UToreadorBehaviorComponent : public UTownSquareEnemyBehaviorComponent
{
    GENERATED_BODY()
public:
    /** Explicit one-to-one pairing. Empty means independent cape combat. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Town Square|Pair") AActor* LinkedBull = nullptr;
};
