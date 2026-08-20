#include "SM64SignActor.h"

#include "GameFramework/Pawn.h"

ASM64SignActor::ASM64SignActor()
{
    InteractionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractionCapsule"));
    RootComponent = InteractionCapsule;
    InteractionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionCapsule->OnComponentBeginOverlap.AddDynamic(this, &ASM64SignActor::OnSignRangeBegin);
    InteractionCapsule->OnComponentEndOverlap.AddDynamic(this, &ASM64SignActor::OnSignRangeEnd);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(InteractionCapsule);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ExactCollisionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExactCollisionMesh"));
    ExactCollisionMesh->SetupAttachment(InteractionCapsule);
    ExactCollisionMesh->SetVisibility(false, true);
    ExactCollisionMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ASM64SignActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    InteractionCapsule->SetCapsuleSize(InteractionRadius, InteractionHeight * 0.5f);
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
    ExactCollisionMesh->SetStaticMesh(DefaultCollisionMesh);
    ExactCollisionMesh->SetCollisionEnabled(DefaultCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Mesh->SetCollisionEnabled(!DefaultCollisionMesh && bAllowRenderCollisionFallback
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ASM64SignActor::ResetForAct_Implementation()
{
    bDialogueActive = false;
    ActiveReader = nullptr;
    ExactCollisionMesh->SetCollisionEnabled(bActEnabled && DefaultCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Mesh->SetCollisionEnabled(bActEnabled && !DefaultCollisionMesh && bAllowRenderCollisionFallback
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

bool ASM64SignActor::CanReadSign(AActor* Reader) const
{
    if (!Reader || !bActEnabled || bDialogueActive
        || FVector::Dist(Reader->GetActorLocation(), GetActorLocation()) > InteractionRadius)
    {
        return false;
    }
    if (!bRequireReaderFacingSign)
    {
        return true;
    }
    FVector ToSign = GetActorLocation() - Reader->GetActorLocation();
    ToSign.Z = 0.0f;
    return FVector::DotProduct(Reader->GetActorForwardVector().GetSafeNormal2D(), ToSign.GetSafeNormal())
        >= FMath::Cos(FMath::DegreesToRadians(FacingToleranceDegrees));
}

bool ASM64SignActor::RequestReadSign(AActor* Reader)
{
    if (!CanReadSign(Reader))
    {
        return false;
    }
    ActiveReader = Reader;
    bDialogueActive = true;
    OnRequestSignDialog(DialogId, Reader);
    return true;
}

void ASM64SignActor::CompleteSignDialog()
{
    if (!bDialogueActive)
    {
        return;
    }
    AActor* Reader = ActiveReader;
    bDialogueActive = false;
    ActiveReader = nullptr;
    OnSignDialogCompleted(DialogId, Reader);
}

void ASM64SignActor::OnSignRangeBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<APawn>())
    {
        OnReaderEnteredRange(OtherActor);
    }
}

void ASM64SignActor::OnSignRangeEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor->IsA<APawn>())
    {
        OnReaderExitedRange(OtherActor);
    }
}
