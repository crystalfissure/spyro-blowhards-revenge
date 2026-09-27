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
    UFUNCTION(BlueprintCallable, Category="Spyro|Tests")
    static bool MountTownSquareTestContent(const FString& Directory);
    UFUNCTION(BlueprintCallable, Category="Spyro|Tests")
    static bool PrepareTownSquareTest(AActor* Actor);
    UFUNCTION(BlueprintCallable, Category="Spyro|Tests")
    static bool PrepareTownSquareChargeTest(AActor* Actor);
    UFUNCTION(BlueprintCallable, Category="Spyro|Tests")
    static FString DescribeTownSquareDamageTypes();
    /** Configure only a new Town Square child Blueprint. */
    UFUNCTION(BlueprintCallable, Category="Spyro|Blueprint")
    static bool ConfigureTownSquareEnemy(UBlueprint* Blueprint, bool Bull, USkeletalMesh* Mesh, const TArray<UAnimSequence*>& Animations, const TArray<USoundBase*>& Sounds, USoundAttenuation* Attenuation);
    /** Adds an editable, closed RunPath without altering the enemy's existing settings or materials. */
    UFUNCTION(BlueprintCallable, Category="Spyro|Blueprint")
    static bool AddToreadorRunPath(UBlueprint* Blueprint);

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureGnorcThiefCollisionAndAlert(UBlueprint* Blueprint, USoundAttenuation* AlertAttenuation);

    /** Changes only the thief's audio slots, preserving its existing mesh and behavior settings. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureGnorcThiefAudio(UBlueprint* Blueprint, const TArray<USoundBase*>& Sounds, USoundAttenuation* Attenuation);

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureGnorcThief(UBlueprint* Blueprint, const TArray<UAnimSequence*>& Animations, USkeletalMesh* FinalMesh);

    /** Isolated PIE fixture: prototype map or an explicitly tagged automation thief. Never saves assets. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Tests")
    static bool PrepareGnorcThiefTest(AActor* Actor);
    /** Sets the Blueprint charge enum without Python's PlayerState/Player_State name collision. */
    UFUNCTION(BlueprintCallable, Category = "Spyro|Tests")
    static bool PrepareGnorcThiefChargeTest(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool CompileBlueprint(UBlueprint* Blueprint);
};
