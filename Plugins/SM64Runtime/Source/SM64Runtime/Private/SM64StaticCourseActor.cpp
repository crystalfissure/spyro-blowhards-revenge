#include "SM64StaticCourseActor.h"

ASM64StaticCourseActor::ASM64StaticCourseActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
    SceneRoot->SetMobility(EComponentMobility::Static);

    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(SceneRoot);
    VisualMesh->SetMobility(EComponentMobility::Static);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetGenerateOverlapEvents(false);

    CollisionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollisionMesh"));
    CollisionMesh->SetupAttachment(SceneRoot);
    CollisionMesh->SetMobility(EComponentMobility::Static);
    CollisionMesh->SetCollisionProfileName(TEXT("BlockAll"));
    CollisionMesh->SetGenerateOverlapEvents(false);
    CollisionMesh->SetVisibility(false, true);
    CollisionMesh->SetHiddenInGame(true, true);
}

void ASM64StaticCourseActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Map Check warns for any serialized UStaticMeshComponent with a null mesh,
    // even when that optional component is intentionally disabled.  Give the
    // inactive half the other authoritative mesh, then keep it hidden and
    // collision-disabled.  This does not duplicate rendering or collision.
    VisualMesh->SetStaticMesh(DefaultMesh ? DefaultMesh : DefaultCollisionMesh);
    VisualMesh->SetVisibility(DefaultMesh != nullptr, true);
    CollisionMesh->SetStaticMesh(DefaultCollisionMesh ? DefaultCollisionMesh : DefaultMesh);
    CollisionMesh->SetCollisionEnabled(
        DefaultCollisionMesh ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}
