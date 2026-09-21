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
    static bool CompileBlueprint(UBlueprint* Blueprint);
};
