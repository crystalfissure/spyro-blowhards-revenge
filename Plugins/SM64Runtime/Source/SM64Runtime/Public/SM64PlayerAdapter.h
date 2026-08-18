#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "SM64Types.h"
#include "SM64PlayerAdapter.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64PlayerAdapter : public AActor
{
    GENERATED_BODY()

public:
    ASM64PlayerAdapter();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* AttackProbe;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Player")
    AActor* SpyroActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Player")
    float AttackRadius = 180.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Player")
    bool bCannonLaunched = false;

    UFUNCTION(BlueprintCallable, Category = "SM64|Player")
    void BindToPlayer(AActor* NewSpyroActor);

    UFUNCTION(BlueprintCallable, Category = "SM64|Player")
    int32 DispatchAttack(ESM64AttackType AttackType, FVector Direction);

    UFUNCTION(BlueprintCallable, Category = "SM64|Player")
    void SetCannonLaunched(bool bNewCannonLaunched);

protected:
    virtual void BeginPlay() override;
};
