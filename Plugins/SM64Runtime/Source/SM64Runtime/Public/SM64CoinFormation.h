#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SM64Collectible.h"
#include "SM64FixedStepActor.h"
#include "SM64CoinFormation.generated.h"

UENUM(BlueprintType)
enum class ESM64CoinFormationType : uint8
{
    HorizontalLine = 0,
    VerticalLine = 1,
    HorizontalRing = 2,
    VerticalRing = 3,
    Arrow = 4
};

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64CoinFormation : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    ASM64CoinFormation();

    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* SceneRoot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Coin Formation")
    ESM64CoinFormationType FormationType = ESM64CoinFormationType::HorizontalLine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Coin Formation")
    bool bFlying = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Coin Formation")
    TSubclassOf<ASM64Collectible> CoinClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Coin Formation")
    float SpawnDistance = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Coin Formation")
    float DespawnDistance = 2100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Coin Formation")
    TEnumAsByte<ECollisionChannel> FloorTraceChannel = ECC_WorldStatic;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Coin Formation")
    uint8 CollectedFlags = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Coin Formation")
    TArray<ASM64Collectible*> SpawnedCoins;

    UFUNCTION(BlueprintPure, Category = "SM64|Coin Formation")
    FVector GetCoinLocalOffset(int32 CoinIndex, bool& bOutSpawnCoin, bool& bOutOnGround) const;

    UFUNCTION(BlueprintNativeEvent, Category = "SM64|Coin Formation")
    ASM64Collectible* SpawnFormationCoin(int32 CoinIndex, FName CoinStableId, const FTransform& SpawnTransform);
    virtual ASM64Collectible* SpawnFormationCoin_Implementation(int32 CoinIndex, FName CoinStableId,
        const FTransform& SpawnTransform);

protected:
    void SpawnCoins();
    void DespawnCoinsAndRememberCollection();
    FVector ProjectCoinToFloor(const FVector& Position) const;
};
