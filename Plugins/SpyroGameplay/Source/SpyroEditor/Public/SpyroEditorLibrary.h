#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SpyroEditorLibrary.generated.h"

class UAnimSequence;
class UBlueprint;
class USkeletalMesh;
class AActor;

UCLASS()
class SPYROEDITOR_API USpyroEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureGnorcThief(UBlueprint* Blueprint, const TArray<UAnimSequence*>& Animations, USkeletalMesh* FinalMesh);

    /** Isolated automation fixture. Refuses editor-world and non-thief actors. Never saves assets. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Tests")
    static bool PrepareGnorcThiefTest(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool AddChargeWobbleComponent(
        UBlueprint* Blueprint,
        FName ComponentVariableName = TEXT("ChargeWobble"));

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureChargeWobble(
        UBlueprint* Blueprint,
        UAnimSequence* RestAnimation,
        UAnimSequence* ReactionAnimation);

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool CompileBlueprint(UBlueprint* Blueprint);

    /** Builds the project-native Blueprint base used by non-destructible
     * skeletal props that play a one-shot reaction when Damageable_Com
     * reports a resisted Ram or Burn attempt. The saved Blueprint contains
     * no SpyroGameplay runtime class. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureProjectHitReactionBase(UBlueprint* Blueprint);

    /** Reparents an existing skeletal prop to the project-native hit-reaction
     * base, removes ChargeWobbleComponent, and preserves its mesh/materials. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool MigrateLanternToProjectHitReactionBase(
        UBlueprint* Blueprint,
        UBlueprint* BaseBlueprint,
        UAnimSequence* RestAnimation,
        UAnimSequence* ReactionAnimation);
};
