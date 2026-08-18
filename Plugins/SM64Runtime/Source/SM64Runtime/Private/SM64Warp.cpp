#include "SM64Warp.h"

#include "GameFramework/Pawn.h"

ASM64Warp::ASM64Warp()
{
    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    RootComponent = Trigger;
    Trigger->SetBoxExtent(FVector(100.0f));
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ASM64Warp::OnWarpOverlap);
}

void ASM64Warp::OnWarpOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!Destination || !OtherActor || !OtherActor->IsA<APawn>())
    {
        return;
    }
    const float Now = GetWorld()->GetTimeSeconds();
    if (const float* LastTime = LastWarpTimes.Find(OtherActor))
    {
        if (Now - *LastTime < ReentryDelay)
        {
            return;
        }
    }
    LastWarpTimes.Add(OtherActor, Now);
    Destination->LastWarpTimes.Add(OtherActor, Now);
    OtherActor->SetActorLocationAndRotation(
        Destination->GetActorLocation(),
        Destination->GetActorRotation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
}
