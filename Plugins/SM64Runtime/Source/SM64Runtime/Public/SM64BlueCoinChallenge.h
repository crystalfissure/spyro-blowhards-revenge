#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "SM64Interactable.h"
#include "SM64BlueCoinChallenge.generated.h"

UENUM(BlueprintType)
enum class ESM64BlueCoinSwitchState : uint8
{
    Idle = 0,
    Receding = 1,
    Ticking = 2,
    Expired = 3
};

UENUM(BlueprintType)
enum class ESM64TimedBlueCoinState : uint8
{
    Waiting = 0,
    Active = 1,
    Collected = 2,
    Expired = 3
};

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64BlueCoinSwitch : public ASM64FixedStepActor, public ISM64Interactable
{
    GENERATED_BODY()

public:
    ASM64BlueCoinSwitch();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;
    virtual bool HandleSM64Attack_Implementation(ESM64AttackType AttackType, AActor* InstigatorActor,
        FVector ImpactPoint, FVector ImpactDirection) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* SwitchMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* ExactCollisionMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* DefaultCollisionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Blue Coin")
    FName ChallengeId = TEXT("WF_BlueCoins");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Blue Coin")
    ESM64AttackType RequiredAttack = ESM64AttackType::Headbash;

    UFUNCTION(BlueprintPure, Category = "SM64|Blue Coin")
    ESM64BlueCoinSwitchState GetSwitchState() const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Blue Coin")
    bool PressSwitch(AActor* InstigatorActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Blue Coin")
    void OnBlueCoinSwitchPressed(AActor* InstigatorActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Blue Coin")
    void OnBlueCoinSwitchTick(bool bFirst200Frames, int32 RemainingFrames);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Blue Coin")
    void OnBlueCoinSwitchExpired();

protected:
    bool HasPendingChallengeCoins() const;
    void ActivateChallengeCoins(AActor* InstigatorActor);

    FTransform InitialTransform;
    bool bTransformCaptured = false;
};

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64TimedBlueCoin : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    ASM64TimedBlueCoin();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* Trigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Blue Coin")
    FName ChallengeId = TEXT("WF_BlueCoins");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Blue Coin")
    int32 CoinValue = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Blue Coin")
    float SpinDegreesPerFrame = 6.0f;

    UFUNCTION(BlueprintPure, Category = "SM64|Blue Coin")
    ESM64TimedBlueCoinState GetCoinState() const;

    UFUNCTION(BlueprintPure, Category = "SM64|Blue Coin")
    bool IsPendingForSwitch() const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Blue Coin")
    void ActivateFromSwitch(AActor* SwitchActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Blue Coin")
    void OnTimedBlueCoinActivated();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Blue Coin")
    void OnTimedBlueCoinCollected(AActor* Collector);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Blue Coin")
    void OnTimedBlueCoinExpired();

protected:
    UFUNCTION()
    void OnCoinOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void SetCoinPresentation(bool bVisible, bool bCollectible);
};
