#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "WFThwomp.generated.h"

UENUM(BlueprintType)
enum class EWFThwompAction : uint8
{
    Raise = 0,
    IdleAtTop = 1,
    Lower = 2,
    Land = 3,
    IdleAtBottom = 4
};

/** Exact fixed-frame implementation shared by bhvThwomp and bhvThwomp2. */
UCLASS(Blueprintable)
class SM64RUNTIME_API AWFThwomp : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    AWFThwomp();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* Mesh;

    /** Invisible imported thwomp_seg5 collision; render geometry never collides. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* ExactCollisionMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* ThwompCollisionAsset = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* Thwomp2CollisionAsset = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    bool bAllowProxyCollisionFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Thwomp")
    bool bUseThwomp2Collision = false;

    /** oBhvParams2ndByte; WF passes zero to both instances. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Thwomp", meta = (ClampMin = "0", ClampMax = "255"))
    int32 RaiseDelayParameter = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Thwomp")
    FVector BoxExtent = FVector(120.0f, 60.0f, 140.0f);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Thwomp")
    float VerticalVelocityPerFrame = 0.0f;

    /** Thwomps use surface crushing rather than INTERACT_DAMAGE; zero preserves that semantic. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Damage")
    int32 ContactDamage = 0;

    UFUNCTION(BlueprintPure, Category = "SM64|Thwomp")
    EWFThwompAction GetThwompAction() const;

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Thwomp")
    void OnThwompLanded(bool bPlayerWithin1500Units);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Damage")
    void OnThwompPlayerContact(AActor* PlayerActor, int32 DamageAmount, bool bCrushingContact);

protected:
    UFUNCTION()
    void OnThwompHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

    FVector HomeLocation = FVector::ZeroVector;
    bool bHomeCaptured = false;
    int32 RandomWaitFrames = 0;
};
