#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MMAWeaponDamageNotify.generated.h"

/**
 * Processes the enemy Blueprint's existing Damage Check event for components
 * that are already touching the weapon hitbox at the animation contact frame.
 * This keeps the project's native Spyro damage, immunity and pain logic intact.
 */
UCLASS(meta = (DisplayName = "MMA Weapon Damage"))
class MMAGAMEPLAY_API UMMAWeaponDamageNotify : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
    virtual FString GetNotifyName_Implementation() const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Damage")
    FString WeaponComponentName = TEXT("Weapon Hitbox");

private:
    UPrimitiveComponent* FindWeaponHitbox(AActor* Owner) const;
    bool InvokeDamageCheck(AActor* Owner, UPrimitiveComponent* OtherComponent) const;
};
