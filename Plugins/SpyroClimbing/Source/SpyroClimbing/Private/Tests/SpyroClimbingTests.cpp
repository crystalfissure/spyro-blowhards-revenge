#if WITH_DEV_AUTOMATION_TESTS

#include "SpyroClimbingComponent.h"
#include "SpyroClimbSurface.h"
#include "SpyroClimbingTestObserver.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"

namespace SpyroClimbingTests
{
    struct FClimbTestScene
    {
        UWorld* World;
        ACharacter* Character;
        UCharacterMovementComponent* Movement;
        USpyroClimbingComponent* Climbing;
        ASpyroClimbSurface* Wall;

        FClimbTestScene()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            // A base GameMode enables the real APawn::TakeDamage path without loading project gameplay.
            World->SetGameInstance(NewObject<UGameInstance>(World));
            World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
            World->SetGameMode(FURL());
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            Wall = World->SpawnActor<ASpyroClimbSurface>(FVector(0, 0, 300), FRotator::ZeroRotator);
            Character = World->SpawnActor<ACharacter>(FVector(100, 0, 300), FRotator(0, 180, 0));
            Movement = Character->GetCharacterMovement();
            Movement->SetMovementMode(MOVE_Falling);
            Climbing = NewObject<USpyroClimbingComponent>(Character);
            Character->AddInstanceComponent(Climbing);
            Climbing->RegisterComponent();
            Climbing->RegrabDelay = 0.f;
        }

        ~FClimbTestScene()
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }

        void Step(FVector2D Input, float Seconds)
        {
            Climbing->AddClimbInput(Input);
            Climbing->TickComponent(Seconds, LEVELTICK_All, &Climbing->PrimaryComponentTick);
        }

        void Reset()
        {
            Climbing->CancelClimbing();
            Character->SetActorLocationAndRotation(FVector(100, 0, 300), FRotator(0, 180, 0));
            Movement->SetMovementMode(MOVE_Falling);
        }

        UBoxComponent* Obstacle(const FVector& Location, const FVector& Extent)
        {
            AActor* Actor = World->SpawnActor<AActor>();
            UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Box);
            Box->SetBoxExtent(Extent);
            Box->SetCollisionProfileName(TEXT("BlockAll"));
            Box->RegisterComponent();
            Actor->SetActorLocation(Location);
            return Box;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingEntryTest, "SpyroClimbing.EntryAndEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingEntryTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    TestTrue(TEXT("Component initialized"), S.Climbing->HasBegunPlay());
    TestTrue(TEXT("Nearby spatial query finds front face"), S.Climbing->TryStartClimbingNearby());
    TestEqual(TEXT("Snaps to radius plus wall gap"), S.Character->GetActorLocation().X,
        S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + S.Climbing->WallGap);
    TestFalse(TEXT("Movement tick suspended"), S.Movement->IsComponentTickEnabled());
    TestFalse(TEXT("Cannot double enter"), S.Climbing->TryStartClimbing(S.Wall));
    S.Reset();
    S.Character->SetActorLocation(FVector(-100, 0, 300));
    TestFalse(TEXT("Back face rejected"), S.Climbing->TryStartClimbing(S.Wall));
    S.Reset();
    S.Character->SetActorRotation(FRotator::ZeroRotator);
    TestFalse(TEXT("Facing away rejected"), S.Climbing->TryStartClimbing(S.Wall));
    S.Reset();
    S.Character->SetActorLocation(FVector(1000, 0, 300));
    TestFalse(TEXT("Far face rejected"), S.Climbing->TryStartClimbing(S.Wall));
    S.Reset();
    S.Movement->SetMovementMode(MOVE_Swimming);
    TestFalse(TEXT("Swimming rejected"), S.Climbing->TryStartClimbing(S.Wall));
    S.Reset();
    S.Climbing->SetClimbingAllowed(false);
    TestFalse(TEXT("Gameplay gate respected"), S.Climbing->TryStartClimbing(S.Wall));
    S.Climbing->SetClimbingAllowed(true);
    S.Wall->SetActorScale3D(FVector(2.f));
    TestFalse(TEXT("Scaled panel rejected"), S.Climbing->TryStartClimbing(S.Wall));
    S.Wall->SetActorScale3D(FVector::OneVector);
    S.Wall->SetActorRotation(FRotator(10.f, 0.f, 0.f));
    TestFalse(TEXT("Tilted panel rejected"), S.Climbing->TryStartClimbing(S.Wall));
    S.Wall->SetActorRotation(FRotator::ZeroRotator);
    S.Wall->Width = 10.f;
    TestFalse(TEXT("Panel narrower than required support rejected"), S.Climbing->TryStartClimbing(S.Wall));
    S.Wall->Width = 400.f;
    S.Obstacle(FVector(65, 0, 300), FVector(2, 100, 100));
    TestFalse(TEXT("Obstructed approach rejected"), S.Climbing->TryStartClimbing(S.Wall));
    TestTrue(TEXT("Failed grab leaves movement tick enabled"), S.Movement->IsComponentTickEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingMovementTest, "SpyroClimbing.MovementAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingMovementTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    if (!TestTrue(TEXT("Enter"), S.Climbing->TryStartClimbing(S.Wall))) { return false; }
    FVector Start = S.Character->GetActorLocation();
    S.Step(FVector2D(1, 1), .5f);
    TestTrue(TEXT("Equal two-axis input moves only upward at full vertical speed"),
        (S.Character->GetActorLocation() - Start).Equals(FVector(0, 0, 50), .1f));
    TestTrue(TEXT("Animation input reports only the selected vertical axis"), S.Climbing->ClimbInput.Equals(FVector2D(0, 1)));
    Start = S.Character->GetActorLocation();
    S.Step(FVector2D(1, 0), .25f);
    TestTrue(TEXT("Right points right when facing panel"), S.Character->GetActorLocation().Y < 0);
    TestTrue(TEXT("Right does not change height"), FMath::IsNearlyEqual(S.Character->GetActorLocation().Z, Start.Z, .1f));
    Start = S.Character->GetActorLocation();
    S.Step(FVector2D::ZeroVector, .5f);
    TestTrue(TEXT("Input consumed once; release stops immediately"), Start.Equals(S.Character->GetActorLocation(), .01f));
    TestTrue(TEXT("Idle velocity is zero"), S.Climbing->ClimbVelocity.IsNearlyZero());
    S.Step(FVector2D(0, 1), 10.f);
    const float Top = S.Wall->GetActorLocation().Z + S.Wall->Height * .5f - S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    TestTrue(TEXT("Capsule stays below top"), FMath::IsNearlyEqual(S.Character->GetActorLocation().Z, Top, .1f));
    S.Step(FVector2D(1, 0), 10.f);
    const float Right = S.Wall->Width * .5f
        - S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius() * S.Climbing->HorizontalEdgeMargin;
    TestTrue(TEXT("Capsule respects side-edge margin"), FMath::IsNearlyEqual(S.Character->GetActorLocation().Y, -Right, .1f));
    S.Step(FVector2D(-1, -1), .1f);
    TestTrue(TEXT("Equal negative input moves down without moving sideways"),
        FMath::IsNearlyEqual(S.Character->GetActorLocation().Y, -Right, .1f) && S.Character->GetActorLocation().Z < Top - 1.f);
    S.Step(FVector2D(-1, 0), .1f);
    TestTrue(TEXT("Can move back away from an edge"), S.Character->GetActorLocation().Y > -Right + 1.f);
    S.Reset();
    S.Climbing->TryStartClimbing(S.Wall);
    S.Step(FVector2D(0, 1), 1.f);
    const FVector OneStep = S.Character->GetActorLocation();
    S.Reset();
    S.Climbing->TryStartClimbing(S.Wall);
    for (int32 Index = 0; Index < 60; ++Index) { S.Step(FVector2D(0, 1), 1.f / 60.f); }
    TestTrue(TEXT("Frame rate independent travel"), OneStep.Equals(S.Character->GetActorLocation(), .1f));
    S.Reset();
    S.Wall->SetActorRotation(FRotator(0, 90, 0));
    S.Character->SetActorLocationAndRotation(FVector(0, 100, 300), FRotator(0, -90, 0));
    TestTrue(TEXT("Yaw rotated panel accepts entry"), S.Climbing->TryStartClimbingNearby());
    S.Step(FVector2D(1, 0), .5f);
    TestTrue(TEXT("Input follows rotated panel"), S.Character->GetActorLocation().X > 49.f);

    S.Reset();
    S.Wall->SetActorRotation(FRotator::ZeroRotator);
    S.Climbing->HorizontalClimbingSpeed = 60.f;
    S.Climbing->ClimbSpeed = 120.f;
    if (!TestTrue(TEXT("Enter with independent speeds"), S.Climbing->TryStartClimbing(S.Wall))) { return false; }
    Start = S.Character->GetActorLocation();
    S.Step(FVector2D(1, 0), .5f);
    TestTrue(TEXT("Horizontal setting controls sideways travel"),
        (S.Character->GetActorLocation() - Start).Equals(FVector(0, -30, 0), .1f));
    Start = S.Character->GetActorLocation();
    S.Step(FVector2D(0, 1), .5f);
    TestTrue(TEXT("Vertical setting independently controls upward travel"),
        (S.Character->GetActorLocation() - Start).Equals(FVector(0, 0, 60), .1f));
    Start = S.Character->GetActorLocation();
    S.Step(FVector2D(-.5f, 0), .5f);
    TestTrue(TEXT("Analog input scales horizontal speed"),
        (S.Character->GetActorLocation() - Start).Equals(FVector(0, 15, 0), .1f));
    S.Step(FVector2D(1, -1), .25f);
    TestTrue(TEXT("Equal mixed-sign input uses only vertical speed"),
        S.Climbing->ClimbVelocity.Equals(FVector2D(0, -120), .1f));
    S.Step(FVector2D(-.75f, .25f), .2f);
    TestTrue(TEXT("Stronger horizontal input retains analog strength despite faster vertical setting"),
        S.Climbing->ClimbVelocity.Equals(FVector2D(-45, 0), .1f));
    TestTrue(TEXT("Animation input reports only selected horizontal axis"),
        S.Climbing->ClimbInput.Equals(FVector2D(-.75f, 0)));
    S.Step(FVector2D(.2f, -.5f), .2f);
    TestTrue(TEXT("Stronger vertical input retains analog strength"),
        S.Climbing->ClimbVelocity.Equals(FVector2D(0, -60), .1f));
    S.Climbing->HorizontalClimbingSpeed = 0.f;
    Start = S.Character->GetActorLocation();
    S.Step(FVector2D(1, 0), .5f);
    TestTrue(TEXT("Zero horizontal speed stops only sideways travel"),
        S.Character->GetActorLocation().Equals(Start, .1f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingCollisionTest, "SpyroClimbing.CollisionAndSurfaceLoss",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingCollisionTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    if (!TestTrue(TEXT("Enter"), S.Climbing->TryStartClimbing(S.Wall))) { return false; }
    S.Obstacle(FVector(50, 0, 430), FVector(100, 150, 10));
    S.Step(FVector2D(0, 1), 1.f);
    TestTrue(TEXT("Sweep stops before ceiling"), S.Character->GetActorLocation().Z
        <= 420.f - S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + .2f);
    TestTrue(TEXT("Collision keeps climbing active"), S.Climbing->IsClimbing());
    S.Step(FVector2D(0, 1), .1f);
    TestTrue(TEXT("Blocked velocity suitable for idle animation"), S.Climbing->ClimbVelocity.Size() < 1.f);
    TestEqual(TEXT("Obstacle selects idle animation"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
    S.Wall->SetActorLocation(S.Wall->GetActorLocation() + FVector(1, 0, 0));
    S.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("Moving panel releases character"), S.Climbing->IsClimbing());
    TestTrue(TEXT("Movement tick restored"), S.Movement->IsComponentTickEnabled());
    S.Reset();
    S.Climbing->TryStartClimbing(S.Wall);
    S.Wall->Destroy();
    S.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("Destroyed panel releases character"), S.Climbing->IsClimbing());
    TestEqual(TEXT("Lost panel falls"), S.Movement->MovementMode.GetValue(), MOVE_Falling);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingExitTest, "SpyroClimbing.ExitDamageAndStateRestoration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingExitTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    S.Character->bUseControllerRotationYaw = true;
    S.Character->bUseControllerRotationPitch = true;
    S.Movement->bOrientRotationToMovement = true;
    S.Movement->bUseControllerDesiredRotation = true;
    if (!TestTrue(TEXT("Enter"), S.Climbing->TryStartClimbing(S.Wall))) { return false; }
    TestFalse(TEXT("Controller rotation disabled during climb"), S.Character->bUseControllerRotationYaw);
    S.Climbing->RegrabDelay = .4f;
    TestTrue(TEXT("Jump handled"), S.Climbing->JumpOff());
    TestFalse(TEXT("Jump exits"), S.Climbing->IsClimbing());
    TestTrue(TEXT("Jump launches out and up"), S.Movement->PendingLaunchVelocity.Equals(
        FVector(S.Climbing->JumpAwaySpeed, 0, S.Climbing->JumpUpSpeed), .01f));
    TestTrue(TEXT("Controller yaw restored"), S.Character->bUseControllerRotationYaw);
    TestTrue(TEXT("Controller pitch restored"), S.Character->bUseControllerRotationPitch);
    TestTrue(TEXT("Movement orientation restored"), S.Movement->bOrientRotationToMovement);
    TestTrue(TEXT("Desired rotation restored"), S.Movement->bUseControllerDesiredRotation);
    TestTrue(TEXT("Movement tick restored after jump"), S.Movement->IsComponentTickEnabled());
    TestFalse(TEXT("Regrab cooldown enforced"), S.Climbing->TryStartClimbing(S.Wall));
    TestFalse(TEXT("Normal jump not consumed when not climbing"), S.Climbing->JumpOff());

    FClimbTestScene D;
    D.Climbing->TryStartClimbing(D.Wall);
    const FVector Knockback(100, 20, 300);
    D.Character->LaunchCharacter(Knockback, true, true);
    D.Climbing->NotifyDamaged();
    TestFalse(TEXT("Custom damage exits"), D.Climbing->IsClimbing());
    TestTrue(TEXT("Damage pending launch preserved"), D.Movement->PendingLaunchVelocity.Equals(Knockback));
    D.Reset();
    D.Climbing->TryStartClimbing(D.Wall);
    UGameplayStatics::ApplyDamage(D.Character, 1.f, nullptr, nullptr, nullptr);
    TestFalse(TEXT("Native damage exits"), D.Climbing->IsClimbing());
    D.Reset();
    D.Climbing->TryStartClimbing(D.Wall);
    D.Movement->SetMovementMode(MOVE_Swimming);
    D.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("External mode interrupts climb"), D.Climbing->IsClimbing());
    TestEqual(TEXT("External swimming mode preserved"), D.Movement->MovementMode.GetValue(), MOVE_Swimming);
    D.Reset();
    D.Climbing->TryStartClimbing(D.Wall);
    D.Movement->DisableMovement();
    D.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("External disabled mode interrupts climb"), D.Climbing->IsClimbing());
    TestEqual(TEXT("External disabled mode preserved"), D.Movement->MovementMode.GetValue(), MOVE_None);
    D.Reset();
    D.Climbing->TryStartClimbing(D.Wall);
    D.Climbing->SetClimbingAllowed(false);
    TestFalse(TEXT("Gameplay gate exits existing climb"), D.Climbing->IsClimbing());
    TestTrue(TEXT("Gameplay gate restores movement tick"), D.Movement->IsComponentTickEnabled());
    D.Climbing->SetClimbingAllowed(true);
    D.Reset();
    D.Climbing->TryStartClimbing(D.Wall);
    D.Climbing->Deactivate();
    TestFalse(TEXT("Deactivation exits"), D.Climbing->IsClimbing());
    TestTrue(TEXT("Deactivation restores movement"), D.Movement->IsComponentTickEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingMeshTest, "SpyroClimbing.MeshCollisionOffsetAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingMeshTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    S.Wall->bFitDefaultCube = false;
    S.Wall->WallMesh->SetRelativeLocation(FVector(5000, 2000, 0));
    S.Wall->WallMesh->SetRelativeRotation(FRotator(0, 37, 0));
    S.Wall->RefreshSurface();
    TestTrue(TEXT("Unfitted mesh automatically uses collision"), S.Wall->UsesMeshCollision());
    TestEqual(TEXT("Default mesh is WorldStatic"), S.Wall->WallMesh->GetCollisionObjectType(), ECC_WorldStatic);
    const FTransform Transform = S.Wall->WallMesh->GetComponentTransform();
    const FVector Normal = Transform.GetUnitAxis(EAxis::X);
    const FVector Centre = Transform.TransformPosition(FVector(50, 0, 0));
    const auto Approach = [&]()
    {
        S.Climbing->CancelClimbing();
        S.Character->SetActorLocationAndRotation(Centre + Normal * 70.f, (-Normal).Rotation());
        S.Movement->SetMovementMode(MOVE_Falling);
    };
    Approach();
    if (!TestTrue(TEXT("Finds static mesh far from actor origin"), S.Climbing->TryStartClimbingNearby())) { return false; }
    TestTrue(TEXT("Faces the mesh instead of actor +X"), S.Character->GetActorForwardVector().Equals(-Normal, .001f));
    const FVector MeshStart = S.Character->GetActorLocation();
    S.Step(FVector2D(1, 1), .25f);
    TestTrue(TEXT("Mesh collision climbing also rejects diagonal movement"),
        (S.Character->GetActorLocation() - MeshStart).Equals(FVector(0, 0, 25), .1f));
    S.Step(FVector2D(0, 1), 5.f);
    const float Top = Centre.Z + S.Wall->Height * .5f - S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    TestTrue(TEXT("Stops at actual mesh top"), FMath::IsNearlyEqual(S.Character->GetActorLocation().Z, Top, 1.f));
    const FVector AtEdge = S.Character->GetActorLocation();
    S.Step(FVector2D(0, 1), .1f);
    TestTrue(TEXT("Remains on mesh edge"), AtEdge.Equals(S.Character->GetActorLocation(), .1f));
    TestTrue(TEXT("Still climbing at mesh edge"), S.Climbing->IsClimbing());
    TestTrue(TEXT("Mesh jump handled"), S.Climbing->JumpOff());
    TestTrue(TEXT("Jump uses actual mesh normal"), S.Movement->PendingLaunchVelocity.Equals(
        Normal * S.Climbing->JumpAwaySpeed + FVector::UpVector * S.Climbing->JumpUpSpeed, .01f));
    Approach();
    S.Climbing->TryStartClimbingNearby();
    S.Wall->WallMesh->AddRelativeLocation(FVector(1, 0, 0));
    S.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("Moving mesh component releases climb"), S.Climbing->IsClimbing());
    S.Wall->ResetWallMeshTransform();
    TestTrue(TEXT("Reset helper removes cube offset and scale"), S.Wall->WallMesh->GetRelativeTransform().Equals(FTransform::Identity));
    S.Wall->GeometryMode = ESpyroClimbGeometry::Rectangle;
    TestFalse(TEXT("Explicit rectangle remains available"), S.Wall->UsesMeshCollision());
    return true;
}

