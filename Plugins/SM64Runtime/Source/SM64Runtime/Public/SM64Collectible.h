#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64ActActor.h"
#include "SM64Collectible.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64Collectible : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64Collectible();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void SetCurrentAct(int32 NewAct) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* Trigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    int32 CoinValue = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    bool bRedCoin = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    bool bPowerStar = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    bool b100CoinStar = false;

    /** Awards an extra life instead of contributing to the course coin count. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    bool bOneUp = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    int32 StarIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    float SpinDegreesPerSecond = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    bool bRespawnOnActReset = true;

    /** Runtime enemy/crate loot is removed before its source actor resets. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    bool bDestroyOnActReset = false;

    UFUNCTION(BlueprintCallable, Category = "SM64|Collectible")
    void ResetCollectible();

protected:
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION()
    void OnCollected(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    bool bCollected = false;
};
