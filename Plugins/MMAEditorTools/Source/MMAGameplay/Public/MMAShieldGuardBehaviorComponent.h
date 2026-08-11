#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMAShieldGuardBehaviorComponent.generated.h"

class ACharacter;
class APawn;
class UAnimSequence;
class UPrimitiveComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EMMAShieldGuardState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Patrol UMETA(DisplayName = "Patrol"),
    EnGarde UMETA(DisplayName = "En Garde"),
    Attack UMETA(DisplayName = "Attack"),
    Dead UMETA(DisplayName = "Dead")
};
/** Idle-sentry archetype with intermittent patrols, based on the project's Gnorc Soldier contract. */
UCLASS(ClassGroup = (MMA), meta = (BlueprintSpawnableComponent))
class MMAGAMEPLAY_API UMMAShieldGuardBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMMAShieldGuardBehaviorComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMA|Shield Guard")
    EMMAShieldGuardState CurrentState = EMMAShieldGuardState::Idle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Animations")
    UAnimSequence* IdleAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Animations")
    UAnimSequence* PatrolAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Animations")
    UAnimSequence* EnGardeAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Animations")
    UAnimSequence* AttackAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Animations")
    UAnimSequence* DeathAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Idle", meta = (ClampMin = "0.0"))
    float IdleWaitMinimum = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Idle", meta = (ClampMin = "0.0"))
    float IdleWaitMaximum = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Patrol", meta = (ClampMin = "0.0"))
    float PatrolRadius = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Patrol", meta = (ClampMin = "0.0"))
    float PatrolSpeed = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Patrol", meta = (ClampMin = "0.0"))
    float PatrolAcceptanceRadius = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Patrol", meta = (ClampMin = "0.0"))
    float PatrolPauseMinimum = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Patrol", meta = (ClampMin = "0.0"))
    float PatrolPauseMaximum = 1.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Patrol", meta = (ClampMin = "0.1"))
    float PatrolTargetTimeout = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Awareness", meta = (ClampMin = "0.0"))
    float GuardRadius = 520.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Awareness", meta = (ClampMin = "0.0"))
    float LoseInterestRadius = 680.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Awareness")
    bool bRequireLineOfSight = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Movement", meta = (ClampMin = "0.0"))
    float RotationSpeedDegrees = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat", meta = (ClampMin = "0.0"))
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat", meta = (ClampMin = "0.0"))
    float AttackHitRange = 190.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float AttackHalfAngleDegrees = 75.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AttackContactFraction = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat", meta = (ClampMin = "0.0"))
    float AttackCooldownSeconds = 1.2f;

    /** Retail MMA invariant for ordinary enemies; bosses use separate logic. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat", meta = (ClampMin = "1.0"))
    float InitialHitPoints = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat|Recoil", meta = (ClampMin = "0.0"))
    float RecoilHorizontalSpeed = 230.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat|Recoil", meta = (ClampMin = "0.0"))
    float RecoilVerticalSpeed = 110.0f;

    /** The project Damage_Types value used by the guard's weapon; 1 is NORMAL_DAMAGE. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Combat", meta = (ClampMin = "0", ClampMax = "255"))
    uint8 OutgoingDamageType = 1;

    /** Adds the project's Burn entry to Damageable.Damage_Resistances. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Defense")
    bool bImmuneToFlame = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Defense|Charge Impact", meta = (ClampMin = "0.0"))
    float ChargeKnockbackHorizontalSpeed = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Defense|Charge Impact", meta = (ClampMin = "0.0"))
    float ChargeKnockbackVerticalSpeed = 160.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Defense|Charge Collision", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float ChargeCollisionRadiusScale = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Defense|Charge Collision", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float ChargeCollisionHalfHeightScale = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Drop")
    TSubclassOf<AActor> DefaultDropClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Death", meta = (ClampMin = "0.0"))
    float DeathPoofPaddingSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Shield Guard|Debug")
    bool bEnableDebugMessages = false;

private:
    TWeakObjectPtr<ACharacter> CharacterOwner;
    TWeakObjectPtr<USkeletalMeshComponent> MeshComponent;
    TWeakObjectPtr<APawn> TargetPawn;
    TWeakObjectPtr<UActorComponent> DamageableComponent;
    TWeakObjectPtr<UPrimitiveComponent> ShieldCollisionComponent;
    FVector HomeLocation = FVector::ZeroVector;
    FVector PatrolTarget = FVector::ZeroVector;
    FRandomStream PatrolRandom;
    float StateElapsedSeconds = 0.0f;
    float AttackCooldownRemaining = 0.0f;
    float IdleWaitRemaining = 0.0f;
    float PatrolPauseRemaining = 0.0f;
    float PatrolTargetElapsed = 0.0f;
    bool bAttackHitApplied = false;
    bool bPatrolReturningHome = false;
    bool bHasObservedHitPoints = false;
    double LastObservedHitPoints = 0.0;

    void ConfigureNativeEnemyContract();
    void ConfigureChargeCollision(UPrimitiveComponent* Primitive);
    void SetChargeCollisionEnabled(bool bEnabled) const;
    void ConfigureShieldDamageContract();
    void ConfigureDefaultDrop();
    void EnterState(EMMAShieldGuardState NewState);
    void PlayStateAnimation();
    float GetStateDuration() const;
    void BeginPatrolExcursion();
    void ChoosePatrolTarget();
    void MoveTowardPatrolTarget(float DeltaTime) const;
    void FaceLocation(const FVector& Location, float DeltaTime) const;
    void StopMovement() const;
    APawn* FindNearestPlayer(float Radius) const;
    bool HasValidTarget(float Radius) const;
    void ApplyAttackHit();
    void ObserveIncomingDamage();
    void ApplyChargeImpactKnockback() const;
    bool DealNativeDamageToTarget(AActor* Target, bool& bOutDamageApplied) const;
    void ApplyHitRecoil(AActor* Target) const;
    uint8 ReadInheritedAIState() const;
    bool SetInheritedAIState(uint8 Value) const;
    static UActorComponent* FindDamageableComponent(AActor* Actor);
    static void SetInheritedBool(AActor* Owner, FName PropertyName, bool Value);
    void ShowDebugMessage(const FString& Message, const FColor& Color = FColor::Cyan) const;
};
