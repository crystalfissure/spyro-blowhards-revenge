#include "SM64PlayerAdapter.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SM64Interactable.h"
#include "UObject/UnrealType.h"

ASM64PlayerAdapter::ASM64PlayerAdapter()
{
    PrimaryActorTick.bCanEverTick = true;
    AttackProbe = CreateDefaultSubobject<USphereComponent>(TEXT("AttackProbe"));
    RootComponent = AttackProbe;
    AttackProbe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttackProbe->SetSphereRadius(AttackRadius);
}

bool ASM64PlayerAdapter::ReadSpyroBool(FName PropertyName) const
{
    if (!SpyroActor)
    {
        return false;
    }
    if (const FBoolProperty* Property = FindFProperty<FBoolProperty>(SpyroActor->GetClass(), PropertyName))
    {
        return Property->GetPropertyValue_InContainer(SpyroActor);
    }
    return false;
}

FString ASM64PlayerAdapter::ReadSpyroStateToken() const
{
    if (!SpyroActor)
    {
        return FString();
    }
    static const FName Candidates[] = { TEXT("Player_State"), TEXT("PlayerState"), TEXT("State") };
    for (const FName Candidate : Candidates)
    {
        FProperty* Property = FindFProperty<FProperty>(SpyroActor->GetClass(), Candidate);
        if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            const void* ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(SpyroActor);
            const int64 Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
            return EnumProperty->GetEnum()->GetNameStringByValue(Value);
        }
        if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            const uint8 Value = ByteProperty->GetPropertyValue_InContainer(SpyroActor);
            return ByteProperty->Enum ? ByteProperty->Enum->GetNameStringByValue(Value)
                : FString::FromInt(Value);
        }
        if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
        {
            return NameProperty->GetPropertyValue_InContainer(SpyroActor).ToString();
        }
        if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
        {
            return StringProperty->GetPropertyValue_InContainer(SpyroActor);
        }
    }
    return FString();
}

void ASM64PlayerAdapter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!SpyroActor)
    {
        BindToPlayer(UGameplayStatics::GetPlayerPawn(this, 0));
        return;
    }
    AttackDispatchCooldown = FMath::Max(0.0f, AttackDispatchCooldown - DeltaSeconds);
    if (bCannonLaunched)
    {
        ++CannonAirFrames;
        DispatchAttack(ESM64AttackType::Cannon, SpyroActor->GetVelocity().GetSafeNormal());
        if (CannonAirFrames > 3)
        {
            if (const ACharacter* Character = Cast<ACharacter>(SpyroActor))
            {
                if (Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsMovingOnGround())
                {
                    SetCannonLaunched(false);
                }
            }
        }
        return;
    }
    if (!bAutoDetectSpyroAttacks || AttackDispatchCooldown > 0.0f)
    {
        return;
    }
    const FString State = ReadSpyroStateToken().ToLower();
    if (ReadSpyroBool(TEXT("isHeadbashing")) || ReadSpyroBool(TEXT("Headbashing"))
        || State.Contains(TEXT("headbash")))
    {
        DispatchAttack(ESM64AttackType::Headbash, FVector::DownVector);
        AttackDispatchCooldown = 1.0f / 30.0f;
    }
    else if (State.Contains(TEXT("flame")))
    {
        DispatchAttack(ESM64AttackType::Flame, SpyroActor->GetActorForwardVector());
        AttackDispatchCooldown = 1.0f / 30.0f;
    }
    else if (State.Contains(TEXT("charge")) || State.Contains(TEXT("supercharge")))
    {
        DispatchAttack(ESM64AttackType::Charge, SpyroActor->GetActorForwardVector());
        AttackDispatchCooldown = 1.0f / 30.0f;
    }
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
    if (!SpyroActor || (AttackType == ESM64AttackType::Cannon && !bCannonLaunched))
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
            SpyroActor->GetActorLocation(),
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
    CannonAirFrames = 0;
}
