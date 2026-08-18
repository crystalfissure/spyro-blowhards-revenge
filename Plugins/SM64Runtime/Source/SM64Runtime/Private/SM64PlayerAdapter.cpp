#include "SM64PlayerAdapter.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SM64Interactable.h"

ASM64PlayerAdapter::ASM64PlayerAdapter()
{
    PrimaryActorTick.bCanEverTick = false;
    AttackProbe = CreateDefaultSubobject<USphereComponent>(TEXT("AttackProbe"));
    RootComponent = AttackProbe;
    AttackProbe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttackProbe->SetSphereRadius(AttackRadius);
}

void ASM64PlayerAdapter::BeginPlay()
{
    Super::BeginPlay();
    if (!SpyroActor)
    {
        BindToPlayer(UGameplayStatics::GetPlayerPawn(this, 0));
    }
}

void ASM64PlayerAdapter::BindToPlayer(AActor* NewSpyroActor)
{
    SpyroActor = NewSpyroActor;
    if (SpyroActor)
    {
        AttachToActor(SpyroActor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
}

int32 ASM64PlayerAdapter::DispatchAttack(ESM64AttackType AttackType, FVector Direction)
{
    if (!SpyroActor)
    {
        return 0;
    }
    TArray<FOverlapResult> Results;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SM64AttackProbe), false, SpyroActor);
    const FVector Center = SpyroActor->GetActorLocation() + Direction.GetSafeNormal() * AttackRadius * 0.5f;
    GetWorld()->OverlapMultiByObjectType(
        Results,
        Center,
        FQuat::Identity,
        Objects,
        FCollisionShape::MakeSphere(AttackRadius),
        Params);

    int32 Consumed = 0;
    TSet<AActor*> Seen;
    for (const FOverlapResult& Result : Results)
    {
        AActor* Target = Result.GetActor();
        if (!Target || Seen.Contains(Target) || !Target->GetClass()->ImplementsInterface(USM64Interactable::StaticClass()))
        {
            continue;
        }
        Seen.Add(Target);
        if (ISM64Interactable::Execute_HandleSM64Attack(
            Target,
            AttackType,
            SpyroActor,
            Result.Component.IsValid() ? Result.Component->GetComponentLocation() : Center,
            Direction.GetSafeNormal()))
        {
            ++Consumed;
        }
    }
    return Consumed;
}

void ASM64PlayerAdapter::SetCannonLaunched(bool bNewCannonLaunched)
{
    bCannonLaunched = bNewCannonLaunched;
}
