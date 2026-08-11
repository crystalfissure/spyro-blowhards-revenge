#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMAHedgeTrimmerBehaviorComponent.generated.h"

class ACharacter;
class APawn;
class UAnimSequence;
class UParticleSystem;
class UPrimitiveComponent;
class USkeletalMeshComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EMMAHedgeTrimmerState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Notice UMETA(DisplayName = "Alert / Notice"),
    Chase UMETA(DisplayName = "Chase"),
    Attack UMETA(DisplayName = "Attack"),
    ReturnHome UMETA(DisplayName = "Return Home"),
    Dead UMETA(DisplayName = "Dead")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FMMAHedgeTrimmerStateChanged,
    EMMAHedgeTrimmerState,
    PreviousState,
    EMMAHedgeTrimmerState,
    NewState);

/** Reusable close-melee state machine layered over the project's native enemy contract.
 *
 * The historical class name is retained so existing Hedge_Trimmer Blueprints keep
 * their serialized component. New assets expose it as "MMA Enemy State Machine".
 */
UCLASS(ClassGroup = (MMA), meta = (BlueprintSpawnableComponent))
class MMAGAMEPLAY_API UMMAHedgeTrimmerBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMMAHedgeTrimmerBehaviorComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMA|Hedge Trimmer")
    EMMAHedgeTrimmerState CurrentState = EMMAHedgeTrimmerState::Idle;

    UPROPERTY(BlueprintAssignable, Category = "MMA|Hedge Trimmer")
    FMMAHedgeTrimmerStateChanged OnStateChanged;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Animations")
    UAnimSequence* IdleAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Animations")
    UAnimSequence* NoticeAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Animations")
    UAnimSequence* ChaseAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Animations")
    UAnimSequence* AttackAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Animations")
    UAnimSequence* ReturnHomeAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Animations")
    UAnimSequence* DeathAnimation = nullptr;

    /** Optional one-frame pose displayed during the MMA-style shrink-away phase. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Animations")
    UAnimSequence* DeathTerminalAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Awareness", meta = (ClampMin = "0.0"))
    float DetectionRadius = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Awareness", meta = (ClampMin = "0.0"))
    float LoseInterestRadius = 950.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Awareness")
    bool bRequireLineOfSight = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Movement", meta = (ClampMin = "0.0"))
    float ChaseSpeed = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Movement", meta = (ClampMin = "0.0"))
    float ReturnHomeSpeed = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Movement", meta = (ClampMin = "0.0"))
    float MaximumDistanceFromHome = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Movement", meta = (ClampMin = "0.0"))
    float HomeAcceptanceRadius = 65.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Movement", meta = (ClampMin = "0.0"))
    float RotationSpeedDegrees = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat", meta = (ClampMin = "0.0"))
    float AttackRange = 135.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat", meta = (ClampMin = "0.0"))
    float AttackHitRange = 170.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float AttackHalfAngleDegrees = 70.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat", meta = (ClampMin = "0.0"))
    float AttackContactSeconds = 0.105f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat", meta = (ClampMin = "0.0"))
    float AttackCooldownSeconds = 1.0f;

    /** Retail MMA invariant for ordinary enemies; bosses use separate logic. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat", meta = (ClampMin = "1.0"))
    float InitialHitPoints = 1.0f;

    /** Use an explicit project Damage_Types enum value instead of the template Blueprint's value. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat")
    bool bOverrideOutgoingDamageType = false;

    /** Raw Damage_Types value. Spyro64's NORMAL_DAMAGE value is 1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat", meta = (ClampMin = "0", ClampMax = "255"))
    uint8 OutgoingDamageType = 1;

    /** Horizontal launch speed applied away from Hedge_Trimmer after a successful hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat|Recoil", meta = (ClampMin = "0.0"))
    float RecoilHorizontalSpeed = 260.0f;

    /** Small upward launch speed paired with the horizontal hit recoil. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat|Recoil", meta = (ClampMin = "0.0"))
    float RecoilVerticalSpeed = 140.0f;

    /** Populates the inherited Drops_Items component only when its list is empty. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Drop")
    TSubclassOf<AActor> DefaultDropClass;

    /** Extra time after the configured death animation before the inherited poof removes the mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death", meta = (ClampMin = "0.0"))
    float DeathPoofPaddingSeconds = 0.25f;

    /** Playback multiplier for the defeat animation; Hedge Trimmer uses 0.5 to match retail. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death", meta = (ClampMin = "0.01"))
    float DeathPlaybackRate = 1.0f;

    /** Duration of the one-frame terminal pose's transform-driven shrink-away. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death", meta = (ClampMin = "0.01"))
    float DeathTerminalDurationSeconds = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death", meta = (ClampMin = "0.0"))
    float DeathTerminalBackwardDistance = 170.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death", meta = (ClampMin = "0.0"))
    float DeathTerminalUpwardDistance = 45.0f;

    /** Multiplier applied to the mesh's starting relative scale at terminal completion. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeathTerminalEndScale = 0.02f;

    /** Native project particle used to punctuate the terminal disappearance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death")
    UParticleSystem* DeathPoofParticle = nullptr;

    /** Native project sound paired with the death poof particle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death")
    USoundBase* DeathPoofSound = nullptr;

    /** Temporary on-screen state and attack-contact diagnostics for playtesting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Debug")
    bool bEnableDebugMessages = true;

    UFUNCTION(BlueprintCallable, Category = "MMA|Hedge Trimmer")
    void ForceReturnHome();

private:
    TWeakObjectPtr<ACharacter> CharacterOwner;
    TWeakObjectPtr<USkeletalMeshComponent> MeshComponent;
    TWeakObjectPtr<APawn> TargetPawn;
    FVector HomeLocation = FVector::ZeroVector;
    FRotator HomeRotation = FRotator::ZeroRotator;
    float StateElapsedSeconds = 0.0f;
    float AttackCooldownRemaining = 0.0f;
    bool bAttackHitApplied = false;
    bool bAttackHitSucceeded = false;
    bool bDeathTerminalStarted = false;
    bool bDeathSequenceFinished = false;
    FVector DeathTerminalStartWorldLocation = FVector::ZeroVector;
    FVector DeathTerminalStartRelativeScale = FVector::OneVector;

    void EnterState(EMMAHedgeTrimmerState NewState);
    void PlayStateAnimation();
    void MaintainDeathAnimation(UAnimSequence* ExpectedAnimation);
    void TickDeathSequence(float DeltaTime);
    void StartDeathTerminalPhase();
    void SpawnDeathPoof() const;
    float GetStateDuration() const;
    float GetDeathAnimationDuration() const;
    bool IsOwnerDefeated() const;
    APawn* FindNearestPlayer(float Radius) const;
    bool HasValidEngagementTarget() const;
    void FaceLocation(const FVector& Location, float DeltaTime) const;
    void MoveTowardActor(APawn* Target, float DeltaTime) const;
    void MoveTowardHome(float DeltaTime) const;
    void StopMovement() const;
    void ApplyAttackHit();
    void ConfigureNativeEnemyContract();
    void ConfigureIncomingDamageContract();
    void ConfigureDefaultDrop();
    uint8 ReadInheritedAIState() const;
    bool SetInheritedAIState(uint8 Value) const;
    uint8 ReadNativeDamageType() const;
    bool DealNativeDamageToTarget(AActor* Target, bool& bOutDamageApplied) const;
    void ApplyHitRecoil(AActor* Target) const;
    static UActorComponent* FindDamageableComponent(AActor* Actor);
    static UPrimitiveComponent* FindDamageableHitbox(AActor* Actor);
    static void SetInheritedBool(AActor* Owner, FName PropertyName, bool Value);
    static bool SetInheritedFloat(AActor* Owner, FName PropertyName, float Value);
    void ShowDebugMessage(const FString& Message, const FColor& Color = FColor::Cyan) const;
};
