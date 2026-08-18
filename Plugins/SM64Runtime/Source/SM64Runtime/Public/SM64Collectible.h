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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    int32 StarIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collectible")
    float SpinDegreesPerSecond = 90.0f;

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
};
