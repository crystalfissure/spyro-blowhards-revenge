#include "SM64Collectible.h"

#include "GameFramework/Pawn.h"
#include "SM64CourseManager.h"

ASM64Collectible::ASM64Collectible()
{
    PrimaryActorTick.bCanEverTick = true;
    Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
    RootComponent = Trigger;
    Trigger->SetSphereRadius(80.0f);
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Trigger);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ASM64Collectible::OnCollected);
}

void ASM64Collectible::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
}

void ASM64Collectible::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Mesh->AddLocalRotation(FRotator(0.0f, SpinDegreesPerSecond * DeltaSeconds, 0.0f));
}

void ASM64Collectible::OnCollected(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor || !OtherActor->IsA<APawn>())
    {
        return;
    }
    if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        if (bPowerStar)
        {
            Manager->CollectStar(StarIndex, b100CoinStar);
        }
        else
        {
            Manager->AddCoin(CoinValue, bRedCoin);
        }
    }
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    SetLifeSpan(0.05f);
}
