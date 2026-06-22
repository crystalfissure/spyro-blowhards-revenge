#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PS1IsoGateSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, BlueprintType)
class PS1ISOGATE_API UPS1IsoGateSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="PS1 ISO Gate")
    FString DiscImagePath;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="PS1 ISO Gate")
    FString ExpectedSha256;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="PS1 ISO Gate")
    TArray<FString> AllowedExtensions = { TEXT("iso"), TEXT("bin"), TEXT("cue") };

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="PS1 ISO Gate")
    FString ExpectedBootExecutable = TEXT("SCUS_942.28");

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="PS1 ISO Gate")
    TArray<FString> RequiredDiscFiles =
    {
        TEXT("SYSTEM.CNF"),
        TEXT("SCUS_942.28"),
        TEXT("WAD.WAD"),
        TEXT("S0"),
        TEXT("SOURCE"),
        TEXT("PETEXA0.STR"),
        TEXT("PETEXA1.STR"),
        TEXT("PETEXA2.STR"),
        TEXT("PETEXA3.STR"),
        TEXT("PETEXA4.STR"),
        TEXT("PETEXA5.STR")
    };
};
