#pragma once

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

namespace MMAMeleeContact
{
inline void BodySpan(const AActor* Actor, FVector& Center, float& HalfHeight,
    ECollisionChannel& ObjectType)
{
    if (const auto* Character = Cast<ACharacter>(Actor))
    {
        const auto* Capsule = Character->GetCapsuleComponent();
        Center = Capsule->GetComponentLocation();
        HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        ObjectType = Capsule->GetCollisionObjectType();
        return;
    }
    FVector Extent;
    Actor->GetActorBounds(true, Center, Extent);
    HalfHeight = Extent.Z;
    ObjectType = ECC_Pawn;
}

inline void IgnoreCombatant(FCollisionQueryParams& Query, const AActor* Actor)
{
    Query.AddIgnoredActor(Actor);
    TArray<AActor*> Children;
    Actor->GetAllChildActors(Children, true);
    Query.AddIgnoredActors(Children);
}

/** Final contact permission, independent of awareness/range/cone tuning. */
inline bool HasClearContact(const AActor* Attacker, const AActor* Target)
{
    if (!IsValid(Attacker) || !IsValid(Target) || Attacker->IsHidden() || Target->IsHidden() ||
        !Attacker->GetWorld() || Attacker->GetWorld() != Target->GetWorld()) return false;

    FVector Start, End;
    float AttackerHalfHeight, TargetHalfHeight;
    ECollisionChannel AttackerType, TargetType;
    BodySpan(Attacker, Start, AttackerHalfHeight, AttackerType);
    BodySpan(Target, End, TargetHalfHeight, TargetType);
    const float Bottom = FMath::Max(Start.Z - AttackerHalfHeight, End.Z - TargetHalfHeight);
    const float Top = FMath::Min(Start.Z + AttackerHalfHeight, End.Z + TargetHalfHeight);
    if (Top <= Bottom + KINDA_SMALL_NUMBER) return false;

    // Trace inside the common body-height interval, so different capsule heights
    // or a modest jump/step do not aim into the floor or reject valid contact.
    Start.Z = End.Z = (Bottom + Top) * .5f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MMAMeleeContact), false, Attacker);
    IgnoreCombatant(Query, Attacker);
    IgnoreCombatant(Query, Target);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    Objects.AddObjectTypesToQuery(ECC_Destructible);
    TArray<FHitResult> Hits;
    Attacker->GetWorld()->LineTraceMultiByObjectType(Hits, Start, End, Objects, Query);
    for (const auto& Hit : Hits)
    {
        const auto* Surface = Hit.GetComponent();
        // Object queries also see overlap triggers and cosmetic geometry. Only
        // surfaces solid for a combatant can obstruct this melee contact.
        if (Surface && (Surface->GetCollisionResponseToChannel(AttackerType) == ECR_Block ||
            Surface->GetCollisionResponseToChannel(TargetType) == ECR_Block)) return false;
    }
    return true;
}
}
