#include "SM64CoinFormation.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ASM64CoinFormation::ASM64CoinFormation()
{
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
    PrimaryActorTick.bCanEverTick = true;
}

void ASM64CoinFormation::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    for (ASM64Collectible* Coin : SpawnedCoins)
    {
        if (Coin)
        {
            Coin->Destroy();
        }
    }
    SpawnedCoins.Reset();
    CollectedFlags = 0;
}

FVector ASM64CoinFormation::GetCoinLocalOffset(int32 CoinIndex, bool& bOutSpawnCoin, bool& bOutOnGround) const
{
    bOutSpawnCoin = CoinIndex >= 0 && CoinIndex <= 7;
    bOutOnGround = !bFlying;
    if (!bOutSpawnCoin)
    {
        return FVector::ZeroVector;
    }

    const float AngleRadians = FMath::DegreesToRadians(static_cast<float>(CoinIndex) * 45.0f);
    switch (FormationType)
    {
        case ESM64CoinFormationType::HorizontalLine:
            bOutSpawnCoin = CoinIndex <= 4;
            return FVector(0.0f, 160.0f * static_cast<float>(CoinIndex - 2), 0.0f);

        case ESM64CoinFormationType::VerticalLine:
            bOutSpawnCoin = CoinIndex <= 4;
            bOutOnGround = false;
            return FVector(0.0f, 0.0f, 128.0f * static_cast<float>(CoinIndex));

        case ESM64CoinFormationType::HorizontalRing:
            return FVector(FMath::Sin(AngleRadians) * 300.0f, FMath::Cos(AngleRadians) * 300.0f, 0.0f);

        case ESM64CoinFormationType::VerticalRing:
            bOutOnGround = false;
            return FVector(FMath::Cos(AngleRadians) * 200.0f, 0.0f,
                FMath::Sin(AngleRadians) * 200.0f + 200.0f);

        case ESM64CoinFormationType::Arrow:
        {
            static const FVector ArrowOffsets[8] = {
                FVector(0.0f, -150.0f, 0.0f), FVector(0.0f, -50.0f, 0.0f),
                FVector(0.0f, 50.0f, 0.0f), FVector(0.0f, 150.0f, 0.0f),
                FVector(-50.0f, 100.0f, 0.0f), FVector(-100.0f, 50.0f, 0.0f),
                FVector(50.0f, 100.0f, 0.0f), FVector(100.0f, 50.0f, 0.0f)
            };
            return ArrowOffsets[CoinIndex];
        }
    }
    return FVector::ZeroVector;
}

FVector ASM64CoinFormation::ProjectCoinToFloor(const FVector& Position) const
{
    if (!GetWorld())
    {
        return Position;
    }
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SM64CoinFormationFloor), false, this);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Position + FVector(0.0f, 0.0f, 300.0f),
        Position - FVector(0.0f, 0.0f, 2000.0f), FloorTraceChannel, Params))
    {
        return Hit.ImpactPoint;
    }
    return Position;
}

ASM64Collectible* ASM64CoinFormation::SpawnFormationCoin_Implementation(int32 CoinIndex,
    FName CoinStableId, const FTransform& SpawnTransform)
{
    if (!CoinClass || !GetWorld())
    {
        return nullptr;
    }
    FActorSpawnParameters Params;
    Params.Owner = this;
    ASM64Collectible* Coin = GetWorld()->SpawnActor<ASM64Collectible>(CoinClass, SpawnTransform, Params);
    if (Coin)
    {
        Coin->StableId = CoinStableId;
        Coin->ActMask = ActMask;
        Coin->CoinValue = 1;
    }
    return Coin;
}

void ASM64CoinFormation::SpawnCoins()
{
    SpawnedCoins.SetNumZeroed(8);
    for (int32 Index = 0; Index < 8; ++Index)
    {
        if ((CollectedFlags & (1 << Index)) != 0)
        {
            continue;
        }
        bool bSpawnCoin = false;
        bool bOnGround = false;
        const FVector LocalOffset = GetCoinLocalOffset(Index, bSpawnCoin, bOnGround);
        if (!bSpawnCoin)
        {
            continue;
        }
        FVector WorldPosition = GetActorLocation() + GetActorTransform().TransformVectorNoScale(LocalOffset);
        if (bOnGround)
        {
            WorldPosition = ProjectCoinToFloor(WorldPosition);
        }
        const FName CoinId(*FString::Printf(TEXT("%s_Coin_%d"), *StableId.ToString(), Index));
        SpawnedCoins[Index] = SpawnFormationCoin(Index, CoinId, FTransform(GetActorRotation(), WorldPosition));
    }
}

void ASM64CoinFormation::DespawnCoinsAndRememberCollection()
{
    for (int32 Index = 0; Index < SpawnedCoins.Num(); ++Index)
    {
        ASM64Collectible* Coin = SpawnedCoins[Index];
        if (!Coin)
        {
            continue;
        }
        if (Coin->IsHidden() || !Coin->GetActorEnableCollision())
        {
            CollectedFlags |= static_cast<uint8>(1 << Index);
        }
        Coin->Destroy();
    }
    SpawnedCoins.Reset();
}

void ASM64CoinFormation::SimulateSM64Frame_Implementation()
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player)
    {
        return;
    }
    const float Distance = FVector::Dist(Player->GetActorLocation(), GetActorLocation());
    if (SpawnedCoins.Num() == 0 && Distance < SpawnDistance)
    {
        SpawnCoins();
    }
    else if (SpawnedCoins.Num() > 0)
    {
        for (int32 Index = 0; Index < SpawnedCoins.Num(); ++Index)
        {
            const ASM64Collectible* Coin = SpawnedCoins[Index];
            if (Coin && (Coin->IsHidden() || !Coin->GetActorEnableCollision()))
            {
                CollectedFlags |= static_cast<uint8>(1 << Index);
            }
        }
        if (Distance > DespawnDistance)
        {
            DespawnCoinsAndRememberCollection();
        }
    }
}
