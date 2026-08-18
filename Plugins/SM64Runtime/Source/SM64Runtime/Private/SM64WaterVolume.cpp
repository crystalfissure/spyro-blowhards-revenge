#include "SM64WaterVolume.h"

#include "GameFramework/Pawn.h"

ASM64WaterVolume::ASM64WaterVolume()
{
    WaterTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterTrigger"));
    RootComponent = WaterTrigger;
    WaterTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WaterTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    WaterTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    WaterTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASM64WaterVolume::OnWaterBegin);
    WaterTrigger->OnComponentEndOverlap.AddDynamic(this, &ASM64WaterVolume::OnWaterEnd);

    SurfaceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SurfaceMesh"));
    SurfaceMesh->SetupAttachment(WaterTrigger);
    SurfaceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASM64WaterVolume::OnWaterBegin(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<APawn>())
    {
        OnActorEnteredWater.Broadcast(OtherActor);
    }
}

void ASM64WaterVolume::OnWaterEnd(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor->IsA<APawn>())
    {
        OnActorExitedWater.Broadcast(OtherActor);
    }
}
