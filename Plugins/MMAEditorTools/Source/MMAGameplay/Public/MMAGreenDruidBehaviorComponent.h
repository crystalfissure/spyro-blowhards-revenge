#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMAGreenDruidBehaviorComponent.generated.h"

class ACharacter;
class AMMAGreenDruidPlatform;
class APawn;
class UAnimSequence;
class UParticleSystem;
class UParticleSystemComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EMMAGreenDruidState : uint8
{
    Inactive UMETA(DisplayName = "Inactive / Flat Hold"),
    Raising UMETA(DisplayName = "Raising"),
    RaisedHold UMETA(DisplayName = "Raised Hold"),
    Lowering UMETA(DisplayName = "Lowering"),
    DeadReturningFlat UMETA(DisplayName = "Dead / Returning Flat")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FMMAGreenDruidStateChanged,
    EMMAGreenDruidState,
    PreviousState,
    EMMAGreenDruidState,
    NewState);

/** Stationary, non-attacking enemy behavior that cycles explicitly linked platforms. */
UCLASS(ClassGroup = (MMA), meta = (BlueprintSpawnableComponent))
class MMAGAMEPLAY_API UMMAGreenDruidBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMMAGreenDruidBehaviorComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMA|Green Druid")
    EMMAGreenDruidState CurrentState = EMMAGreenDruidState::Inactive;

    UPROPERTY(BlueprintAssignable, Category = "MMA|Green Druid")
    FMMAGreenDruidStateChanged OnStateChanged;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MMA|Green Druid|Platforms")
    TArray<AMMAGreenDruidPlatform*> ControlledPlatforms;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Awareness", meta = (ClampMin = "0.0"))
    float ActivationRadius = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Awareness", meta = (ClampMin = "0.0"))
    float DeactivationRadius = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Timing", meta = (ClampMin = "0.01"))
    float TransitionDuration = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Timing", meta = (ClampMin = "0.0"))
    float RaisedHoldDuration = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Timing", meta = (ClampMin = "0.0"))
    float FlatHoldDuration = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Animations")
    UAnimSequence* IdleAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Animations")
    UAnimSequence* RaiseAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Animations")
    UAnimSequence* LowerAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Animations")
    UAnimSequence* DeathAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Animations")
    bool bPlayLowerAnimationReversed = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|VFX")
    UParticleSystem* ChannelParticle = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Combat", meta = (ClampMin = "1.0"))
    float InitialHitPoints = 1.0f;

    /** Populates the inherited Drops_Items list only when it is empty. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Drop")
    TSubclassOf<AActor> DefaultDropClass;

    UFUNCTION(BlueprintCallable, Category = "MMA|Green Druid")
    void ForceAllPlatformsFlat();

    UFUNCTION(BlueprintPure, Category = "MMA|Green Druid")
    float GetLiftAlpha() const { return LiftAlpha; }

private:
    TWeakObjectPtr<ACharacter> CharacterOwner;
    TWeakObjectPtr<USkeletalMeshComponent> MeshComponent;
    TWeakObjectPtr<UParticleSystemComponent> ChannelParticleComponent;
    TArray<TWeakObjectPtr<AMMAGreenDruidPlatform>> ClaimedPlatforms;
    float LiftAlpha = 0.0f;
    float StateElapsedSeconds = 0.0f;
    bool bDeathObserved = false;
    bool bCycleEngaged = false;
    bool bFlatHoldPending = false;
    bool bMissingLinksWarningIssued = false;

    void EnterState(EMMAGreenDruidState NewState);
    void ApplyLiftAlpha();
    void PlayStateAnimation();
    void ConfigureNativeEnemyContract();
    void ConfigureIncomingDamageContract();
    void ConfigureDefaultDrop();
    bool IsOwnerDefeated() const;
    bool HasPlayerWithin(float Radius) const;
    static UObject* FindWalkingAIObject(AActor* Owner);
    static UActorComponent* FindDamageableComponent(AActor* Owner);
    static FString NormalizePropertyName(FString Name);
    static bool TryReadHitPoints(UObject* Object, double& OutValue);
    static bool TryWriteHitPoints(UObject* Object, double Value);
    static bool TryReadAIState(UObject* Object, uint8& OutState);
    static void SetInheritedBool(AActor* Owner, FName PropertyName, bool Value);
    static bool SetInheritedFloat(AActor* Owner, FName PropertyName, float Value);
};
