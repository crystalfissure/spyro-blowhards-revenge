#pragma once

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"

namespace ToastyEncounterCollision
{
inline void IgnorePlayer(FCollisionQueryParams& Query, AActor* Player)
{
    if (!IsValid(Player)) return;
    Query.AddIgnoredActor(Player);
    TArray<AActor*> Children;
    Player->GetAllChildActors(Children, true);
    Query.AddIgnoredActors(Children);
}

// The root capsule is a terrain-query shape. Only the fitted body and its
// matching charge sensor participate in player and breath queries.
inline void Configure(ACharacter* Character, UBoxComponent* Body, UBoxComponent* Sensor)
{
    Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    for (UBoxComponent* Box : {Body, Sensor})
    {
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionObjectType(ECC_WorldDynamic);
        Box->SetCollisionResponseToChannel(ECC_GameTraceChannel4, Box == Body ? ECR_Block : ECR_Overlap);
        Box->SetGenerateOverlapEvents(Box == Sensor);
        Box->CanCharacterStepUpOn = ECB_No;
    }
    Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    // Native breath damage accepts only the primitive registered on Damageable.
    // The thin outer sensor is also the first hit of a single blanket trace.
    for (auto* Component : Character->GetComponents())
        if (Component->GetClass()->GetName() == TEXT("Damageable_Com_C"))
            if (auto* Target = FindFProperty<FObjectPropertyBase>(Component->GetClass(), TEXT("Object's Hitbox Component")))
                Target->SetObjectPropertyValue_InContainer(Component, Sensor);
}

inline void FitBody(USkeletalMeshComponent* Mesh, UBoxComponent* Body, UBoxComponent* Sensor, bool Costume)
{
    if (!Mesh || !Mesh->SkeletalMesh || !Body || !Sensor) return;
    const auto& Ref = Mesh->SkeletalMesh->GetRefSkeleton();
    const auto& Pose = Mesh->GetComponentSpaceTransforms();
    FBox Bounds(ForceInit);
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    {
        // The extractor's joints are the original model vertices. Omit its
        // armature helpers, and the costume's scythe, hands and broad hat brim.
        const FString Name = Ref.GetBoneName(I).ToString();
        const int32 JointAt = Name.Find(TEXT("joint"));
        if (JointAt == INDEX_NONE || !Pose.IsValidIndex(I)) continue;
        if (Costume)
        {
            // Vertex identities are shared by all three costume rigs. These
            // groups contain the neck, robe, legs, pumpkin head and feet.
            const int32 Joint = FCString::Atoi(*Name.Mid(JointAt + 5));
            if (!((Joint >= 10 && Joint <= 18) || (Joint >= 24 && Joint <= 29) ||
                (Joint >= 31 && Joint <= 38) || (Joint >= 49 && Joint <= 66) ||
                (Joint >= 76 && Joint <= 134))) continue;
        }
        Bounds += Pose[I].GetTranslation();
    }
    if (!Bounds.IsValid) return;
    const FVector Scale = Mesh->GetComponentScale().GetAbs();
    const FVector Extent = (Bounds.GetExtent() * Scale + FVector(2.f)).ComponentMax(FVector(5.f));
    const FVector Center = Mesh->GetComponentTransform().TransformPosition(Bounds.GetCenter());
    for (UBoxComponent* Box : {Body, Sensor})
    {
        Box->SetWorldScale3D(FVector::OneVector);
        Box->SetBoxExtent(Extent + (Box == Sensor ? FVector(1.f) : FVector::ZeroVector), false);
        Box->SetWorldLocationAndRotation(Center, Mesh->GetComponentQuat());
    }
}

inline bool TouchesPlayer(UBoxComponent* Body, AActor* Player)
{
    auto* Character = Cast<ACharacter>(Player);
    if (!Body || !Character) return false;
    FMTDResult Contact;
    return Character->GetCapsuleComponent()->ComputePenetration(Contact,
        FCollisionShape::MakeBox(Body->GetScaledBoxExtent() + FVector(1.f)),
        Body->GetComponentLocation(), Body->GetComponentQuat());
}

inline bool SeparatingContact(const FHitResult& Hit, UBoxComponent* Body, const FVector& Delta)
{
    if (!Hit.bStartPenetrating && Hit.Time > KINDA_SMALL_NUMBER) return false;
    auto* Other = Hit.GetComponent();
    if (!Other) return false;
    const FVector Center = Body->GetComponentLocation();
    const auto Shape = FCollisionShape::MakeBox(Body->GetScaledBoxExtent());
    FMTDResult Start, End;
    const bool OverlapStart = Other->ComputePenetration(Start, Shape, Center, Body->GetComponentQuat());
    const bool OverlapEnd = Other->ComputePenetration(End, Shape, Center + Delta, Body->GetComponentQuat());
    // Use all three axes: a descending pounce must not treat a vertical contact
    // as a harmless horizontal tangent and pass through the player.
    return FVector::DotProduct(Delta, Hit.Normal) >= -KINDA_SMALL_NUMBER &&
        (!OverlapEnd || (OverlapStart && End.Distance <= Start.Distance + KINDA_SMALL_NUMBER));
}

inline bool Floor(ACharacter* Character, AActor* Player, const FVector& Point, float Units, FVector& Ground)
{
    const auto* Capsule = Character->GetCapsuleComponent();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ToastyEncounterFloor), false, Character);
    IgnorePlayer(Query, Player);
    FCollisionObjectQueryParams Types;
    Types.AddObjectTypesToQuery(ECC_WorldStatic);
    Types.AddObjectTypesToQuery(ECC_WorldDynamic);
    const FVector Feet = Point - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
    TArray<FHitResult> Hits;
    Character->GetWorld()->LineTraceMultiByObjectType(Hits, Feet + FVector(0, 0, 1500.f * Units),
        Feet - FVector(0, 0, 4096.f * Units), Types, Query);
    for (const auto& Hit : Hits)
    {
        const auto* Surface = Hit.GetComponent();
        if (!Surface || Cast<APawn>(Hit.GetActor()) || Hit.ImpactNormal.Z < .65f ||
            Surface->GetCollisionResponseToChannel(Capsule->GetCollisionObjectType()) != ECR_Block) continue;
        // An upright capsule touches an incline off its centerline.
        const float Clearance = Capsule->GetScaledCapsuleRadius() * (1.f / Hit.ImpactNormal.Z - 1.f);
        Ground = Hit.ImpactPoint + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + Clearance + 2.f);
        // At triangle seams the center ray's plane can underestimate clearance.
        // Settle the complete capsule onto the terrain, including its footprint.
        FHitResult Support;
        const FVector Lift(0, 0, 600.f * Units);
        const auto Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() - 1.f);
        Character->GetWorld()->SweepSingleByChannel(Support, Ground + Lift, Ground - Lift,
            FQuat::Identity, ECC_WorldStatic, Shape, Query);
        if (!Support.bBlockingHit || Support.bStartPenetrating || Support.ImpactNormal.Z < .65f) return false;
        Ground = Support.Location + FVector(0, 0, 2.f);
        return true;
    }
    return false;
}

inline bool LiftFromFloor(ACharacter* Character, AActor* Player, float Units)
{
    FVector Ground;
    const FVector Start = Character->GetActorLocation();
    if (!Floor(Character, Player, Start, Units, Ground)) return false;
    if (Ground.Z <= Start.Z || Ground.Z - Start.Z > 600.f * Units) return true;
    auto* Capsule = Character->GetCapsuleComponent();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ToastyEncounterClearance), false, Character);
    IgnorePlayer(Query, Player);
    if (Character->GetWorld()->OverlapBlockingTestByChannel(Ground, FQuat::Identity, ECC_WorldStatic,
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() - 1.f), Query)) return false;
    Character->SetActorLocation(Ground, false, nullptr, ETeleportType::TeleportPhysics);
    return true;
}
}
