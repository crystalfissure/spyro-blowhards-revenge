#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SM64Types.generated.h"

class AActor;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Mission", meta = (Bitmask))
    int32 VisibleActMask = 0x3F;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Mission")
    FVector StarLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Mission")
    bool bEjectPlayerOnCollect = true;
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
    TSoftObjectPtr<UObject> Asset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    TSoftClassPtr<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    FTransform SourceTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    FTransform UnrealTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement", meta = (Bitmask))
    int32 ActMask = 0x3F;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    TMap<FName, float> NumericParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    FName CollisionSource;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Placement")
    FName Category;
};

USTRUCT(BlueprintType)
struct SM64RUNTIME_API FSM64WarpDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Warp")
    FName WarpId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Warp")
    FTransform Transform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Warp")
    FName DestinationWarpId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Warp")
    FName DestinationLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Warp", meta = (Bitmask))
    int32 ActMask = 0x3F;
};

USTRUCT(BlueprintType)
struct SM64RUNTIME_API FSM64SurfaceMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Surface")
    ESM64SurfaceType Surface = ESM64SurfaceType::Default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Surface")
    FName PhysicalMaterialId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Surface")
    bool bHazard = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Surface")
    bool bCameraSurface = false;
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Progress")
    bool bCourseComplete = false;

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
    int32 CourseIndex = 5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FName CourseId = TEXT("WF");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FName WarpKey = TEXT("WF");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FText CourseName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    TArray<FSM64MissionDefinition> Missions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    TArray<FSM64Placement> Placements;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    TArray<FSM64WarpDefinition> Warps;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    TArray<FSM64SurfaceMapping> SurfaceMappings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    int32 CanonicalCourseCoinTotal = 100;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    float DeathPlaneHeight = -3071.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    float WaterSurfaceHeight = 973.0f;

    /** Canonical course-floor start. Player capsule height is applied at spawn time. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FVector CourseStart = FVector(2600.0f, 5120.0f, 256.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FRotator CourseStartRotation = FRotator(0.0f, -90.0f, 0.0f);

    /** Node 0A: the source spin-airborne entrance, kept distinct from retries. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FVector EntranceWarpLocation = FVector(2600.0f, 5120.0f, 1256.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FRotator EntranceWarpRotation = FRotator(0.0f, -90.0f, 0.0f);

    /** Enable only when the player adapter supplies the spin-airborne intro state. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    bool bUseSpinAirborneEntrance = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SM64|Course")
    FName ReturnLevel = TEXT("/Game/Spyro64/Levels/00_Homeworld");
};
