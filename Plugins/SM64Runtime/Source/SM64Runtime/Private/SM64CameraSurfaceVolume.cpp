#include "SM64CameraSurfaceVolume.h"

#include "GameFramework/Pawn.h"

ASM64CameraSurfaceVolume::ASM64CameraSurfaceVolume()
{
    CameraVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("CameraVolume"));
    RootComponent = CameraVolume;
    CameraVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CameraVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    CameraVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CameraVolume->OnComponentBeginOverlap.AddDynamic(this, &ASM64CameraSurfaceVolume::OnCameraVolumeBegin);
    CameraVolume->OnComponentEndOverlap.AddDynamic(this, &ASM64CameraSurfaceVolume::OnCameraVolumeEnd);
}

void ASM64CameraSurfaceVolume::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    CameraVolume->SetBoxExtent(BoxExtent);
}

void ASM64CameraSurfaceVolume::ResetForAct_Implementation()
{
    Occupants.Reset();
}

void ASM64CameraSurfaceVolume::OnCameraVolumeBegin(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<APawn>() && !Occupants.Contains(OtherActor))
    {
        Occupants.Add(OtherActor);
        OnCameraSurfaceEntered(OtherActor, CameraMode, CameraProfile, Priority, BlendTime);
    }
}

void ASM64CameraSurfaceVolume::OnCameraVolumeEnd(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    if (OtherActor && Occupants.Remove(OtherActor) > 0)
    {
        OnCameraSurfaceExited(OtherActor, CameraMode, CameraProfile, Priority, BlendTime);
    }
}
