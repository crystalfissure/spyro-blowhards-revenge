#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "SM64Interactable.h"
#include "WFWhomp.generated.h"

class UAnimSequence;
class ASM64Collectible;

UENUM(BlueprintType)
enum class EWFWhompAction : uint8
{
    Initialize = 0,
    Patrol = 1,
    BossChase = 2,
    PrepareJump = 3,
    Jump = 4,
    Land = 5,
    Downed = 6,
    Turn = 7,
    Die = 8,
    StopBossMusic = 9
};

/**
 * Actor-based state scaffold for bhvSmallWhomp and bhvWhompKingBoss.
 * Locomotion and frame transitions are native and deterministic; animation,
 * Spyro damage, dialogue, particles, and star presentation are Blueprint hooks.
 */
UCLASS(Blueprintable)
class SM64RUNTIME_API AWFWhomp : public ASM64FixedStepActor, public ISM64Interactable
{
    GENERATED_BODY()

public:
    AWFWhomp();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;
    virtual bool HandleSM64Attack_Implementation(ESM64AttackType AttackType, AActor* InstigatorActor,
        FVector ImpactPoint, FVector ImpactDirection) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* ExactCollisionMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USkeletalMeshComponent* CharacterMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    USkeletalMesh* DefaultSkeletalMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Animation")
    UAnimSequence* WalkAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Animation")
    UAnimSequence* PrepareJumpAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* DefaultCollisionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    bool bAllowProxyCollisionFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    bool bKingWhomp = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    FVector CollisionExtent = FVector(180.0f, 70.0f, 190.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    float SmallActivationDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    float BossActivationDistance = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    float PatrolDistance = 700.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    float NoticeDistance = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    float AttackDistance = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    int32 PrepareJumpFallbackFrames = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    int32 BossStarIndex = 0;

    /** Keeps the native course playable when no dialogue Blueprint is installed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    bool bAutoCompleteBossDialogs = true;

    /** Source (180,3880,340) mapped to UE (180,340,3880). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    FVector BossStarLocation = FVector(180.0f, 340.0f, 3880.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Whomp")
    TEnumAsByte<ECollisionChannel> FloorTraceChannel = ECC_WorldStatic;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Whomp")
    int32 Health = 3;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Whomp")
    float FacePitchDegrees = 0.0f;

    /** Whomp uses the SM64 squished state, not a direct health decrement. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Damage")
    int32 ContactDamage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Rewards")
    TSubclassOf<ASM64Collectible> DropCoinClass;

    UFUNCTION(BlueprintPure, Category = "SM64|Whomp")
    EWFWhompAction GetWhompAction() const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Whomp")
    void CompleteBossIntroDialog();

    UFUNCTION(BlueprintCallable, Category = "SM64|Whomp")
    void CompleteBossDefeatDialog();

    UFUNCTION(BlueprintCallable, Category = "SM64|Whomp")
    void NotifyPreparationAnimationComplete();

    UFUNCTION(BlueprintCallable, Category = "SM64|Whomp")
    void NotifyHeadbashOnBack(AActor* InstigatorActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnRequestWhompAnimation(EWFWhompAction NewAction, float PlayRate);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnRequestBossIntroDialog(int32 DialogId);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnRequestBossDefeatDialog(int32 DialogId);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnBossMusicRequested(bool bStartMusic);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnWhompBodySlam();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Damage")
    void OnWhompPlayerContact(AActor* PlayerActor, int32 DamageAmount, bool bSquishContact);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnWhompDamaged(int32 NewHealth, AActor* InstigatorActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnDropYellowCoins(int32 Count);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnSpawnBossStar(int32 StarIndex, FVector WorldLocation);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Whomp")
    void OnWhompDefeated(bool bWasKing);

protected:
    UFUNCTION()
    void OnWhompHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

    void EnterWhompAction(EWFWhompAction NewAction, float AnimationRate = 1.0f);
    void MoveForwardPerFrame(float Distance);
    void TurnTowardPlayer(float MaxDegreesPerFrame);
    bool PlayerWithinForwardCone(float Distance, float HalfAngleDegrees) const;
    bool ApplyVerticalMovementAndCheckFloor();
    void StartRecovery();
    void SpawnYellowCoinDrops(int32 Count);

    FVector HomeLocation = FVector::ZeroVector;
    FRotator HomeRotation = FRotator::ZeroRotator;
    bool bHomeCaptured = false;
    bool bBossIntroRequested = false;
    bool bBossDefeatDialogRequested = false;
    bool bPreparationComplete = false;
    bool bLanded = false;
    bool bRecovering = false;
    bool bPendingHeadbash = false;
    AActor* PendingAttackInstigator = nullptr;
    int32 ShakeFrame = 0;
    int32 LandedFrames = 0;
    float VerticalVelocityPerFrame = 0.0f;
    float PitchVelocityDegreesPerFrame = 0.0f;
};
