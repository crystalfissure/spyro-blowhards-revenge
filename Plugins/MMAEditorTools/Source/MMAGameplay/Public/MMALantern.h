#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MMALantern.generated.h"
class UBoxComponent;
class USkeletalMeshComponent;
class UAnimSequence;

/** Non-destructible scenery using the project's Damageable_Com attack contract. */
UCLASS(Blueprintable)
class MMAGAMEPLAY_API AMMALantern : public AActor
{
    GENERATED_BODY()
public:
    AMMALantern();
    virtual void PostInitializeComponents() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lantern") USkeletalMeshComponent* LanternMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lantern") UBoxComponent* Hitbox;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lantern") UActorComponent* Damageable;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lantern") UAnimSequence* RestAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lantern") UAnimSequence* ReactionAnimation;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Lantern") bool bReacting=false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Lantern") bool bDamageContractReady=false;
    UFUNCTION(BlueprintCallable, Category="Lantern") void ReactToAttack();
private:
    float ReactionElapsed=0.f;
    void RestoreRestPose();
    void ConfigureDamageContract();
};
