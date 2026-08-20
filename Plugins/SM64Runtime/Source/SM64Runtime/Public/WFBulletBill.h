#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "SM64Interactable.h"
#include "WFBulletBill.generated.h"

UENUM(BlueprintType)
enum class EWFBulletBillAction : uint8
{
    Reset = 0,
    WaitForPlayer = 1,
    Launch = 2,
    ResetAfterLaunch = 3,
    KnockedBack = 4
};

UCLASS(Blueprintable)
class SM64RUNTIME_API AWFBulletBill : public ASM64FixedStepActor, public ISM64Interactable
{
    GENERATED_BODY()

public:
    AWFBulletBill();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;
    virtual bool HandleSM64Attack_Implementation(ESM64AttackType AttackType, AActor* InstigatorActor,
        FVector ImpactPoint, FVector ImpactDirection) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* CollisionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bullet Bill")
    float ActivationMinDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bullet Bill")
    float ActivationMaxDistance = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bullet Bill")
    float ActivationHalfAngleDegrees = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bullet Bill")
    float CollisionRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Damage")
    int32 DamageAmount = 3;

    UFUNCTION(BlueprintPure, Category = "SM64|Bullet Bill")
    EWFBulletBillAction GetBulletBillAction() const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Bullet Bill")
    void KnockBack(AActor* InstigatorActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Bullet Bill")
    void OnCannonFired();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Bullet Bill")
    void OnSpawnSmokePuff();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Bullet Bill")
    void OnBulletBillImpact(AActor* HitActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Damage")
    void OnBulletBillDamagePlayer(AActor* PlayerActor, int32 CanonicalDamageAmount);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Bullet Bill")
    void OnBulletBillKnockedBack(AActor* InstigatorActor);

protected:
    UFUNCTION()
    void OnBillHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        FVector NormalImpulse, const FHitResult& Hit);

    UFUNCTION()
    void OnBillOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void TurnTowardPlayer(float MaxDegreesPerFrame);
    bool CanSeePlayer(const APawn* Player) const;

    FVector HomeLocation = FVector::ZeroVector;
    FRotator HomeRotation = FRotator::ZeroRotator;
    bool bHomeCaptured = false;
    bool bHitWallThisFrame = false;
};
