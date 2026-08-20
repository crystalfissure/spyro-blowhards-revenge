#include "SM64BreakableActor.h"

#include "Engine/World.h"
#include "SM64Collectible.h"
#include "SM64CourseManager.h"

ASM64BreakableActor::ASM64BreakableActor()
{
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(SceneRoot);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CollisionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollisionMesh"));
    CollisionMesh->SetupAttachment(SceneRoot);
    CollisionMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionMesh->SetVisibility(false, true);
    CollisionMesh->SetHiddenInGame(true, true);
}

void ASM64BreakableActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
    if (DefaultCollisionMesh)
    {
        CollisionMesh->SetStaticMesh(DefaultCollisionMesh);
    }
}

void ASM64BreakableActor::ResetForAct_Implementation()
{
    if (bDestroyActorOnBreak)
    {
        return;
    }
    bBroken = false;
    Mesh->SetVisibility(bActEnabled, true);
    CollisionMesh->SetCollisionEnabled(bActEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

bool ASM64BreakableActor::HandleSM64Attack_Implementation(
    ESM64AttackType AttackType,
    AActor* InstigatorActor,
    FVector ImpactPoint,
    FVector ImpactDirection)
{
    if (bBroken || AttackType != RequiredAttack)
    {
        return false;
    }
    BreakObject(InstigatorActor);
    return true;
}

void ASM64BreakableActor::BreakObject(AActor* InstigatorActor)
{
    if (bBroken)
    {
        return;
    }
    bBroken = true;
    CollisionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetVisibility(false, true);
    SpawnConfiguredDrops();
    OnBroken(InstigatorActor);
    if (bDestroyActorOnBreak)
    {
        SetLifeSpan(0.05f);
    }
}

void ASM64BreakableActor::SpawnConfiguredDrops()
{
    if (!GetWorld() || !DropCoinClass || DropCoinCount <= 0)
    {
        return;
    }
    for (int32 Index = 0; Index < DropCoinCount; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(DropCoinCount);
        const FVector Offset(FMath::Cos(Angle) * DropCoinRadius,
            FMath::Sin(Angle) * DropCoinRadius, 70.0f);
        ASM64Collectible* Coin = GetWorld()->SpawnActor<ASM64Collectible>(
            DropCoinClass, GetActorLocation() + Offset, FRotator::ZeroRotator);
        if (Coin)
        {
            Coin->StableId = FName(*(StableId.ToString() + FString::Printf(TEXT("_Drop_%02d"), Index)));
            Coin->CoinValue = 1;
            Coin->bDestroyOnActReset = true;
            Coin->ActMask = ActMask;
            if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
            {
                Coin->SetCurrentAct(Manager->CurrentAct);
            }
        }
    }
}
