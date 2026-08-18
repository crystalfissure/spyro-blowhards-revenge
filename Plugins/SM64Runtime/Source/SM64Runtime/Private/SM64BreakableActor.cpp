#include "SM64BreakableActor.h"

ASM64BreakableActor::ASM64BreakableActor()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ASM64BreakableActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
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
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetVisibility(false, true);
    OnBroken(InstigatorActor);
    if (bDestroyActorOnBreak)
    {
        SetLifeSpan(0.05f);
    }
}
