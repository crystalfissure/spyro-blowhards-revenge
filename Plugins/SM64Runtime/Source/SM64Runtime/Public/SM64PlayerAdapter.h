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

    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* AttackProbe;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Player")
    AActor* SpyroActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Player")
    float AttackRadius = 180.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Player")
    bool bCannonLaunched = false;

    /** Polls the established Spyro Blueprint state without modifying BP_Spyro. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Player")
    bool bAutoDetectSpyroAttacks = true;

    UFUNCTION(BlueprintCallable, Category = "SM64|Player")
    void BindToPlayer(AActor* NewSpyroActor);

    UFUNCTION(BlueprintCallable, Category = "SM64|Player")
    int32 DispatchAttack(ESM64AttackType AttackType, FVector Direction);

    UFUNCTION(BlueprintCallable, Category = "SM64|Player")
    void SetCannonLaunched(bool bNewCannonLaunched);

protected:
    virtual void BeginPlay() override;
    FString ReadSpyroStateToken() const;
    bool ReadSpyroBool(FName PropertyName) const;

    float AttackDispatchCooldown = 0.0f;
    int32 CannonAirFrames = 0;
};
