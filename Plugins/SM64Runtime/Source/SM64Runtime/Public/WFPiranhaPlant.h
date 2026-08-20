#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "SM64Interactable.h"
#include "WFPiranhaPlant.generated.h"

class UAnimSequence;
class ASM64Collectible;

UENUM(BlueprintType)
enum class EWFPiranhaAction : uint8
{
    Idle = 0,
    Sleeping = 1,
    Biting = 2,
    WokenUp = 3,
    StoppedBiting = 4,
    Attacked = 5,
    ShrinkAndDie = 6,
    WaitToRespawn = 7,
    Respawn = 8
};

UCLASS(Blueprintable)
class SM64RUNTIME_API AWFPiranhaPlant : public ASM64FixedStepActor, public ISM64Interactable
{
    GENERATED_BODY()

public:
    AWFPiranhaPlant();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;
    virtual bool HandleSM64Attack_Implementation(ESM64AttackType AttackType, AActor* InstigatorActor,
        FVector ImpactPoint, FVector ImpactDirection) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* InteractionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USkeletalMeshComponent* CharacterMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    USkeletalMesh* DefaultSkeletalMesh = nullptr;

    /** Exact ten-entry piranha animation table in decomp index order. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Animation")
    TArray<UAnimSequence*> Animations;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Rewards")
    TSubclassOf<ASM64Collectible> BlueCoinClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Piranha")
    float ActivationDistance = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Piranha")
    float WakeDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Piranha")
    float StopBitingDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Piranha")
    float LullabyDistance = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Piranha")
    float FastPlayerSpeedPerFrame = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Piranha")
    float WFHideAboveHeight = 3400.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Piranha")
    float PlantScale = 1.0f;

    UFUNCTION(BlueprintPure, Category = "SM64|Piranha")
    EWFPiranhaAction GetPiranhaAction() const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Piranha")
    void NotifyCurrentAnimationComplete();

    UFUNCTION(BlueprintCallable, Category = "SM64|Piranha")
    void WakeWithoutDamage(AActor* InstigatorActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Piranha")
    void OnRequestPiranhaAnimation(EWFPiranhaAction NewAction);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Piranha")
    void OnLullabyStateChanged(bool bPlaying);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Piranha")
    void OnPiranhaBite(AActor* PlayerActor, int32 DamageAmount);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Piranha")
    void OnPiranhaAttacked(AActor* InstigatorActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Piranha")
    void OnSpawnBlueCoin(FName CoinStableId, int32 CoinValue);

protected:
    UFUNCTION()
    void OnPlantOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void EnterPiranhaAction(EWFPiranhaAction NewAction);
    void PlayNativeAnimation(EWFPiranhaAction NewAction);
    void SpawnBlueCoinDrop();
    bool IsPlayerMovingFast(const APawn* Player) const;
    void TurnTowardPlayer(float MaxDegreesPerFrame);

    bool bAnimationComplete = false;
    bool bLullabyPlaying = false;
    AActor* LastAttackInstigator = nullptr;
};
