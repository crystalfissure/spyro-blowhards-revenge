#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChargeWobbleComponent.generated.h"

class UAnimSequence;
class UBoxComponent;
class USkeletalMeshComponent;

/** Plays a rest pose and a one-shot wobble when Spyro hits the prop.
 *  Damageable_Com is only the project's attack sensor. This component does
 *  not apply health, destruction, or loot; lanterns move when hit. */
UCLASS(ClassGroup = (Spyro), meta = (BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API UChargeWobbleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UChargeWobbleComponent();

    virtual void OnRegister() override;
    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge Wobble")
    UAnimSequence* RestAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge Wobble")
    UAnimSequence* ReactionAnimation = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Charge Wobble")
    bool bReacting = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Charge Wobble")
    bool bDamageContractReady = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Charge Wobble")
    UActorComponent* Damageable = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Charge Wobble")
    void ReactToAttack();

private:
    UPROPERTY(Transient)
    USkeletalMeshComponent* Mesh = nullptr;

    UPROPERTY(Transient)
    UBoxComponent* Hitbox = nullptr;

    UPROPERTY()
    TSubclassOf<UActorComponent> DamageableClass;

    float ReactionElapsed = 0.f;

    USkeletalMeshComponent* ResolveMesh() const;
    void RestoreRestPose();
    void EnsureHitbox();
    void EnsureDamageable();
    void ConfigureDamageContract();
    void ResistIncomingHits();
};
