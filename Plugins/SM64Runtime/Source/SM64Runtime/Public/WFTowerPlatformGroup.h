#pragma once

#include "CoreMinimal.h"
#include "SM64ActActor.h"
#include "SM64MovingPlatformBase.h"
#include "WFTowerPlatformGroup.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API AWFTowerPlatformGroup : public ASM64ActActor
{
    GENERATED_BODY()

public:
    AWFTowerPlatformGroup();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetCurrentAct(int32 NewAct) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Tower")
    UStaticMesh* PlatformMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Tower")
    UStaticMesh* ElevatorMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Tower")
    UStaticMesh* PlatformCollisionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Tower")
    TSubclassOf<ASM64MovingPlatformBase> PlatformClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Tower")
    float Radius = 704.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Tower")
    float PlatformHeightStep = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Tower")
    float ActivationHeightBelowRoot = 700.0f;

    UFUNCTION(BlueprintCallable, Category = "SM64|Tower")
    void SpawnPlatforms();

    UFUNCTION(BlueprintCallable, Category = "SM64|Tower")
    void DestroyPlatforms();

protected:
    TArray<TWeakObjectPtr<ASM64MovingPlatformBase>> Platforms;
    bool bSpawned = false;
};
