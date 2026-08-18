#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SM64Types.generated.h"

UENUM(BlueprintType)
enum class ESM64AttackType : uint8
{
    Charge UMETA(DisplayName = "Charge"),
    Flame UMETA(DisplayName = "Flame"),
    Headbash UMETA(DisplayName = "Headbash"),
    Cannon UMETA(DisplayName = "Cannon")
};

UENUM(BlueprintType)
enum class ESM64PlatformMotion : uint8
{
    None,
    Sliding,
    SmallBomp,
    LargeBomp,
    RotatingWood,
    RotatingContinuous,
    TowerSliding,
    TowerElevator,
    TumblingPiece
};

UENUM(BlueprintType)
enum class ESM64SurfaceType : uint8
{
    Default,
    Death,
    VerySlippery,
    Slippery,
    NotSlippery,
    WallMisc,
    NoiseDefault,
    BossCamera,
    CameraMiddle
};

USTRUCT(BlueprintType)
struct SM64RUNTIME_API FSM64MissionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Mission")
    int32 StarIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Mission")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Mission", meta = (ClampMin = "1", ClampMax = "6"))
    int32 PreferredAct = 1;
};

USTRUCT(BlueprintType)
struct SM64RUNTIME_API FSM64Placement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    FName StableId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    FName BehaviorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    FTransform SourceTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    FTransform UnrealTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement", meta = (Bitmask))
    int32 ActMask = 0x3F;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    TMap<FName, float> NumericParameters;
};

USTRUCT(BlueprintType)
struct SM64RUNTIME_API FSM64CourseProgress
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Progress")
    uint8 MissionStarMask = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Progress")
    bool bCollected100CoinStar = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Progress")
    bool bCannonUnlocked = false;

    bool HasMissionStar(int32 StarIndex) const
    {
        return StarIndex >= 0 && StarIndex < 6 && (MissionStarMask & (1 << StarIndex)) != 0;
    }

    void SetMissionStar(int32 StarIndex)
    {
        if (StarIndex >= 0 && StarIndex < 6)
        {
            MissionStarMask |= (1 << StarIndex);
        }
    }
};

UCLASS(BlueprintType)
class SM64RUNTIME_API USM64CourseDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FName CourseId = TEXT("WF");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FText CourseName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    TArray<FSM64MissionDefinition> Missions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    TArray<FSM64Placement> Placements;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FVector CourseStart = FVector(2600.0f, 5120.0f, 1256.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FRotator CourseStartRotation = FRotator(0.0f, -90.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FName ReturnLevel = TEXT("/Game/Spyro64/Levels/00_Homeworld");
};
