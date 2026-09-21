#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SpyroEditorLibrary.generated.h"

class UAnimSequence;
class UBlueprint;
class USkeletalMesh;
class AActor;
class USoundBase;
class USoundAttenuation;

UCLASS()
class SPYROEDITOR_API USpyroEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureGnorcThiefCollisionAndAlert(UBlueprint* Blueprint, USoundAttenuation* AlertAttenuation);

    /** Changes only the thief's audio slots, preserving its existing mesh and behavior settings. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureGnorcThiefAudio(UBlueprint* Blueprint, const TArray<USoundBase*>& Sounds, USoundAttenuation* Attenuation);

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureGnorcThief(UBlueprint* Blueprint, const TArray<UAnimSequence*>& Animations, USkeletalMesh* FinalMesh);

    /** Isolated automation fixture. Refuses editor-world and non-thief actors. Never saves assets. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Tests")
    static bool PrepareGnorcThiefTest(AActor* Actor);
    /** Sets the Blueprint charge enum without Python's PlayerState/Player_State name collision. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Tests")
    static bool PrepareGnorcThiefChargeTest(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool CompileBlueprint(UBlueprint* Blueprint);
};
