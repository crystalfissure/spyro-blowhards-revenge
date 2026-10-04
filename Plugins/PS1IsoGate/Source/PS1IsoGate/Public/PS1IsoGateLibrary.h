#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PS1IsoGateTypes.h"
#include "PS1IsoGateLibrary.generated.h"

UCLASS()
class PS1ISOGATE_API UPS1IsoGateLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Verifies the selected game's regional executable and required files using built-in rules. */
    UFUNCTION(BlueprintCallable, Category="PS1 ISO Gate")
    static FPS1IsoVerificationResult VerifyPS1DiscImage(EPS1IsoGame Game, const FString& DiscImagePath);

    UFUNCTION(BlueprintCallable, Category="PS1 ISO Gate")
    static bool ChoosePS1DiscImage(FString& SelectedDiscImagePath);

    /** Opens the disc image picker and verifies the selected game using built-in rules. */
    UFUNCTION(BlueprintCallable, Category="PS1 ISO Gate")
    static FPS1IsoVerificationResult ChooseAndVerifyConfiguredPS1DiscImage(EPS1IsoGame Game, FString& SelectedDiscImagePath);
};