// Project-specific regression: deliberately loads the supplied imported mesh, without changing/saving it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingAutumnPlainsTest, "SpyroClimbing.Project.AutumnPlainsMergedMesh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingAutumnPlainsTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Experimental_Mechanics/Climbing/Test_Level/Autumn_Plains_Climbing_Surfaces_Merged.Autumn_Plains_Climbing_Surfaces_Merged"));
    if (!TestNotNull(TEXT("Supplied imported climbing mesh"), Mesh)) { return false; }
    if (!TestNotNull(TEXT("Mesh render data for test inspection"), Mesh->GetRenderData())) { return false; }
    AddInfo(FString::Printf(TEXT("Imported mesh bounds: %s, collision complexity: %d"),
        *Mesh->GetBounds().ToString(), int32(Mesh->GetBodySetup()->CollisionTraceFlag)));
    S.Wall->SetActorLocation(FVector::ZeroVector);
    S.Wall->ResetWallMeshTransform();
    S.Wall->WallMesh->SetStaticMesh(Mesh);
    S.Wall->RefreshSurface();

    struct FMeshPlane { FVector Normal; FVector Origin; TArray<FVector> Vertices; };
    TArray<FMeshPlane> Planes;
    const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
    for (int32 Index = 0; Index < LOD.IndexBuffer.GetNumIndices(); Index += 3)
    {
        const FVector A = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(Index));
        const FVector B = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(Index + 1));
        const FVector C = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(Index + 2));
        const FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (Normal.IsNearlyZero()) { continue; }
        int32 Group = Planes.IndexOfByPredicate([&](const FMeshPlane& P)
        {
            return FMath::Abs(FVector::DotProduct(P.Normal, Normal)) > .9999f
                && FMath::Abs(FVector::DotProduct(A - P.Origin, P.Normal)) < .1f;
        });
        if (Group == INDEX_NONE)
        {
            FMeshPlane Plane;
            Plane.Normal = Normal;
            Plane.Origin = A;
            Group = Planes.Add(Plane);
        }
        Planes[Group].Vertices.Append({ A, B, C });
    }
    TestEqual(TEXT("Three distinct flat wall planes in supplied asset"), Planes.Num(), 3);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbTestMesh), true);
    for (int32 Index = 0; Index < Planes.Num(); ++Index)
    {
        const FMeshPlane& Plane = Planes[Index];
        FSpyroClimbFrame Frame;
        Frame.Normal = Plane.Normal;
        Frame.Origin = Plane.Origin;
        Frame.Right = FVector::CrossProduct(Frame.Normal, FVector::UpVector).GetSafeNormal();
        FVector2D Min(MAX_flt, MAX_flt), Max(-MAX_flt, -MAX_flt);
        for (const FVector& Vertex : Plane.Vertices)
        {
            const FVector2D XY = Frame.ToCoordinates(Vertex);
            Min.X = FMath::Min(Min.X, XY.X); Min.Y = FMath::Min(Min.Y, XY.Y);
            Max.X = FMath::Max(Max.X, XY.X); Max.Y = FMath::Max(Max.Y, XY.Y);
        }
        const FVector Centre = Frame.ToWorld((Min + Max) * .5f, 0.f);
        FHitResult Hit;
        bool bHit = S.Wall->WallMesh->LineTraceComponent(Hit, Centre + Frame.Normal * 100.f, Centre - Frame.Normal * 100.f, Params);
        if (!bHit)
        {
            bHit = S.Wall->WallMesh->LineTraceComponent(Hit, Centre - Frame.Normal * 100.f, Centre + Frame.Normal * 100.f, Params);
        }
        if (!TestTrue(FString::Printf(TEXT("Panel %d has real mesh collision"), Index + 1), bHit)) { continue; }
        const FVector Normal = Hit.ImpactNormal.GetSafeNormal2D();
        AddInfo(FString::Printf(TEXT("Panel %d: centre %s, normal %s, size %.1f x %.1f cm"),
            Index + 1, *Centre.ToString(), *Normal.ToString(), Max.X - Min.X, Max.Y - Min.Y));
        S.Climbing->CancelClimbing();
        S.Character->SetActorLocationAndRotation(Centre + Normal * 70.f, (-Normal).Rotation());
        S.Movement->SetMovementMode(MOVE_Falling);
        if (!TestTrue(FString::Printf(TEXT("Panel %d can be found and grabbed with merged origin"), Index + 1),
            S.Climbing->TryStartClimbingNearby())) { continue; }
        const FVector Start = S.Character->GetActorLocation();
        S.Step(FVector2D(0, 1), .4f);
        TestTrue(TEXT("Climbs across the panel's triangles"), S.Character->GetActorLocation().Z > Start.Z + 39.f);
        S.Step(FVector2D(0, 1), 10.f);
        const float Top = Centre.Z + (Max.Y - Min.Y) * .5f - S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        TestTrue(TEXT("Stops at imported panel's top"), FMath::IsNearlyEqual(S.Character->GetActorLocation().Z, Top, 1.f));
        TestTrue(TEXT("Mesh jump handled"), S.Climbing->JumpOff());
        TestTrue(TEXT("Merged panel jump follows that panel's normal"), S.Movement->PendingLaunchVelocity.Equals(
            Normal * S.Climbing->JumpAwaySpeed + FVector::UpVector * S.Climbing->JumpUpSpeed, .1f));
    }
    S.Climbing->CancelClimbing();
    S.Character->SetActorLocationAndRotation(FVector(70, 0, 0), FRotator(0, 180, 0));
    S.Movement->SetMovementMode(MOVE_Falling);
    TestFalse(TEXT("No phantom climb rectangle at merged mesh pivot"), S.Climbing->TryStartClimbingNearby());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingTransferAnalogTest, "SpyroClimbing.Transfer.AnalogAndLanding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingTransferAnalogTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    S.Wall->Width = S.Wall->Height = 2000.f;
    S.Wall->RefreshSurface();
    S.Climbing->LandingSearchTolerance = 0.f;
    S.Climbing->bDropOffOnDownJump = false; // Explicitly exercise optional downward transfers.
    const auto Grab = [&]() { S.Reset(); return S.Climbing->TryStartClimbing(S.Wall); };
    if (!TestTrue(TEXT("Attach before transfer"), Grab())) { return false; }
    FVector Start = S.Character->GetActorLocation();
    TestTrue(TEXT("Partial analog jump consumed"), S.Climbing->JumpOrTransfer(FVector2D(0, .5f)));
    TestTrue(TEXT("Owns movement while airborne"), S.Climbing->IsClimbing() && S.Climbing->IsTransferring());
    TestFalse(TEXT("Normal movement stays suspended"), S.Movement->IsComponentTickEnabled());
    TestEqual(TEXT("Same surface is a valid target"), S.Climbing->GetTransferTarget(), S.Wall);
    TestTrue(TEXT("Half stick requests half distance"), S.Climbing->TransferTargetLocation.Equals(
        Start + FVector::UpVector * S.Climbing->VerticalJumpDistance * .5f, .01f));
    const float ShortDuration = S.Climbing->TransferDuration;
    const FVector Landing = S.Climbing->TransferTargetLocation;
    S.Step(FVector2D(1, 0), ShortDuration * .5f);
    TestTrue(TEXT("Hop arcs out of the wall"), S.Character->GetActorLocation().X > Start.X + 10.f);
    TestTrue(TEXT("Halfway progress"), FMath::IsNearlyEqual(S.Climbing->TransferProgress, .5f, .001f));
    TestTrue(TEXT("Repeat jump consumed"), S.Climbing->JumpOrTransfer(FVector2D(-1, 0)));
    TestTrue(TEXT("Repeat press/input cannot redirect reserved landing"), S.Climbing->TransferTargetLocation.Equals(Landing));
    S.Step(FVector2D(1, 0), ShortDuration * .5f + .001f);
    TestTrue(TEXT("Reattaches without ending movement ownership"), S.Climbing->IsClimbing() && !S.Climbing->IsTransferring());
    TestTrue(TEXT("Exact reserved landing"), S.Character->GetActorLocation().Equals(Landing, .01f));
    TestTrue(TEXT("Landing clears airborne velocity"), S.Movement->Velocity.IsNearlyZero());
    S.Step(FVector2D::ZeroVector, .1f);
    TestTrue(TEXT("Airborne movement input does not leak after landing"), S.Character->GetActorLocation().Equals(Landing, .01f));
    S.Step(FVector2D(1, 0), .1f);
    TestTrue(TEXT("Normal climbing resumes"), S.Character->GetActorLocation().Y < Landing.Y - 9.f);

    Grab();
    Start = S.Character->GetActorLocation();
    S.Climbing->JumpOrTransfer(FVector2D(1, 1));
    TestTrue(TEXT("Diagonal jump input chooses vertical on ties"), S.Climbing->TransferTargetLocation.Equals(
        Start + FVector::UpVector * S.Climbing->VerticalJumpDistance, .01f));
    TestTrue(TEXT("Full reach takes longer than half reach"), S.Climbing->TransferDuration > ShortDuration);
    const float FullDuration = S.Climbing->TransferDuration;
    S.Step(FVector2D::ZeroVector, FullDuration + 1.f);
    const FVector LargeStepLanding = S.Character->GetActorLocation();
    TestFalse(TEXT("A hitch still completes the full curved sweep"), S.Climbing->IsTransferring());
    Grab();
    S.Climbing->JumpOrTransfer(FVector2D(0, 1));
    for (int32 I = 0; I < 61; ++I) { S.Step(FVector2D::ZeroVector, FullDuration / 60.f); }
    TestTrue(TEXT("Small steps match large-step landing"), S.Character->GetActorLocation().Equals(LargeStepLanding, .01f));
    for (const FVector2D Input : { FVector2D(.75f, .2f), FVector2D(-.75f, .2f), FVector2D(.2f, -.75f) })
    {
        Grab();
        Start = S.Character->GetActorLocation();
        S.Climbing->JumpOrTransfer(Input);
        const bool bHorizontal = FMath::Abs(Input.X) > FMath::Abs(Input.Y);
        const FVector Expected = Start + (bHorizontal ? -FVector::RightVector * Input.X * S.Climbing->HorizontalJumpDistance
            : FVector::UpVector * Input.Y * S.Climbing->VerticalJumpDistance);
        TestTrue(TEXT("Signed stronger axis keeps its analog distance"), S.Climbing->TransferTargetLocation.Equals(Expected, .01f));
        S.Step(FVector2D::ZeroVector, 2.f);
        TestTrue(TEXT("Left/right/down hop lands on its target"), S.Character->GetActorLocation().Equals(Expected, .01f));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingHorizontalArcTest, "SpyroClimbing.Transfer.HorizontalUpwardArc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingHorizontalArcTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    for (float Pitch : { 0.f, -20.f, 20.f })
    {
        FClimbTestScene S;
        S.Wall->GeometryMode = Pitch == 0.f ? ESpyroClimbGeometry::Rectangle : ESpyroClimbGeometry::MeshCollision;
        S.Wall->Width = S.Wall->Height = 2000.f;
        S.Wall->RefreshSurface();
        S.Wall->SetActorRotation(FRotator(Pitch, 37, 0));
        S.Climbing->LandingSearchTolerance = 0.f;
        FSpyroClimbFrame Frame;
        Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
        float HalfDuration = 0.f;
        for (float Axis : { -.5f, -1.f, .5f, 1.f })
        {
            S.Climbing->CancelClimbing();
            S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, 100.f), Frame.FacingRotation());
            S.Climbing->HorizontalTransferArcHeight = 50.f;
            if (!TestTrue(TEXT("Grab before sideways arc"), S.Climbing->TryStartClimbing(S.Wall))) { continue; }
            const FVector Start = S.Character->GetActorLocation();
            S.Climbing->JumpOrTransfer(FVector2D(Axis, 0));
            if (!TestTrue(TEXT("Sideways arc starts"), S.Climbing->IsTransferring())) { continue; }
            const FVector Landing = S.Climbing->TransferTargetLocation;
            TestTrue(TEXT("Lift preserves horizontal reach and landing height"), Landing.Equals(
                Start + Frame.Right * (S.Climbing->HorizontalJumpDistance * Axis), .05f));
            const float Duration = S.Climbing->TransferDuration;
            if (FMath::Abs(Axis) < 1.f) { HalfDuration = Duration; }
            else { TestTrue(TEXT("Short analog arc still takes less time"), Duration > HalfDuration); }
            // Changing tuning after takeoff must not invalidate the already checked trajectory.
            S.Climbing->HorizontalTransferArcHeight = 0.f;
            const float Scale = FMath::Sqrt(FMath::Abs(Axis));
            const FVector Arc = Frame.Normal * (S.Climbing->TransferArcHeight * Scale) + FVector::UpVector * (50.f * Scale);
            S.Step(FVector2D::ZeroVector, Duration * .25f);
            TestTrue(TEXT("Sideways jump rises along the new arc"), S.Character->GetActorLocation().Equals(
                FMath::Lerp(Start, Landing, .25f) + Arc * .75f, .05f));
            S.Step(FVector2D::ZeroVector, Duration * .25f);
            TestTrue(TEXT("Left and right arcs peak halfway with analog-scaled lift"), S.Character->GetActorLocation().Equals(
                (Start + Landing) * .5f + Arc, .05f));
            TestTrue(TEXT("Default horizontal arc rises even on inclined walls"), S.Character->GetActorLocation().Z > Start.Z);
            S.Step(FVector2D::ZeroVector, Duration * .25f);
            TestTrue(TEXT("Sideways jump descends toward reserved landing"), S.Character->GetActorLocation().Equals(
                FMath::Lerp(Start, Landing, .75f) + Arc * .75f, .05f));
            S.Step(FVector2D::ZeroVector, Duration);
            TestTrue(TEXT("Raised arc lands exactly and resumes climbing"), S.Climbing->IsClimbing()
                && !S.Climbing->IsTransferring() && S.Character->GetActorLocation().Equals(Landing, .05f));
        }
        // A vertical jump immediately after a horizontal one must discard its extra world-up arc.
        S.Climbing->bDropOffOnDownJump = false;
        S.Climbing->HorizontalTransferArcHeight = 200.f;
        for (float Axis : { -.5f, .5f })
        {
            const FVector Start = S.Character->GetActorLocation();
            S.Climbing->JumpOrTransfer(FVector2D(0, Axis));
            if (!TestTrue(TEXT("Vertical transfer after horizontal one starts"), S.Climbing->IsTransferring())) { continue; }
            const FVector Landing = S.Climbing->TransferTargetLocation;
            S.Step(FVector2D::ZeroVector, S.Climbing->TransferDuration * .5f);
            TestTrue(TEXT("Up/down transfers retain their original outward-only arc"), S.Character->GetActorLocation().Equals(
                (Start + Landing) * .5f + Frame.Normal * (S.Climbing->TransferArcHeight * FMath::Sqrt(.5f)), .05f));
            S.Step(FVector2D::ZeroVector, 1.f);
        }
    }

    // An overhead obstacle clears the old horizontal path but intersects the new upward arc.
    FClimbTestScene S;
    S.Wall->Width = S.Wall->Height = 2000.f;
    S.Wall->RefreshSurface();
    S.Climbing->LandingSearchTolerance = 0.f;
    const auto Grab = [&]() { S.Reset(); return S.Climbing->TryStartClimbing(S.Wall); };
    if (!TestTrue(TEXT("Grab before overhead clearance check"), Grab())) { return false; }
    const FVector Start = S.Character->GetActorLocation();
    const FVector Ceiling = Start + FVector(S.Climbing->TransferArcHeight, -S.Climbing->HorizontalJumpDistance * .5f,
        S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 30.f);
    UBoxComponent* Blocker = S.Obstacle(Ceiling, FVector(8, 12, 5));
    S.Climbing->HorizontalTransferArcHeight = 0.f;
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestTrue(TEXT("Zero height keeps old path below overhead obstacle"), S.Climbing->IsTransferring());
    S.Step(FVector2D::ZeroVector, S.Climbing->TransferDuration * .5f);
    TestTrue(TEXT("Zero lift has no vertical hump"), FMath::IsNearlyEqual(S.Character->GetActorLocation().Z, Start.Z, .01f));
    S.Step(FVector2D::ZeroVector, 1.f);
    TestTrue(TEXT("Zero-height path lands"), S.Climbing->IsClimbing() && !S.Climbing->IsTransferring());
    Grab();
    S.Climbing->HorizontalTransferArcHeight = 50.f;
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestFalse(TEXT("Preflight rejects overhead obstacle on raised arc"), S.Climbing->IsClimbing());
    Blocker->GetOwner()->Destroy();
    Grab();
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestTrue(TEXT("Raised arc starts when overhead space is clear"), S.Climbing->IsTransferring());
    S.Obstacle(Ceiling, FVector(8, 12, 5));
    S.Step(FVector2D::ZeroVector, 1.f);
    TestFalse(TEXT("Hitch still sweeps raised arc against new overhead obstacle"), S.Climbing->IsClimbing());
    TestTrue(TEXT("Blocked arc restores ordinary movement"), S.Movement->IsComponentTickEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingDropTest, "SpyroClimbing.Transfer.DownwardDropOff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingDropTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    USpyroClimbingTestObserver* Observer = NewObject<USpyroClimbingTestObserver>(S.Climbing);
    S.Climbing->OnClimbEnded.AddDynamic(Observer, &USpyroClimbingTestObserver::OnClimbEnded);
    TestTrue(TEXT("Downward drop is enabled by default"), S.Climbing->bDropOffOnDownJump);
    TestEqual(TEXT("Reason has requested Blueprint label"), StaticEnum<ESpyroClimbExitReason>()->GetDisplayNameTextByValue(
        static_cast<int64>(ESpyroClimbExitReason::DroppedOff)).ToString(), FString(TEXT("Dropped Off")));

    for (float Pitch : { 0.f, 20.f, -20.f })
    {
        S.Wall->GeometryMode = Pitch == 0.f ? ESpyroClimbGeometry::Rectangle : ESpyroClimbGeometry::MeshCollision;
        S.Wall->SetActorRotation(FRotator(Pitch, 37, 0));
        FSpyroClimbFrame Frame;
        Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
        for (FVector2D Input : { FVector2D(0, -1), FVector2D(.2f, -.5f), FVector2D(1, -1) })
        {
            S.Climbing->RegrabDelay = 0.f;
            // Scene time is fixed in this fixture; tick the world clock past the previous cooldown.
            S.World->Tick(LEVELTICK_TimeOnly, .5f);
            S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, 100.f), Frame.FacingRotation());
            S.Movement->SetMovementMode(MOVE_Falling);
            const FTransform MeshBefore = S.Character->GetMesh()->GetRelativeTransform();
            if (!TestTrue(TEXT("Grab before drop"), S.Climbing->TryStartClimbing(S.Wall))) { continue; }
            S.Step(FVector2D(0, -1), .1f);
            TestTrue(TEXT("Ordinary down input still climbs"), S.Climbing->IsClimbing() && S.Movement->Velocity.Size() > 1.f);
            S.Climbing->RegrabDelay = .4f;
            const FVector BeforeDrop = S.Character->GetActorLocation();
            const int32 EventsBefore = Observer->EndCount;
            TestTrue(TEXT("Down jump is consumed"), S.Climbing->JumpOrTransfer(Input));
            TestFalse(TEXT("Drop exits instead of transferring"), S.Climbing->IsClimbing() || S.Climbing->IsTransferring());
            TestEqual(TEXT("Drop broadcasts exactly one exit"), Observer->EndCount, EventsBefore + 1);
            TestEqual(TEXT("Dispatcher reports Dropped Off"), Observer->LastReason, ESpyroClimbExitReason::DroppedOff);
            TestTrue(TEXT("Drop adds no launch velocity"), S.Movement->PendingLaunchVelocity.IsZero());
            TestTrue(TEXT("Climbing velocity is removed"), S.Movement->Velocity.IsZero());
            TestTrue(TEXT("Drop does not reposition character"), S.Character->GetActorLocation().Equals(BeforeDrop, .001f));
            TestEqual(TEXT("Gravity resumes through Falling"), S.Movement->MovementMode.GetValue(), MOVE_Falling);
            TestTrue(TEXT("Movement tick restored"), S.Movement->IsComponentTickEnabled());
            TestEqual(TEXT("Animation resets to idle"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
            TestTrue(TEXT("Drop restores mesh alignment"), S.Character->GetMesh()->GetRelativeTransform().Equals(MeshBefore, .001f));
            TestFalse(TEXT("Drop starts regrab cooldown"), S.Climbing->TryStartClimbing(S.Wall));
            TestFalse(TEXT("Repeated drop after exit is not handled"), S.Climbing->JumpOrTransfer(Input));
            TestEqual(TEXT("No duplicate exit after release"), Observer->EndCount, EventsBefore + 1);
        }
    }
    // Disabled mode restores a downward guided hop, including when support exists on this same wall.
    FClimbTestScene Optional;
    Optional.Wall->Width = Optional.Wall->Height = 2000.f;
    Optional.Wall->RefreshSurface();
    Optional.Climbing->bDropOffOnDownJump = false;
    Optional.Climbing->TryStartClimbing(Optional.Wall);
    Optional.Climbing->JumpOrTransfer(FVector2D(0, -.5f));
    TestTrue(TEXT("Disabling drop restores downward transfer"), Optional.Climbing->IsTransferring());
    const FVector Landing = Optional.Climbing->TransferTargetLocation;
    Optional.Climbing->bDropOffOnDownJump = true;
    Optional.Climbing->JumpOrTransfer(FVector2D(0, -1));
    TestTrue(TEXT("Repeated jump during flight does not trigger drop"), Optional.Climbing->IsTransferring());
    Optional.Step(FVector2D::ZeroVector, 1.f);
    TestTrue(TEXT("Optional downward transfer lands"), Optional.Climbing->IsClimbing()
        && Optional.Character->GetActorLocation().Equals(Landing, .01f));
    Optional.Reset();
    Optional.Climbing->TryStartClimbing(Optional.Wall);
    Optional.Climbing->JumpOrTransfer(FVector2D(.8f, -.2f));
    TestTrue(TEXT("Stronger horizontal input keeps sideways jump"), Optional.Climbing->IsTransferring());
    Optional.Reset();
    Optional.Climbing->TryStartClimbing(Optional.Wall);
    Optional.Climbing->JumpOrTransfer(FVector2D(0, -.05f));
    TestTrue(TEXT("Downward stick drift keeps neutral jump-off"), !Optional.Climbing->IsClimbing()
        && Optional.Movement->PendingLaunchVelocity.Size() > 1.f);
    S.Climbing->OnClimbEnded.RemoveDynamic(Observer, &USpyroClimbingTestObserver::OnClimbEnded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingLargeTransferTest, "SpyroClimbing.Transfer.LargeDescendingFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingLargeTransferTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    for (float Pitch : { 0.f, -20.f, 20.f })
    {
        float HalfDuration = 0.f;
        for (float Axis : { .5f, -1.f })
        {
            FClimbTestScene S;
            S.Wall->GeometryMode = Pitch == 0.f ? ESpyroClimbGeometry::Rectangle : ESpyroClimbGeometry::MeshCollision;
            S.Wall->Width = 140.f;
            S.Wall->Height = 1000.f;
            S.Wall->RefreshSurface();
            S.Wall->SetActorRotation(FRotator(Pitch, 37, 0));
            S.Climbing->HorizontalJumpDistance = 200.f;
            S.Climbing->LargeTransferDistance = 400.f;
            S.Climbing->LargeTransferHeightDrop = 100.f;
            S.Climbing->LargeTransferLandingSearchTolerance = 0.f;
            FSpyroClimbFrame Frame;
            Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
            const FVector LandingDelta = Frame.Right * (400.f * Axis) - Frame.Up * (100.f * FMath::Abs(Axis) / Frame.Up.Z);
            ASpyroClimbSurface* Destination = S.World->SpawnActor<ASpyroClimbSurface>(Frame.Origin + LandingDelta, S.Wall->GetActorRotation());
            Destination->GeometryMode = S.Wall->GeometryMode;
            Destination->Width = 140.f;
            Destination->Height = 400.f;
            Destination->RefreshSurface();
            const auto Grab = [&]()
            {
                S.Climbing->CancelClimbing();
                S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, 100.f), Frame.FacingRotation());
                S.Movement->SetMovementMode(MOVE_Falling);
                return S.Climbing->TryStartClimbing(S.Wall);
            };
            if (!TestTrue(TEXT("Grab before large transfer"), Grab())) { continue; }
            const FVector Start = S.Character->GetActorLocation();
            S.Climbing->JumpOrTransfer(FVector2D(Axis, 0));
            if (!TestTrue(TEXT("Second pass finds longer descending jump"), S.Climbing->IsLargeTransfer())) { continue; }
            TestEqual(TEXT("Large jump reserves the farther wall"), S.Climbing->GetTransferTarget(), Destination);
            const FVector Landing = S.Climbing->TransferTargetLocation;
            TestTrue(TEXT("Analog input scales reach and height drop"), Landing.Equals(Start + LandingDelta, .1f));
            TestTrue(TEXT("Large target is strictly below takeoff"), Landing.Z < Start.Z);
            const float Duration = S.Climbing->TransferDuration;
            TestTrue(TEXT("Large transfer takes longer than any normal transfer"), Duration > S.Climbing->MaxTransferDuration);
            if (FMath::Abs(Axis) < 1.f) { HalfDuration = Duration; }
            else { TestTrue(TEXT("Shorter analog large transfer takes less time"), Duration > HalfDuration); }
            S.Step(FVector2D::ZeroVector, Duration * .25f);
            TestTrue(TEXT("Large jump initially rises before falling to lower target"), S.Character->GetActorLocation().Z > Start.Z);
            // Reserved path and duration survive runtime tuning changes and repeated jump input.
            S.Climbing->LargeTransferHeightDrop = 400.f;
            S.Climbing->LargeTransferDistance = 1000.f;
            S.Climbing->LargeTransferArcHeight = S.Climbing->LargeTransferUpwardArcHeight = 0.f;
            S.Climbing->JumpOrTransfer(FVector2D(-Axis, 0));
            TestTrue(TEXT("Large destination remains locked"), S.Climbing->TransferTargetLocation.Equals(Landing));
            TestEqual(TEXT("Large duration remains locked"), S.Climbing->TransferDuration, Duration);
            S.Step(FVector2D::ZeroVector, Duration);
            TestTrue(TEXT("Large jump lands and resumes climbing"), S.Climbing->IsClimbing() && !S.Climbing->IsTransferring());
            TestFalse(TEXT("Landing clears large-transfer animation flag"), S.Climbing->IsLargeTransfer());
            TestTrue(TEXT("Large transfer reaches exact lower landing"), S.Character->GetActorLocation().Equals(Landing, .1f));
            const FVector BeforeMove = S.Character->GetActorLocation();
            S.Step(FVector2D(0, 1), .1f);
            TestTrue(TEXT("Climbing resumes on destination"), S.Character->GetActorLocation().Z > BeforeMove.Z);

            S.Climbing->LargeTransferDistance = 400.f;
            S.Climbing->LargeTransferHeightDrop = 100.f;
            S.Climbing->LargeTransferArcHeight = 85.f;
            S.Climbing->LargeTransferUpwardArcHeight = 100.f;
            Grab();
            S.Climbing->bEnableLargeTransfers = false;
            S.Climbing->JumpOrTransfer(FVector2D(Axis, 0));
            TestFalse(TEXT("Disabled fallback jumps off when normal target is absent"), S.Climbing->IsClimbing());
            S.Climbing->bEnableLargeTransfers = true;
            Grab();
            // Add a normal landing while keeping the large destination available.
            ASpyroClimbSurface* NormalDestination = S.World->SpawnActor<ASpyroClimbSurface>(
                Frame.Origin + Frame.Right * (200.f * Axis), S.Wall->GetActorRotation());
            NormalDestination->GeometryMode = S.Wall->GeometryMode;
            NormalDestination->Width = 80.f;
            NormalDestination->Height = 300.f;
            NormalDestination->RefreshSurface();
            S.Climbing->JumpOrTransfer(FVector2D(Axis, 0));
            TestTrue(TEXT("Normal transfer always wins when available"), S.Climbing->IsTransferring() && !S.Climbing->IsLargeTransfer());
            TestEqual(TEXT("Normal target chosen ahead of farther lower surface"), S.Climbing->GetTransferTarget(), NormalDestination);
            S.Climbing->CancelClimbing();
            NormalDestination->Destroy();

            if (Pitch == 20.f)
            {
                Grab();
                S.Climbing->LargeTransferHeightDrop = 1.f;
                const FVector Desired = Start + Frame.Right * (400.f * Axis) - Frame.Up * (FMath::Abs(Axis) / Frame.Up.Z);
                Destination->SetActorLocation(Frame.Origin + (Desired - Start) + Frame.Normal * 40.f);
                FVector Projected;
                FSpyroClimbFrame ProjectedFrame;
                TestTrue(TEXT("Depth correction can otherwise produce a higher tilted landing"), Destination->FindTransferLanding(
                    Desired, Frame.Normal, 100.f, S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                    S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), S.Climbing->WallGap,
                    S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius() * .5f, Projected, ProjectedFrame)
                    && Projected.Z > Start.Z);
                S.Climbing->JumpOrTransfer(FVector2D(Axis, 0));
                TestFalse(TEXT("Higher actual landing rejected after projection"), S.Climbing->IsClimbing());
            }
        }
    }
    // Preflight and in-flight collision, interruption cleanup, and duration clamping on the long path.
    FClimbTestScene S;
    S.Wall->Width = 140.f;
    S.Wall->RefreshSurface();
    S.Climbing->HorizontalJumpDistance = 200.f;
    S.Climbing->LargeTransferDistance = 400.f;
    S.Climbing->LargeTransferHeightDrop = 100.f;
    S.Climbing->LargeTransferLandingSearchTolerance = 0.f;
    ASpyroClimbSurface* Destination = S.World->SpawnActor<ASpyroClimbSurface>(FVector(0, -400, 200), FRotator::ZeroRotator);
    const auto Grab = [&]() { S.Reset(); return S.Climbing->TryStartClimbing(S.Wall); };
    Grab();
    const FVector Start = S.Character->GetActorLocation();
    const FVector ArcCentre = Start + FVector(85, -200, 50);
    UBoxComponent* Blocker = S.Obstacle(ArcCentre, FVector(10, 10, 10));
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestFalse(TEXT("Blocked large arc falls back to jump-off"), S.Climbing->IsClimbing());
    Blocker->GetOwner()->Destroy();
    Grab();
    S.Climbing->MaxTransferDuration = 2.f;
    S.Climbing->LargeTransferMinDuration = S.Climbing->LargeTransferMaxDuration = .1f;
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestTrue(TEXT("Large duration remains longer even with inconsistent settings"), S.Climbing->IsLargeTransfer()
        && S.Climbing->TransferDuration > 2.f);
    Blocker = S.Obstacle(ArcCentre, FVector(10, 10, 10));
    S.Step(FVector2D::ZeroVector, 4.f);
    TestFalse(TEXT("Large arc is swept during a hitch"), S.Climbing->IsClimbing());
    TestFalse(TEXT("Obstacle clears large-transfer flag"), S.Climbing->IsLargeTransfer());
    Blocker->GetOwner()->Destroy();
    Grab();
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    S.Character->LaunchCharacter(FVector(123, 45, 67), true, true);
    S.Climbing->NotifyDamaged();
    TestFalse(TEXT("Damage cancels large transfer"), S.Climbing->IsClimbing() || S.Climbing->IsLargeTransfer());
    TestTrue(TEXT("Damage launch survives large transfer cancellation"), S.Movement->PendingLaunchVelocity.Equals(FVector(123, 45, 67)));
    Grab();
    Destination->Destroy();
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestFalse(TEXT("Both searches empty still jump off"), S.Climbing->IsClimbing());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingTransferExitTest, "SpyroClimbing.Transfer.FallbackAndInterruptions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingTransferExitTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    S.Wall->Width = S.Wall->Height = 2000.f;
    S.Wall->RefreshSurface();
    S.Climbing->VerticalJumpDistance = 200.f;
    S.Climbing->LandingSearchTolerance = 0.f;
    const auto Grab = [&]() { S.Reset(); return S.Climbing->TryStartClimbing(S.Wall); };
    const auto Hop = [&]()
    {
        TestTrue(TEXT("Reattach before interruption test"), Grab());
        S.Climbing->JumpOrTransfer(FVector2D(0, 1));
        TestTrue(TEXT("Transfer active before interruption test"), S.Climbing->IsTransferring());
    };
    for (const FVector2D Input : { FVector2D::ZeroVector, FVector2D(.1f, -.1f) })
    {
        Grab();
        TestTrue(TEXT("Neutral/drift jump is handled"), S.Climbing->JumpOrTransfer(Input));
        TestFalse(TEXT("Neutral/drift exits climbing"), S.Climbing->IsClimbing());
        TestTrue(TEXT("Fallback queues outward/upward launch"), S.Movement->PendingLaunchVelocity.Equals(
            FVector(S.Climbing->JumpAwaySpeed, 0, S.Climbing->JumpUpSpeed), .01f));
    }
    S.Wall->Height = 600.f;
    S.Wall->RefreshSurface();
    S.Climbing->VerticalJumpDistance = 400.f;
    Grab();
    S.Climbing->JumpOrTransfer(FVector2D(0, 1));
    TestFalse(TEXT("No directional landing jumps off"), S.Climbing->IsClimbing());
    S.Climbing->VerticalJumpDistance = 250.f;
    S.Climbing->LandingSearchTolerance = 40.f;
    Grab();
    const FVector EdgeStart = S.Character->GetActorLocation();
    S.Climbing->JumpOrTransfer(FVector2D(0, 1));
    TestTrue(TEXT("Small edge adjustment finds full capsule support"), S.Climbing->IsTransferring());
    TestTrue(TEXT("Edge adjustment only shortens reach"), S.Climbing->TransferTargetLocation.Z <= EdgeStart.Z + 250.f
        && S.Climbing->TransferTargetLocation.Z >= EdgeStart.Z + 210.f);
    S.Step(FVector2D::ZeroVector, 1.f);
    TestTrue(TEXT("Adjusted landing remains supported"), S.Climbing->IsClimbing());

    S.Wall->Height = 2000.f;
    S.Wall->RefreshSurface();
    S.Climbing->VerticalJumpDistance = 200.f;
    S.Climbing->LandingSearchTolerance = 0.f;
    Grab();
    const FVector Start = S.Character->GetActorLocation();
    UBoxComponent* Blocker = S.Obstacle(Start + FVector(S.Climbing->TransferArcHeight, 0, 100), FVector(5, 70, 5));
    S.Climbing->JumpOrTransfer(FVector2D(0, 1));
    TestFalse(TEXT("Blocked arc rejects transfer before departure"), S.Climbing->IsClimbing());
    Blocker->GetOwner()->Destroy();
    Hop();
    Blocker = S.Obstacle(Start + FVector(S.Climbing->TransferArcHeight, 0, 100), FVector(5, 70, 5));
    S.Step(FVector2D::ZeroVector, 1.f);
    TestFalse(TEXT("Obstacle appearing in flight interrupts transfer"), S.Climbing->IsClimbing());
    TestTrue(TEXT("Blocked flight restores movement"), S.Movement->IsComponentTickEnabled());
    Blocker->GetOwner()->Destroy();

    Hop();
    S.Step(FVector2D::ZeroVector, .1f);
    const FVector Knockback(123, 456, 789);
    S.Character->LaunchCharacter(Knockback, true, true);
    S.Climbing->NotifyDamaged();
    TestFalse(TEXT("Damage cancels airborne transfer"), S.Climbing->IsTransferring() || S.Climbing->IsClimbing());
    TestTrue(TEXT("Damage launch survives transfer cancellation"), S.Movement->PendingLaunchVelocity.Equals(Knockback));
    Hop();
    S.Movement->SetMovementMode(MOVE_None);
    S.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("External disabled mode interrupts transfer"), S.Climbing->IsClimbing());
    TestEqual(TEXT("Death movement mode preserved"), S.Movement->MovementMode, TEnumAsByte<EMovementMode>(MOVE_None));
    Hop();
    S.Character->AddActorWorldOffset(FVector(0, 0, 10));
    S.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("Teleport interrupts guidance"), S.Climbing->IsClimbing());
    Hop();
    S.Wall->AddActorWorldOffset(FVector(0, 0, 1));
    S.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("Moving destination interrupts transfer"), S.Climbing->IsClimbing());
    S.Wall->AddActorWorldOffset(FVector(0, 0, -1));
    Hop();
    S.Wall->bClimbable = false;
    S.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("Disabled landing surface interrupts transfer"), S.Climbing->IsClimbing());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingTransferSurfaceTest, "SpyroClimbing.Transfer.SeparateSurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingTransferSurfaceTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    ASpyroClimbSurface* Other = S.World->SpawnActor<ASpyroClimbSurface>(FVector(-30, -450, 300), FRotator(0, 10, 0));
    S.Climbing->HorizontalJumpDistance = 450.f;
    S.Climbing->LandingSearchTolerance = 0.f;
    S.Climbing->TryStartClimbing(S.Wall);
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    if (!TestTrue(TEXT("Finds adjacent actor with a different yaw/depth"), S.Climbing->IsTransferring())) { return false; }
    TestEqual(TEXT("Departure remains current until landing"), S.Climbing->GetCurrentSurface(), S.Wall);
    TestEqual(TEXT("Correct reserved target"), S.Climbing->GetTransferTarget(), Other);
    const FVector Landing = S.Climbing->TransferTargetLocation;
    S.Wall->Destroy();
    S.Step(FVector2D::ZeroVector, 1.f);
    TestTrue(TEXT("Departure destruction does not cancel an airborne jump to another actor"), S.Climbing->IsClimbing());
    TestEqual(TEXT("Destination becomes current"), S.Climbing->GetCurrentSurface(), Other);
    TestTrue(TEXT("Lands on reserved point"), S.Character->GetActorLocation().Equals(Landing, .01f));
    TestTrue(TEXT("Yaw matches destination"), S.Character->GetActorForwardVector().Equals(-Other->GetOutwardNormal(), .001f));
    S.Step(FVector2D(0, 1), .1f);
    TestTrue(TEXT("Climbs on destination frame"), S.Character->GetActorLocation().Z > Landing.Z + 9.f);
    S.Climbing->JumpOrTransfer(FVector2D(0, .5f));
    TestTrue(TEXT("Can hop again after arrival"), S.Climbing->IsTransferring());
    Other->Destroy();
    S.Step(FVector2D::ZeroVector, .1f);
    TestFalse(TEXT("Destroyed destination releases airborne character"), S.Climbing->IsClimbing());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingMergedTransferTest, "SpyroClimbing.Project.AutumnPlainsTransfers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingMergedTransferTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    S.Climbing->bEnableLargeTransfers = false; // Normal reach/edge-margin regression below.
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Experimental_Mechanics/Climbing/Test_Level/Autumn_Plains_Climbing_Surfaces_Merged.Autumn_Plains_Climbing_Surfaces_Merged"));
    if (!TestNotNull(TEXT("Imported merged climbing mesh"), Mesh)) { return false; }
    S.Wall->SetActorLocation(FVector::ZeroVector);
    S.Wall->ResetWallMeshTransform();
    S.Wall->WallMesh->SetStaticMesh(Mesh);
    // Interior point on the lower of the two adjacent panels, far from their shared pivot.
    const FVector Centre(9446.706f, -14535.568f, 3788.831f);
    const FVector Normal = FVector(-.994f, -.112f, 0).GetSafeNormal();
    S.Character->SetActorLocationAndRotation(Centre + Normal * 70.f, (-Normal).Rotation());
    if (!TestTrue(TEXT("Grabs lower offset panel"), S.Climbing->TryStartClimbingNearby())) { return false; }
    const FVector Start = S.Character->GetActorLocation();
    S.Climbing->VerticalJumpDistance = 200.f;
    S.Climbing->HorizontalJumpDistance = 350.f;
    S.Climbing->LandingSearchTolerance = 0.f;
    S.Climbing->JumpOrTransfer(FVector2D(0, .75f));
    TestTrue(TEXT("Hop along the same imported panel"), S.Climbing->IsTransferring());
    S.Step(FVector2D::ZeroVector, 1.f);
    TestTrue(TEXT("Analog vertical hop advances 150cm"), S.Character->GetActorLocation().Equals(Start + FVector(0, 0, 150), .1f));
    TestTrue(TEXT("Same-panel landing stays climbing"), S.Climbing->IsClimbing());
    const FVector BeforeCrossing = S.Character->GetActorLocation();
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    if (!TestTrue(TEXT("Crosses empty gap to another patch in SAME actor/mesh"), S.Climbing->IsTransferring())) { return false; }
    TestEqual(TEXT("Cross-gap target is the same surface actor"), S.Climbing->GetTransferTarget(), S.Wall);
    const FVector Target = S.Climbing->TransferTargetLocation;
    TestTrue(TEXT("Landing is on the other panel, not the departure edge"), Target.Y > BeforeCrossing.Y + 300.f);
    S.Step(FVector2D::ZeroVector, 1.f);
    TestTrue(TEXT("Cross-gap transfer lands"), S.Climbing->IsClimbing() && !S.Climbing->IsTransferring());
    TestTrue(TEXT("Cross-gap reserved position reached"), S.Character->GetActorLocation().Equals(Target, .1f));
    S.Climbing->JumpOrTransfer(FVector2D(-1, 0));
    TestTrue(TEXT("Can transfer left back across the same mesh gap"), S.Climbing->IsTransferring());
    S.Step(FVector2D::ZeroVector, 1.f);
    TestTrue(TEXT("Return transfer lands"), S.Climbing->IsClimbing());
    S.Climbing->JumpOrTransfer(FVector2D(.45f, 0));
    TestFalse(TEXT("Partial input cannot auto-extend across an out-of-range gap"), S.Climbing->IsClimbing());

    // The user's shorter jump is possible when departure and landing can approach the panel edges.
    S.Character->SetActorLocationAndRotation(Centre + FVector(0, 0, 150) + Normal * 70.f, (-Normal).Rotation());
    S.Movement->SetMovementMode(MOVE_Falling);
    S.Climbing->HorizontalEdgeMargin = .5f;
    S.Climbing->HorizontalJumpDistance = 150.f;
    if (!TestTrue(TEXT("Regrab merged panel for edge jump"), S.Climbing->TryStartClimbingNearby())) { return false; }
    S.Step(FVector2D(1, 0), 10.f);
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestTrue(TEXT("150cm jump crosses merged-panel gap with half-radius margins"), S.Climbing->IsTransferring());
    S.Step(FVector2D::ZeroVector, 1.f);
    S.Step(FVector2D::ZeroVector, .1f);
    TestTrue(TEXT("Narrow-margin merged landing remains climbing"), S.Climbing->IsClimbing());

    // The second search must also work across disconnected patches of this same imported mesh.
    S.Climbing->CancelClimbing();
    S.Character->SetActorLocationAndRotation(Centre + FVector(0, 0, 150) + Normal * 70.f, (-Normal).Rotation());
    S.Climbing->HorizontalJumpDistance = 30.f;
    S.Climbing->bEnableLargeTransfers = true;
    S.Climbing->LargeTransferDistance = 150.f;
    S.Climbing->LargeTransferHeightDrop = 60.f;
    S.Climbing->LargeTransferLandingSearchTolerance = 0.f;
    TestTrue(TEXT("Regrab for large merged-mesh transfer"), S.Climbing->TryStartClimbingNearby());
    S.Step(FVector2D(1, 0), 10.f);
    const float LargeStartHeight = S.Character->GetActorLocation().Z;
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestTrue(TEXT("Large transfer reaches lower patch in same mesh"), S.Climbing->IsLargeTransfer());
    TestEqual(TEXT("Large merged target uses same actor"), S.Climbing->GetTransferTarget(), S.Wall);
    TestTrue(TEXT("Large merged landing is lower"), S.Climbing->TransferTargetLocation.Z < LargeStartHeight);
    S.Step(FVector2D::ZeroVector, S.Climbing->TransferDuration + .1f);
    TestTrue(TEXT("Large merged transfer lands"), S.Climbing->IsClimbing() && !S.Climbing->IsTransferring());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingEdgeMarginTest, "SpyroClimbing.HorizontalEdgeMargin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingEdgeMarginTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    for (const ESpyroClimbGeometry Geometry : { ESpyroClimbGeometry::Rectangle, ESpyroClimbGeometry::MeshCollision })
    {
        FClimbTestScene S;
        TestEqual(TEXT("Default margin is halfway to capsule edge"), S.Climbing->HorizontalEdgeMargin, .5f);
        S.Wall->GeometryMode = Geometry;
        S.Wall->SetActorLocationAndRotation(FVector(5000, 2000, 300), FRotator(0, 37, 0));
        const FVector Centre = S.Wall->GetActorLocation();
        const FVector Normal = S.Wall->GetOutwardNormal();
        const FVector Right = S.Wall->GetClimbRight();
        const auto Approach = [&](float Side)
        {
            S.Climbing->CancelClimbing();
            S.Character->SetActorLocationAndRotation(Centre + Right * Side + Normal * 100.f, (-Normal).Rotation());
            S.Movement->SetMovementMode(MOVE_Falling);
        };
        for (float Scale : { 1.f, 1.5f })
        {
            S.Climbing->CancelClimbing();
            S.Character->SetActorScale3D(FVector(Scale));
            const float Radius = S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
            const float HalfHeight = S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            for (float Margin : { 1.f, .5f, 0.f, -1.f, 2.f })
            {
                S.Climbing->HorizontalEdgeMargin = Margin;
                Approach(0.f);
                if (!TestTrue(TEXT("Grab with chosen edge margin"), S.Climbing->TryStartClimbing(S.Wall))) { return false; }
                const float SideLimit = S.Wall->Width * .5f - Radius * FMath::Clamp(Margin, 0.f, 1.f);
                S.Step(FVector2D(1, 0), 10.f);
                TestTrue(TEXT("Right limit follows scaled margin"), FMath::IsNearlyEqual(
                    FVector::DotProduct(S.Character->GetActorLocation() - Centre, Right), SideLimit, .15f));
                S.Step(FVector2D::ZeroVector, .1f);
                TestTrue(TEXT("Stable at edge after input release"), S.Climbing->IsClimbing());
                S.Step(FVector2D(0, 1), 10.f);
                TestTrue(TEXT("Top still requires full capsule height"), FMath::IsNearlyEqual(
                    S.Character->GetActorLocation().Z, Centre.Z + S.Wall->Height * .5f - HalfHeight, .15f));
                S.Step(FVector2D(-1, 0), 10.f);
                TestTrue(TEXT("Left limit follows scaled margin"), FMath::IsNearlyEqual(
                    FVector::DotProduct(S.Character->GetActorLocation() - Centre, Right), -SideLimit, .15f));
                S.Step(FVector2D(0, -1), 10.f);
                TestTrue(TEXT("Bottom still requires full capsule height"), FMath::IsNearlyEqual(
                    S.Character->GetActorLocation().Z, Centre.Z - S.Wall->Height * .5f + HalfHeight, .15f));
                TestTrue(TEXT("Wall clearance uses full physical radius"), FMath::IsNearlyEqual(
                    FVector::DotProduct(S.Character->GetActorLocation() - Centre, Normal), Radius + S.Climbing->WallGap, .15f));
                TestEqual(TEXT("Physical capsule is not resized"), S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius(), Radius);
            }
        }
        S.Character->SetActorScale3D(FVector::OneVector);
        const float Radius = S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float NearEdge = S.Wall->Width * .5f - Radius * .5f - 2.f;
        S.Climbing->HorizontalEdgeMargin = 1.f;
        Approach(NearEdge);
        TestFalse(TEXT("Original margin rejects partial-overhang entry"), S.Climbing->TryStartClimbing(S.Wall));
        S.Climbing->HorizontalEdgeMargin = .5f;
        TestTrue(TEXT("Half margin accepts same entry"), S.Climbing->TryStartClimbing(S.Wall));
        S.Climbing->HorizontalEdgeMargin = 0.f;
        Approach(S.Wall->Width * .5f + 2.f);
        TestFalse(TEXT("Centre outside the panel still cannot grab"), S.Climbing->TryStartClimbing(S.Wall));
        Approach(NearEdge);
        const FVector Snap = Centre + Right * NearEdge + Normal * (Radius + S.Climbing->WallGap);
        S.Obstacle(Snap + Right * (Radius * .8f), FVector(2, 2, 10));
        TestFalse(TEXT("Zero support margin still checks full capsule collision on entry"), S.Climbing->TryStartClimbing(S.Wall));
    }

    FClimbTestScene S;
    ASpyroClimbSurface* Other = S.World->SpawnActor<ASpyroClimbSurface>(FVector(0, -450, 300), FRotator::ZeroRotator);
    S.Climbing->HorizontalJumpDistance = 100.f;
    S.Climbing->LandingSearchTolerance = 0.f;
    for (float Margin : { 1.f, .5f, 0.f })
    {
        S.Reset();
        S.Climbing->HorizontalEdgeMargin = Margin;
        TestTrue(TEXT("Grab before edge transfer"), S.Climbing->TryStartClimbing(S.Wall));
        S.Step(FVector2D(1, 0), 10.f);
        S.Climbing->JumpOrTransfer(FVector2D(1, 0));
        if (Margin == 1.f)
        {
            TestFalse(TEXT("100cm cannot bridge gap with original full-radius margins"), S.Climbing->IsClimbing());
        }
        else
        {
            TestTrue(TEXT("Reduced margins allow shorter transfer"), S.Climbing->IsTransferring());
            S.Step(FVector2D::ZeroVector, 1.f);
            S.Step(FVector2D::ZeroVector, .1f);
            TestTrue(TEXT("Transfer validation accepts relaxed landing"), S.Climbing->IsClimbing());
            TestEqual(TEXT("Lands on receiving surface"), S.Climbing->GetCurrentSurface(), Other);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingMagmaConeTest, "SpyroClimbing.Project.MagmaConeMesh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingMagmaConeTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    FClimbTestScene S;
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Experimental_Mechanics/Climbing/Test_Level_2/Magma_Cone_Climbing_Surfaces.Magma_Cone_Climbing_Surfaces"));
    if (!TestNotNull(TEXT("Magma Cone imported mesh"), Mesh) || !TestNotNull(TEXT("Mesh render data"), Mesh->GetRenderData())) { return false; }
    S.Wall->SetActorLocation(FVector::ZeroVector);
    S.Wall->ResetWallMeshTransform();
    S.Wall->WallMesh->SetStaticMesh(Mesh);
    AddInfo(FString::Printf(TEXT("Magma Cone bounds: %s, collision complexity: %d"),
        *Mesh->GetBounds().ToString(), int32(Mesh->GetBodySetup()->CollisionTraceFlag)));
    struct FPanel { FVector Normal; FVector Origin; TArray<FVector> Vertices; };
    TArray<FPanel> Panels;
    const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
    for (int32 I = 0; I < LOD.IndexBuffer.GetNumIndices(); I += 3)
    {
        const FVector A = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I));
        const FVector B = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I + 1));
        const FVector C = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I + 2));
        const FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (Normal.IsNearlyZero()) { continue; }
        int32 Group = Panels.IndexOfByPredicate([&](const FPanel& P)
        {
            return FMath::Abs(FVector::DotProduct(P.Normal, Normal)) > .99999f
                && FMath::Abs(FVector::DotProduct(A - P.Origin, P.Normal)) < .1f;
        });
        if (Group == INDEX_NONE)
        {
            FPanel Panel; Panel.Normal = Normal; Panel.Origin = A;
            Group = Panels.Add(Panel);
        }
        Panels[Group].Vertices.Append({ A, B, C });
    }
    AddInfo(FString::Printf(TEXT("Magma Cone distinct planes: %d"), Panels.Num()));
    for (int32 I = 0; I < Panels.Num(); ++I)
    {
        const FPanel& Panel = Panels[I];
        FSpyroClimbFrame Frame;
        Frame.Origin = Panel.Origin;
        Frame.Normal = Panel.Normal;
        Frame.Right = FVector::CrossProduct(Frame.Normal, FVector::UpVector).GetSafeNormal();
        Frame.Up = FVector::CrossProduct(Frame.Right, Frame.Normal).GetSafeNormal();
        FVector2D Min(MAX_flt, MAX_flt), Max(-MAX_flt, -MAX_flt);
        for (const FVector& V : Panel.Vertices)
        {
            const FVector2D XY = Frame.ToCoordinates(V);
            Min.X = FMath::Min(Min.X, XY.X); Min.Y = FMath::Min(Min.Y, XY.Y);
            Max.X = FMath::Max(Max.X, XY.X); Max.Y = FMath::Max(Max.Y, XY.Y);
        }
        const FVector Centre = Frame.ToWorld((Min + Max) * .5f, 0.f);
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroMagmaMesh), true);
        bool bHit = S.Wall->WallMesh->LineTraceComponent(Hit, Centre + Frame.Normal * 200.f, Centre - Frame.Normal * 200.f, Params);
        if (!bHit) { bHit = S.Wall->WallMesh->LineTraceComponent(Hit, Centre - Frame.Normal * 200.f, Centre + Frame.Normal * 200.f, Params); }
        AddInfo(FString::Printf(TEXT("Panel %d: centre %s, normal %s, tilt %.3f degrees, size %.1f x %.1f, collision %d"),
            I + 1, *Centre.ToString(), *Frame.Normal.ToString(), FMath::RadiansToDegrees(FMath::Asin(FMath::Abs(Frame.Normal.Z))),
            Max.X - Min.X, Max.Y - Min.Y, bHit));
        if (!TestTrue(TEXT("Magma panel has collision"), bHit)) { continue; }
        const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
        const float Radius = S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float HalfHeight = S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // Bottom sliver triangles are shorter than the capsule; they need not be valid entry points.
        if (Max.Y - Min.Y < 2.f * HalfHeight) { continue; }
        const float Offset = Radius + (HalfHeight - Radius) * FMath::Abs(Normal.Z) + S.Climbing->WallGap;
        S.Climbing->CancelClimbing();
        S.Character->SetActorLocationAndRotation(Centre + Normal * (Offset + 20.f), (-Normal).GetSafeNormal2D().Rotation());
        S.Movement->SetMovementMode(MOVE_Falling);
        const bool bGrabbed = S.Climbing->TryStartClimbingNearby();
        TestTrue(FString::Printf(TEXT("Magma panel %d can be grabbed"), I + 1), bGrabbed);
        if (!bGrabbed)
        {
            FSpyroClimbFrame Found;
            const FVector Location = S.Character->GetActorLocation();
            const bool bFound = S.Wall->FindClimbFrame(Location, S.Character->GetActorForwardVector(), HalfHeight + 70.f, .5f, Found);
            const FVector Target = Found.ToWorld(Found.ToCoordinates(Location), Found.CapsuleStandOff(Radius, HalfHeight, S.Climbing->WallGap));
            const bool bSupport = S.Wall->SupportsPosition(Target, Found, Radius * S.Climbing->HorizontalEdgeMargin, HalfHeight);
            FCollisionQueryParams ClearanceParams(SCENE_QUERY_STAT(SpyroMagmaClearance), false, S.Character);
            const bool bBlocked = S.World->OverlapBlockingTestByChannel(Target, FQuat::Identity, ECC_Pawn,
                S.Character->GetCapsuleComponent()->GetCollisionShape(), ClearanceParams);
            AddInfo(FString::Printf(TEXT("Failed %d: found %d, support %d, blocked %d, intended normal %s, found normal %s, plane distance %.1f"),
                I + 1, bFound, bSupport, bBlocked, *Normal.ToString(), *Found.Normal.ToString(), FVector::DotProduct(Location - Found.Origin, Found.Normal)));
        }
        else
        {
            TestTrue(TEXT("Magma entry keeps upright capsule"), S.Character->GetActorUpVector().Equals(FVector::UpVector, .001f));
            const FVector Entry = S.Character->GetActorLocation();
            S.Step(FVector2D(0, 1), .2f);
            TestTrue(TEXT("Magma climbing gains height"), S.Character->GetActorLocation().Z > Entry.Z + 10.f);
            TestEqual(TEXT("Tilt corrections cannot select sideways/down animation"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Up);
            S.Step(FVector2D(0, -1), .2f);
            TestEqual(TEXT("Down animation on Magma mesh"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Down);
            S.Step(FVector2D(1, 0), .1f);
            TestEqual(TEXT("Right animation on Magma mesh"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Right);
            S.Step(FVector2D(-1, 0), .1f);
            TestEqual(TEXT("Left animation on Magma mesh"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Left);
            S.Step(FVector2D::ZeroVector, .1f);
            TestTrue(TEXT("Magma remains attached after four-way movement"), S.Climbing->IsClimbing());
            if (I == 9 || I == 37)
            {
                const FVector BeforeLongClimb = S.Character->GetActorLocation();
                for (int32 Tick = 0; Tick < 100; ++Tick)
                {
                    S.Step(FVector2D(0, 1), .05f);
                    TestEqual(TEXT("Up animation remains stable across Magma triangle seams"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Up);
                }
                TestTrue(FString::Printf(TEXT("Magma panel %d climbs through changing triangle slopes"), I + 1),
                    S.Climbing->IsClimbing() && S.Character->GetActorLocation().Z > BeforeLongClimb.Z + 400.f);
                for (int32 Tick = 0; Tick < 100; ++Tick)
                {
                    S.Step(FVector2D(0, -1), .05f);
                    TestEqual(TEXT("Down animation remains stable across Magma triangle seams"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Down);
                }
                TestTrue(TEXT("Downward traversal stays attached"), S.Climbing->IsClimbing());
                S.Climbing->VerticalJumpDistance = 150.f;
                S.Climbing->JumpOrTransfer(FVector2D(0, 1));
                TestTrue(TEXT("Tilted Magma surface allows upward transfer"), S.Climbing->IsTransferring());
                S.Step(FVector2D::ZeroVector, 1.f);
                TestTrue(TEXT("Tilted Magma transfer lands"), S.Climbing->IsClimbing() && !S.Climbing->IsTransferring());
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingTiltedMeshTest, "SpyroClimbing.TiltedMeshMovementAndTransfers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingTiltedMeshTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    for (float Pitch : { -20.f, 20.f, 44.f })
    {
        FClimbTestScene S;
        S.Wall->GeometryMode = ESpyroClimbGeometry::MeshCollision;
        S.Wall->Width = S.Wall->Height = 1200.f;
        S.Wall->SetActorRotation(FRotator(Pitch, 37, 0));
        S.Wall->RefreshSurface();
        FSpyroClimbFrame Frame;
        Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
        const float Radius = S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float HalfHeight = S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float StandOff = Radius + (HalfHeight - Radius) * FMath::Abs(Frame.Normal.Z) + S.Climbing->WallGap;
        S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, StandOff + 20.f), Frame.FacingRotation());
        if (!TestTrue(TEXT("Grab a tilted mesh with upright capsule"), S.Climbing->TryStartClimbingNearby())) { continue; }
        TestTrue(TEXT("Tilt uses full normal capsule extent"), FMath::IsNearlyEqual(
            FVector::DotProduct(S.Character->GetActorLocation() - Frame.Origin, Frame.Normal), StandOff, .1f));
        FVector Before = S.Character->GetActorLocation();
        S.Step(FVector2D(0, 1), .5f);
        TestTrue(TEXT("Up follows slope at configured speed"), (S.Character->GetActorLocation() - Before).Equals(Frame.Up * 50.f, .1f));
        Before = S.Character->GetActorLocation();
        S.Step(FVector2D(1, 0), .5f);
        TestTrue(TEXT("Right remains horizontal along tilted wall"), (S.Character->GetActorLocation() - Before).Equals(Frame.Right * 50.f, .1f));
        TestTrue(TEXT("Ordinary movement leaves capsule upright"), S.Character->GetActorUpVector().Equals(FVector::UpVector, .001f));
        Before = S.Character->GetActorLocation();
        S.Climbing->JumpOrTransfer(FVector2D(0, .5f));
        TestTrue(TEXT("Hop on tilted plane starts"), S.Climbing->IsTransferring());
        TestTrue(TEXT("Hop distance follows slope"), S.Climbing->TransferTargetLocation.Equals(
            Before + Frame.Up * (S.Climbing->VerticalJumpDistance * .5f), .1f));
        const FVector Landing = S.Climbing->TransferTargetLocation;
        const float Duration = S.Climbing->TransferDuration;
        for (int32 Step = 0; Step < 21; ++Step)
        {
            S.Step(FVector2D::ZeroVector, Duration / 20.f);
            TestTrue(TEXT("Transfer keeps capsule upright"), S.Character->GetActorUpVector().Equals(FVector::UpVector, .001f));
            FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroTiltedClearance), false, S.Character);
            TestFalse(TEXT("Tilted hop does not penetrate wall"), S.World->OverlapBlockingTestByChannel(
                S.Character->GetActorLocation(), FQuat::Identity, ECC_Pawn, S.Character->GetCapsuleComponent()->GetCollisionShape(), Params));
        }
        TestTrue(TEXT("Tilted hop lands and remains climbing"), S.Climbing->IsClimbing() && !S.Climbing->IsTransferring());
        TestTrue(TEXT("Tilted hop lands at reserved point"), S.Character->GetActorLocation().Equals(Landing, .1f));
        S.Step(FVector2D(0, -1), .5f);
        TestTrue(TEXT("Downward movement follows slope after landing"), S.Character->GetActorLocation().Equals(Landing - Frame.Up * 50.f, .1f));
        S.Climbing->JumpOff();
        TestTrue(TEXT("Jump-off uses full tilted normal"), S.Movement->PendingLaunchVelocity.Equals(
            Frame.Normal * S.Climbing->JumpAwaySpeed + FVector::UpVector * S.Climbing->JumpUpSpeed, .1f));

        S.Climbing->CancelClimbing();
        S.Wall->SetActorRotation(FRotator(65, 37, 0));
        Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
        S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, 110.f), Frame.FacingRotation());
        S.Movement->SetMovementMode(MOVE_Falling);
        TestFalse(TEXT("Floor-like slopes beyond supported tilt are rejected"), S.Climbing->TryStartClimbing(S.Wall));
    }
    // Different destination tilt needs a newly computed stand-off while retaining upright rotation.
    FClimbTestScene S;
    S.Wall->GeometryMode = ESpyroClimbGeometry::MeshCollision;
    S.Wall->SetActorRotation(FRotator(20, 0, 0));
    FSpyroClimbFrame Frame;
    Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
    S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, 100.f), Frame.FacingRotation());
    ASpyroClimbSurface* Destination = S.World->SpawnActor<ASpyroClimbSurface>(FVector(0, -450, 300), FRotator(-20, 0, 0));
    Destination->GeometryMode = ESpyroClimbGeometry::MeshCollision;
    S.Climbing->HorizontalJumpDistance = 450.f;
    TestTrue(TEXT("Grab source before transfer to different tilt"), S.Climbing->TryStartClimbing(S.Wall));
    S.Climbing->JumpOrTransfer(FVector2D(1, 0));
    TestEqual(TEXT("Reserves differently tilted destination"), S.Climbing->GetTransferTarget(), Destination);
    // The fixture's mesh has identity rotation. Mid-flight between opposite tilts should be upright.
    S.Step(FVector2D::ZeroVector, S.Climbing->TransferDuration * .5f);
    TestTrue(TEXT("Mesh blends between opposite wall tilts in flight"),
        S.Character->GetMesh()->GetUpVector().Equals(FVector::UpVector, .001f));
    S.Step(FVector2D::ZeroVector, 1.f);
    TestTrue(TEXT("Mesh faces destination slope on landing"),
        S.Character->GetMesh()->GetForwardVector().Equals(-Destination->GetActorForwardVector(), .001f));
    S.Step(FVector2D(0, 1), .1f);
    TestTrue(TEXT("Different-tilt landing remains valid for climbing"), S.Climbing->IsClimbing());
    TestTrue(TEXT("Different-tilt transfer keeps capsule upright"), S.Character->GetActorUpVector().Equals(FVector::UpVector, .001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingAnimationTest, "SpyroClimbing.AnimationDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingAnimationTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    for (const ESpyroClimbGeometry Geometry : { ESpyroClimbGeometry::Rectangle, ESpyroClimbGeometry::MeshCollision })
    {
        FClimbTestScene S;
        S.Wall->GeometryMode = Geometry;
        S.Wall->Width = S.Wall->Height = 2000.f;
        S.Wall->RefreshSurface();
        S.Wall->SetActorRotation(FRotator(Geometry == ESpyroClimbGeometry::MeshCollision ? 20.f : 0.f, 37, 0));
        FSpyroClimbFrame Frame;
        Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
        S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, 100.f), Frame.FacingRotation());
        TestEqual(TEXT("Inactive direction starts idle"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        if (!TestTrue(TEXT("Grab before animation checks"), S.Climbing->TryStartClimbing(S.Wall))) { continue; }
        TestEqual(TEXT("Entry starts idle"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        struct FDirectionCase { FVector2D Input; ESpyroClimbDirection Direction; float Speed; };
        const FDirectionCase Cases[] = {
            { FVector2D(1, 0), ESpyroClimbDirection::Right, 100.f },
            { FVector2D(-.25f, 0), ESpyroClimbDirection::Left, 25.f },
            { FVector2D(0, .5f), ESpyroClimbDirection::Up, 50.f },
            { FVector2D(0, -1), ESpyroClimbDirection::Down, 100.f },
            { FVector2D(.8f, .3f), ESpyroClimbDirection::Right, 80.f },
            { FVector2D(-1, 1), ESpyroClimbDirection::Up, 100.f }
        };
        for (const FDirectionCase& Case : Cases)
        {
            S.Step(Case.Input, .1f);
            TestEqual(TEXT("Animation follows the selected movement axis"), S.Climbing->ClimbDirection, Case.Direction);
            TestTrue(TEXT("Animation speed retains analog strength"), FMath::IsNearlyEqual(S.Climbing->ClimbAnimationSpeed, Case.Speed, .1f));
        }
        S.Step(FVector2D::ZeroVector, .1f);
        TestEqual(TEXT("Released input selects idle immediately"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        TestEqual(TEXT("Idle has zero animation speed"), S.Climbing->ClimbAnimationSpeed, 0.f);
        S.Climbing->HorizontalClimbingSpeed = 0.f;
        S.Step(FVector2D(1, 0), .1f);
        TestEqual(TEXT("Zero speed does not animate movement"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        S.Climbing->HorizontalClimbingSpeed = 2.f;
        S.Step(FVector2D(.4f, 0), .5f);
        TestEqual(TEXT("Sub-threshold movement cannot start flickering animation"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        S.Step(FVector2D(.6f, 0), .5f);
        TestEqual(TEXT("Motion above start threshold selects direction"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Right);
        S.Step(FVector2D(.3f, 0), .5f);
        TestEqual(TEXT("Hysteresis retains direction through small speed fluctuations"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Right);
        S.Step(FVector2D(.1f, 0), .5f);
        TestEqual(TEXT("Motion below stop threshold returns idle"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        S.Climbing->HorizontalClimbingSpeed = 100.f;
        for (int32 I = 0; I < 3; ++I) { S.Step(FVector2D(1, 0), 10.f); }
        TestEqual(TEXT("Holding into a side edge settles to idle"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        S.Step(FVector2D(-1, 0), .1f);
        TestEqual(TEXT("Reversing away from edge selects left immediately"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Left);
        S.Climbing->JumpOrTransfer(FVector2D(0, .5f));
        TestTrue(TEXT("Transfer starts during animation test"), S.Climbing->IsTransferring());
        TestEqual(TEXT("Transfer clears wall animation before takeoff"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        S.Step(FVector2D(1, 0), .1f);
        TestEqual(TEXT("Airborne movement does not select a wall animation"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        TestEqual(TEXT("Airborne wall-animation speed is zero"), S.Climbing->ClimbAnimationSpeed, 0.f);
        S.Step(FVector2D::ZeroVector, 1.f);
        TestTrue(TEXT("Animation test transfer landed"), S.Climbing->IsClimbing() && !S.Climbing->IsTransferring());
        TestEqual(TEXT("Landing starts idle"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        S.Step(FVector2D(0, -1), .1f);
        TestEqual(TEXT("Movement after landing updates direction"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Down);
        S.Climbing->NotifyDamaged();
        TestEqual(TEXT("Damage clears animation direction"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
        TestEqual(TEXT("Damage clears animation speed"), S.Climbing->ClimbAnimationSpeed, 0.f);
        S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, 100.f), Frame.FacingRotation());
        S.Climbing->TryStartClimbing(S.Wall);
        S.Step(FVector2D(0, 1), .1f);
        S.Obstacle(S.Character->GetActorLocation() + FVector(0, 0, 130), FVector(200, 200, 10));
        S.Step(FVector2D(0, 1), 1.f);
        for (int32 I = 0; I < 10; ++I)
        {
            S.Step(FVector2D(0, 1), 1.f / 60.f);
            TestTrue(TEXT("Obstacle retains wall climbing"), S.Climbing->IsClimbing());
            TestEqual(TEXT("Holding against collision stays idle"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
            TestEqual(TEXT("Blocking collision clears animation speed"), S.Climbing->ClimbAnimationSpeed, 0.f);
        }
        S.Climbing->JumpOff();
        TestEqual(TEXT("Jump-off clears animation direction"), S.Climbing->ClimbDirection, ESpyroClimbDirection::Idle);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpyroClimbingMeshAlignmentTest, "SpyroClimbing.MeshSurfaceAlignment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpyroClimbingMeshAlignmentTest::RunTest(const FString& Parameters)
{
    using namespace SpyroClimbingTests;
    // Deliberately non-default offsets/rotation/scale: imported character axes must be preserved.
    const FTransform Authored(FRotator(4, -90, 8), FVector(8, -5, -85), FVector(.7f, .8f, .9f));
    for (float Pitch : { -20.f, 0.f, 20.f, 44.f })
    {
        FClimbTestScene S;
        S.Wall->GeometryMode = ESpyroClimbGeometry::MeshCollision;
        S.Wall->Width = S.Wall->Height = 1200.f;
        S.Wall->SetActorRotation(FRotator(Pitch, 37, 0));
        S.Wall->RefreshSurface();
        S.Character->SetActorScale3D(FVector(1.2f));
        USkeletalMeshComponent* Mesh = S.Character->GetMesh();
        Mesh->SetRelativeTransform(Authored);
        FSpyroClimbFrame Frame;
        Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
        const float Radius = S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float Height = S.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector,
            Frame.CapsuleStandOff(Radius, Height, S.Climbing->WallGap) + 20.f), Frame.FacingRotation());
        if (!TestTrue(TEXT("Grab before mesh alignment checks"), S.Climbing->TryStartClimbing(S.Wall))) { continue; }

        const auto CheckAlignment = [&]()
        {
            const FQuat VisualFrame = Mesh->GetComponentQuat() * Authored.GetRotation().Inverse();
            TestTrue(TEXT("Mesh up follows wall tangent"), VisualFrame.GetUpVector().Equals(Frame.Up, .001f));
            TestTrue(TEXT("Mesh forward faces full wall normal"), VisualFrame.GetForwardVector().Equals(-Frame.Normal, .001f));
            TestTrue(TEXT("Capsule remains upright"), S.Character->GetActorUpVector().Equals(FVector::UpVector, .001f));
            const FVector Origin = Mesh->GetComponentLocation() - VisualFrame.RotateVector(Authored.GetLocation() * 1.2f);
            const FVector ExpectedOrigin = S.Character->GetActorLocation() - Frame.Normal * ((Height - Radius) * FMath::Abs(Frame.Normal.Z));
            TestTrue(TEXT("Mesh offset rotates about wall-aligned visual origin"), Origin.Equals(ExpectedOrigin, .05f));
            TestTrue(TEXT("Visual origin retains original radius plus wall gap"), FMath::IsNearlyEqual(
                FVector::DotProduct(Origin - Frame.Origin, Frame.Normal), Radius + S.Climbing->WallGap, .05f));
            TestTrue(TEXT("Authored mesh scale preserved"), Mesh->GetRelativeScale3D().Equals(Authored.GetScale3D(), .001f));
        };
        CheckAlignment();
        for (int32 Tick = 0; Tick < 20; ++Tick) { S.Step(FVector2D(0, 1), .05f); }
        CheckAlignment();
        const FTransform BeforeIdle = Mesh->GetComponentTransform();
        for (int32 Tick = 0; Tick < 20; ++Tick) { S.Step(FVector2D::ZeroVector, .05f); }
        TestTrue(TEXT("Mesh alignment does not accumulate while idle"), Mesh->GetComponentTransform().Equals(BeforeIdle, .01f));
        S.Climbing->bOrientMeshToSurface = false;
        S.Step(FVector2D::ZeroVector, .05f);
        TestTrue(TEXT("Disabling alignment restores authored transform"), Mesh->GetRelativeTransform().Equals(Authored, .001f));
        S.Climbing->bOrientMeshToSurface = true;
        S.Step(FVector2D::ZeroVector, .05f);
        CheckAlignment();
        S.Climbing->JumpOrTransfer(FVector2D(0, .5f));
        TestTrue(TEXT("Same-surface tilted transfer starts"), S.Climbing->IsTransferring());
        S.Step(FVector2D::ZeroVector, S.Climbing->TransferDuration * .5f);
        const FQuat InFlight = Mesh->GetComponentQuat() * Authored.GetRotation().Inverse();
        TestTrue(TEXT("Same-slope hop retains tilt in flight"), InFlight.GetUpVector().Equals(Frame.Up, .001f));
        S.Step(FVector2D::ZeroVector, 1.f);
        CheckAlignment();
        S.Climbing->CancelClimbing();
        TestTrue(TEXT("Exit restores exact mesh transform"), Mesh->GetRelativeTransform().Equals(Authored, .001f));
    }

    // Each exit must restore the mesh before another movement system takes over.
    for (int32 Exit = 0; Exit < 7; ++Exit)
    {
        FClimbTestScene S;
        S.Wall->GeometryMode = ESpyroClimbGeometry::MeshCollision;
        S.Wall->SetActorRotation(FRotator(20, 0, 0));
        FSpyroClimbFrame Frame;
        Frame.SetPlane(S.Wall->GetActorLocation(), S.Wall->GetActorForwardVector());
        S.Character->SetActorLocationAndRotation(Frame.ToWorld(FVector2D::ZeroVector, 100.f), Frame.FacingRotation());
        USkeletalMeshComponent* Mesh = S.Character->GetMesh();
        Mesh->SetRelativeTransform(Authored);
        if (!TestTrue(TEXT("Grab before mesh restoration check"), S.Climbing->TryStartClimbing(S.Wall))) { continue; }
        switch (Exit)
        {
        case 0: S.Climbing->JumpOff(); break;
        case 1: S.Climbing->NotifyDamaged(); break;
        case 2: S.Climbing->Deactivate(); break;
        case 3: S.Wall->Destroy(); S.Step(FVector2D::ZeroVector, .1f); break;
        case 4: S.Movement->SetMovementMode(MOVE_None); S.Step(FVector2D::ZeroVector, .1f); break;
        case 5:
            S.Climbing->JumpOrTransfer(FVector2D(0, .5f));
            S.Step(FVector2D::ZeroVector, .1f);
            S.Climbing->NotifyDamaged();
            break;
        case 6:
        {
            // Damage during a mesh transform must not leave a tilted mesh or overwrite knockback.
            bool bFired = false;
            const FDelegateHandle Handle = Mesh->TransformUpdated.AddLambda(
                [&](USceneComponent*, EUpdateTransformFlags, ETeleportType)
                {
                    if (bFired) { return; }
                    bFired = true;
                    S.Character->LaunchCharacter(FVector(123, 45, 67), true, true);
                    S.Climbing->NotifyDamaged();
                });
            S.Climbing->bOrientMeshToSurface = false;
            S.Step(FVector2D::ZeroVector, .1f);
            Mesh->TransformUpdated.Remove(Handle);
            TestTrue(TEXT("Mesh transform invoked damage callback"), bFired);
            TestTrue(TEXT("Mesh callback preserves damage launch"), S.Movement->PendingLaunchVelocity.Equals(FVector(123, 45, 67)));
            break;
        }
        }
        TestFalse(TEXT("Exit ends climbing"), S.Climbing->IsClimbing());
        TestTrue(TEXT("Every exit restores mesh offset rotation and scale"), Mesh->GetRelativeTransform().Equals(Authored, .001f));
    }
    // Opting out before entry should leave the mesh alone for Blueprint-owned presentation.
    FClimbTestScene S;
    S.Character->GetMesh()->SetRelativeTransform(Authored);
    S.Climbing->bOrientMeshToSurface = false;
    S.Climbing->TryStartClimbing(S.Wall);
    S.Step(FVector2D(0, 1), .1f);
    TestTrue(TEXT("Opt-out does not alter mesh"), S.Character->GetMesh()->GetRelativeTransform().Equals(Authored, .001f));
    S.Climbing->CancelClimbing();
    return true;
}

#endif
