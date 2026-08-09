#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMAHedgeTrimmerBehaviorComponent.generated.h"

class ACharacter;
class APawn;
class UAnimSequence;
class UPrimitiveComponent;
class USkeletalMeshComponent;

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

/** Bespoke Hedge_Trimmer state machine layered over the project's native enemy contract. */
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

    /** Horizontal launch speed applied away from Hedge_Trimmer after a successful hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat|Recoil", meta = (ClampMin = "0.0"))
    float RecoilHorizontalSpeed = 260.0f;

    /** Small upward launch speed paired with the horizontal hit recoil. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Combat|Recoil", meta = (ClampMin = "0.0"))
    float RecoilVerticalSpeed = 140.0f;

    /** Populates the inherited Drops_Items component only when its list is empty. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Drop")
    TSubclassOf<AActor> DefaultDropClass;

    /** Extra time after Slot 018 before the inherited death poof removes the mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Hedge Trimmer|Death", meta = (ClampMin = "0.0"))
    float DeathPoofPaddingSeconds = 0.25f;

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
    float StateElapsedSeconds = 0.0f;
    float AttackCooldownRemaining = 0.0f;
    bool bAttackHitApplied = false;

    void EnterState(EMMAHedgeTrimmerState NewState);
    void PlayStateAnimation();
    float GetStateDuration() const;
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
