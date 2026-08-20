#pragma once

#include "CoreMinimal.h"
#include "SM64Collectible.h"
#include "SM64StarMarker.generated.h"

UENUM(BlueprintType)
enum class ESM64StarMarkerGoal : uint8
{
    EightRedCoins,
    HundredCoins,
    External
};

/** Reveal/placement hook for red-coin and 100-coin bonus stars. */
UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64StarMarker : public ASM64Collectible
{
    GENERATED_BODY()

public:
    ASM64StarMarker();

    virtual void BeginPlay() override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star Marker")
    ESM64StarMarkerGoal Goal = ESM64StarMarkerGoal::EightRedCoins;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star Marker")
    bool bInitiallyRevealed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star Marker")
    bool bRevealAtPlayerLocation = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star Marker")
    FVector RevealLocationOffset = FVector::ZeroVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Star Marker")
    bool bRevealed = false;

    UFUNCTION(BlueprintCallable, Category = "SM64|Star Marker")
    void RevealStar();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Star Marker")
    void OnStarMarkerRevealed(FVector WorldLocation, bool bIs100CoinBonus);

protected:
    UFUNCTION()
    void OnRedCoinGoal();

    UFUNCTION()
    void OnHundredCoinGoal();

    bool IsAlreadyCollected() const;
};
