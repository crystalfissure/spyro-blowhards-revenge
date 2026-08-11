#include "MMAShieldGuardBehaviorComponent.h"

#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr uint8 ShieldDeadAIState = 3;
constexpr float ShieldMinimumOneShotDuration = 0.1f;
constexpr int32 ShieldMaximumLocalPlayersToCheck = 4;

FString NormalizeShieldPropertyName(FString Name)
{
    Name.ReplaceInline(TEXT("_"), TEXT(""));
    Name.ReplaceInline(TEXT(" "), TEXT(""));
    Name.ReplaceInline(TEXT("'"), TEXT(""));
    return Name.ToLower();
}
UObject* FindShieldWalkingAIObject(AActor* Owner)
{
    if (!Owner)
    {
        return nullptr;
    }
    if (FObjectPropertyBase* WalkingAI = FindFProperty<FObjectPropertyBase>(
            Owner->GetClass(), TEXT("Walking_AI_Character")))
    {
        return WalkingAI->GetObjectPropertyValue_InContainer(Owner);
    }
    return nullptr;
}

bool TryReadShieldAIState(UObject* Object, uint8& OutState)
{
    if (!Object)
    {
        return false;
    }
    FProperty* State = FindFProperty<FProperty>(Object->GetClass(), TEXT("AICharacter_State"));
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(State))
    {
        OutState = static_cast<uint8>(
            EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(
                EnumProperty->ContainerPtrToValuePtr<void>(Object)));
        return true;
    }
    if (FByteProperty* ByteProperty = CastField<FByteProperty>(State))
    {
        OutState = ByteProperty->GetPropertyValue_InContainer(Object);
        return true;
    }
    return false;
}

bool TryReadShieldHitPoints(UObject* Object, double& OutValue)
{
    if (!Object)
    {
        return false;
    }
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || NormalizeShieldPropertyName(Property->GetName()) != TEXT("hitpoints"))
        {
            continue;
        }
        if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
        {
            OutValue = IntProperty->GetPropertyValue_InContainer(Object);
            return true;
        }
        if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
        {
            OutValue = FloatProperty->GetPropertyValue_InContainer(Object);
            return true;
        }
        if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            OutValue = ByteProperty->GetPropertyValue_InContainer(Object);
            return true;
        }
    }
    return false;
}

bool TryWriteShieldHitPoints(UObject* Object, double Value)
{
    if (!Object)
    {
        return false;
    }
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || NormalizeShieldPropertyName(Property->GetName()) != TEXT("hitpoints"))
        {
            continue;
        }
        if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
        {
            IntProperty->SetPropertyValue_InContainer(Object, FMath::RoundToInt(Value));
            return true;
        }
        if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
        {
            FloatProperty->SetPropertyValue_InContainer(Object, static_cast<float>(Value));
            return true;
        }
        if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
        {
            DoubleProperty->SetPropertyValue_InContainer(Object, Value);
            return true;
        }
        if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            ByteProperty->SetPropertyValue_InContainer(
                Object, static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value), 0, 255)));
            return true;
        }
    }
    return false;
}

UEnum* GetInnerEnum(FProperty* Inner)
{
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Inner))
    {
        return EnumProperty->GetEnum();
    }
    if (FByteProperty* ByteProperty = CastField<FByteProperty>(Inner))
    {
        return ByteProperty->Enum;
    }
    return nullptr;
}

int64 ReadInnerEnumValue(FProperty* Inner, void* Address)
{
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Inner))
    {
        return EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Address);
    }
    if (FByteProperty* ByteProperty = CastField<FByteProperty>(Inner))
    {
        return ByteProperty->GetPropertyValue(Address);
    }
    return INDEX_NONE;
}

void WriteInnerEnumValue(FProperty* Inner, void* Address, int64 Value)
{
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Inner))
    {
        EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(Address, Value);
    }
    else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Inner))
    {
        ByteProperty->SetPropertyValue(Address, static_cast<uint8>(Value));
    }
}

bool FindEnumValueContaining(UEnum* Enum, const FString& Token, int64& OutValue)
{
    if (!Enum)
    {
        return false;
    }
    for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
    {
        const FString Name = Enum->GetNameStringByIndex(Index).ToLower();
        const FString Display = Enum->GetDisplayNameTextByIndex(Index).ToString().ToLower();
        if (Name.Contains(Token) || Display.Contains(Token))
        {
            OutValue = Enum->GetValueByIndex(Index);
            return true;
        }
    }
    return false;
}
}

UMMAShieldGuardBehaviorComponent::UMMAShieldGuardBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UMMAShieldGuardBehaviorComponent::BeginPlay()
{
    Super::BeginPlay();
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    CharacterOwner = Character;
    MeshComponent = Character ? Character->GetMesh() : nullptr;
    HomeLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    PatrolRandom.Initialize(GetTypeHash(HomeLocation) ^ (GetOwner() ? GetOwner()->GetUniqueID() : 0));
    ConfigureNativeEnemyContract();
    ConfigureShieldDamageContract();
    ConfigureDefaultDrop();
    EnterState(EMMAShieldGuardState::Idle);
}

void UMMAShieldGuardBehaviorComponent::SetInheritedBool(
    AActor* Owner,
    FName PropertyName,
    bool Value)
{
    auto TrySet = [PropertyName, Value](UObject* Object)
    {
        if (!Object)
        {
            return false;
        }
        if (FBoolProperty* Property = FindFProperty<FBoolProperty>(
                Object->GetClass(), PropertyName))
        {
            Property->SetPropertyValue_InContainer(Object, Value);
            return true;
        }
        return false;
    };
    if (!TrySet(Owner))
    {
        TrySet(FindShieldWalkingAIObject(Owner));
    }
}

void UMMAShieldGuardBehaviorComponent::ConfigureNativeEnemyContract()
{
    AActor* Owner = GetOwner();
    SetInheritedBool(Owner, TEXT("Can_Become_Alert"), false);
    SetInheritedBool(Owner, TEXT("Can_Attack"), false);
    SetInheritedBool(Owner, TEXT("WeaponHitboxActive"), false);
    SetInheritedAIState(0);

    if (ACharacter* Character = CharacterOwner.Get())
    {
        Character->bUseControllerRotationYaw = false;
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->bOrientRotationToMovement = false;
            Movement->bUseControllerDesiredRotation = false;
            Movement->RotationRate = FRotator(0.0f, RotationSpeedDegrees, 0.0f);
        }
    }

    if (!Owner)
    {
        return;
    }
    TArray<UPrimitiveComponent*> Primitives;
    Owner->GetComponents(Primitives);
    for (UPrimitiveComponent* Primitive : Primitives)
    {
        FString Name = Primitive ? NormalizeShieldPropertyName(Primitive->GetName()) : FString();
        Name.ReplaceInline(TEXT("genvariable"), TEXT(""));
        if (Primitive && Name.Contains(TEXT("weaponhitbox")))
        {
            Primitive->SetGenerateOverlapEvents(false);
            Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        else if (Primitive && Name.Contains(TEXT("shieldhitbox")))
        {
            ConfigureChargeCollision(Primitive);
        }
    }

    if (DeathAnimation)
    {
        const float Delay = DeathAnimation->GetPlayLength() + DeathPoofPaddingSeconds;
        for (TFieldIterator<FFloatProperty> It(Owner->GetClass()); It; ++It)
        {
            if (NormalizeShieldPropertyName(It->GetName()) == TEXT("corpsepoofdelay"))
            {
                It->SetPropertyValue_InContainer(Owner, Delay);
                break;
            }
        }
    }
}

void UMMAShieldGuardBehaviorComponent::ConfigureChargeCollision(
    UPrimitiveComponent* Primitive)
{
    ACharacter* Character = CharacterOwner.Get();
    UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
    if (!Primitive || !Capsule)
    {
        return;
    }
    ShieldCollisionComponent = Primitive;
    Primitive->AttachToComponent(
        Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Primitive->SetRelativeLocation(FVector::ZeroVector);
    Primitive->SetRelativeRotation(FRotator::ZeroRotator);
    Primitive->SetRelativeScale3D(FVector::OneVector);
    if (UBoxComponent* Box = Cast<UBoxComponent>(Primitive))
    {
        Box->SetBoxExtent(FVector(
            Capsule->GetUnscaledCapsuleRadius() * ChargeCollisionRadiusScale,
            Capsule->GetUnscaledCapsuleRadius() * ChargeCollisionRadiusScale,
            Capsule->GetUnscaledCapsuleHalfHeight() * ChargeCollisionHalfHeightScale));
    }
    // Spyro ignores Pawn bodies while charging. The stock Gnorc shield solves
    // that with a WorldStatic query box which blocks Pawn; keep the blocker
    // slightly inside the damageable body capsule so the Ram hit registers first.
    Primitive->SetCollisionObjectType(ECC_WorldStatic);
    Primitive->SetCollisionResponseToAllChannels(ECR_Ignore);
    Primitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Primitive->SetGenerateOverlapEvents(true);
    Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void UMMAShieldGuardBehaviorComponent::SetChargeCollisionEnabled(bool bEnabled) const
{
    if (UPrimitiveComponent* Primitive = ShieldCollisionComponent.Get())
    {
        Primitive->SetGenerateOverlapEvents(bEnabled);
        Primitive->SetCollisionEnabled(
            bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
}

UActorComponent* UMMAShieldGuardBehaviorComponent::FindDamageableComponent(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }
    TArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (Component &&
            (Component->GetClass()->GetName().Contains(TEXT("Damageable_Com")) ||
             Component->FindFunction(TEXT("Deal Damage")) ||
             Component->FindFunction(TEXT("Deal_Damage"))))
        {
            return Component;
        }
    }
    return nullptr;
}

void UMMAShieldGuardBehaviorComponent::ConfigureShieldDamageContract()
{
    AActor* Owner = GetOwner();
    UActorComponent* Damageable = FindDamageableComponent(Owner);
    if (!Owner || !Damageable)
    {
        ShowDebugMessage(TEXT("Shield guard: Damageable component missing"), FColor::Red);
        return;
    }
    DamageableComponent = Damageable;
    Owner->SetCanBeDamaged(true);
    for (TFieldIterator<FProperty> It(Damageable->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        const FString Name = Property ? NormalizeShieldPropertyName(Property->GetName()) : FString();
        if (Name == TEXT("invincible"))
        {
            if (FBoolProperty* Invincible = CastField<FBoolProperty>(Property))
            {
                Invincible->SetPropertyValue_InContainer(Damageable, false);
            }
        }
        else if (Name == TEXT("objectshitboxcomponent"))
        {
            if (FObjectPropertyBase* Hitbox = CastField<FObjectPropertyBase>(Property))
            {
                UCapsuleComponent* Capsule = CharacterOwner.IsValid()
                    ? CharacterOwner->GetCapsuleComponent()
                    : nullptr;
                if (Capsule && Capsule->IsA(Hitbox->PropertyClass))
                {
                    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                    Capsule->SetGenerateOverlapEvents(true);
                    Hitbox->SetObjectPropertyValue_InContainer(Damageable, Capsule);
                }
            }
        }
        else if (Name == TEXT("damageresistances"))
        {
            FArrayProperty* Resistances = CastField<FArrayProperty>(Property);
            UEnum* Enum = Resistances ? GetInnerEnum(Resistances->Inner) : nullptr;
            if (!Resistances || !Enum)
            {
                continue;
            }
            FScriptArrayHelper Values(
                Resistances,
                Resistances->ContainerPtrToValuePtr<void>(Damageable));
            auto RemoveResistance = [&Values, Resistances, Enum](const FString& Token)
            {
                int64 Value = INDEX_NONE;
                if (!FindEnumValueContaining(Enum, Token, Value))
                {
                    return;
                }
                for (int32 Index = Values.Num() - 1; Index >= 0; --Index)
                {
                    if (ReadInnerEnumValue(Resistances->Inner, Values.GetRawPtr(Index)) == Value)
                    {
                        Values.RemoveValues(Index, 1);
                    }
                }
            };
            // Spyro's charge is named Ram in this project's Damage_Types enum.
            RemoveResistance(TEXT("ram"));
            RemoveResistance(TEXT("charge"));

            int64 BurnValue = INDEX_NONE;
            const bool bFoundBurn = FindEnumValueContaining(Enum, TEXT("burn"), BurnValue) ||
                FindEnumValueContaining(Enum, TEXT("flame"), BurnValue);
            if (bImmuneToFlame && bFoundBurn)
            {
                bool bAlreadyPresent = false;
                for (int32 Index = 0; Index < Values.Num(); ++Index)
                {
                    bAlreadyPresent |=
                        ReadInnerEnumValue(Resistances->Inner, Values.GetRawPtr(Index)) == BurnValue;
                }
                if (!bAlreadyPresent)
                {
                    const int32 Index = Values.AddValue();
                    WriteInnerEnumValue(Resistances->Inner, Values.GetRawPtr(Index), BurnValue);
                }
            }
        }
    }
    if (!TryWriteShieldHitPoints(Damageable, InitialHitPoints))
    {
        ShowDebugMessage(TEXT("Shield guard: could not set one-hit health"), FColor::Yellow);
    }
    bHasObservedHitPoints = TryReadShieldHitPoints(Damageable, LastObservedHitPoints);
}

void UMMAShieldGuardBehaviorComponent::ConfigureDefaultDrop()
{
    AActor* Owner = GetOwner();
    if (!Owner || !DefaultDropClass)
    {
        return;
    }
    TArray<UActorComponent*> Components;
    Owner->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (!Component || !Component->GetClass()->GetName().Contains(TEXT("Drops_Items")))
        {
            continue;
        }
        FArrayProperty* Items = FindFProperty<FArrayProperty>(
            Component->GetClass(), TEXT("Items_to_Drop"));
        FObjectPropertyBase* Inner = Items ? CastField<FObjectPropertyBase>(Items->Inner) : nullptr;
        if (!Items || !Inner)
        {
            continue;
        }
        FScriptArrayHelper Values(Items, Items->ContainerPtrToValuePtr<void>(Component));
        if (Values.Num() == 0)
        {
            const int32 Index = Values.AddValue();
            Inner->SetObjectPropertyValue(Values.GetRawPtr(Index), DefaultDropClass.Get());
        }
        return;
    }
}

uint8 UMMAShieldGuardBehaviorComponent::ReadInheritedAIState() const
{
    uint8 State = 0;
    AActor* Owner = GetOwner();
    if (TryReadShieldAIState(Owner, State) ||
        TryReadShieldAIState(FindShieldWalkingAIObject(Owner), State))
    {
        return State;
    }
    return 0;
}

bool UMMAShieldGuardBehaviorComponent::SetInheritedAIState(uint8 Value) const
{
    auto TrySet = [Value](UObject* Object)
    {
        if (!Object)
        {
            return false;
        }
        FProperty* State = FindFProperty<FProperty>(Object->GetClass(), TEXT("AICharacter_State"));
        if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(State))
        {
            EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                EnumProperty->ContainerPtrToValuePtr<void>(Object), static_cast<int64>(Value));
            return true;
        }
        if (FByteProperty* ByteProperty = CastField<FByteProperty>(State))
        {
            ByteProperty->SetPropertyValue_InContainer(Object, Value);
            return true;
        }
        return false;
    };
    AActor* Owner = GetOwner();
    return TrySet(Owner) || TrySet(FindShieldWalkingAIObject(Owner));
}

APawn* UMMAShieldGuardBehaviorComponent::FindNearestPlayer(float Radius) const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }
    APawn* Best = nullptr;
    float BestSquared = FMath::Square(Radius);
    for (int32 PlayerIndex = 0; PlayerIndex < ShieldMaximumLocalPlayersToCheck; ++PlayerIndex)
    {
        APawn* Candidate = UGameplayStatics::GetPlayerPawn(this, PlayerIndex);
        if (!Candidate || Candidate == Owner || Candidate->IsActorBeingDestroyed() || Candidate->IsHidden())
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared2D(
            Owner->GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared > BestSquared)
        {
            continue;
        }
        if (bRequireLineOfSight)
        {
            AAIController* Controller = Cast<AAIController>(
                CharacterOwner.IsValid() ? CharacterOwner->GetController() : nullptr);
            if (Controller && !Controller->LineOfSightTo(Candidate))
            {
                continue;
            }
        }
        Best = Candidate;
        BestSquared = DistanceSquared;
    }
    return Best;
}

bool UMMAShieldGuardBehaviorComponent::HasValidTarget(float Radius) const
{
    AActor* Owner = GetOwner();
    APawn* Target = TargetPawn.Get();
    return Owner && Target && !Target->IsActorBeingDestroyed() && !Target->IsHidden() &&
        FVector::DistSquared2D(Owner->GetActorLocation(), Target->GetActorLocation()) <=
            FMath::Square(Radius);
}

void UMMAShieldGuardBehaviorComponent::FaceLocation(
    const FVector& Location,
    float DeltaTime) const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    FVector Direction = Location - Owner->GetActorLocation();
    Direction.Z = 0.0f;
    if (Direction.IsNearlyZero())
    {
        return;
    }
    const FRotator Wanted(0.0f, Direction.Rotation().Yaw, 0.0f);
    Owner->SetActorRotation(FMath::RInterpConstantTo(
        Owner->GetActorRotation(), Wanted, DeltaTime, RotationSpeedDegrees));
}

void UMMAShieldGuardBehaviorComponent::StopMovement() const
{
    ACharacter* Character = CharacterOwner.Get();
    if (Character && Character->GetCharacterMovement())
    {
        Character->GetCharacterMovement()->StopMovementImmediately();
    }
}

void UMMAShieldGuardBehaviorComponent::ChoosePatrolTarget()
{
    const float Angle = PatrolRandom.FRandRange(0.0f, 2.0f * PI);
    const float Distance = PatrolRandom.FRandRange(PatrolRadius * 0.45f, PatrolRadius);
    PatrolTarget = HomeLocation + FVector(
        FMath::Cos(Angle) * Distance,
        FMath::Sin(Angle) * Distance,
        0.0f);
    PatrolTargetElapsed = 0.0f;
}

void UMMAShieldGuardBehaviorComponent::BeginPatrolExcursion()
{
    bPatrolReturningHome = false;
    PatrolPauseRemaining = 0.0f;
    ChoosePatrolTarget();
    EnterState(EMMAShieldGuardState::Patrol);
}

void UMMAShieldGuardBehaviorComponent::MoveTowardPatrolTarget(float DeltaTime) const
{
    ACharacter* Character = CharacterOwner.Get();
    if (!Character)
    {
        return;
    }
    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = PatrolSpeed;
    }
    FaceLocation(PatrolTarget, DeltaTime);
    FVector Direction = PatrolTarget - Character->GetActorLocation();
    Direction.Z = 0.0f;
    Character->AddMovementInput(Direction.GetSafeNormal(), 1.0f);
}

float UMMAShieldGuardBehaviorComponent::GetStateDuration() const
{
    UAnimSequence* Animation = nullptr;
    switch (CurrentState)
    {
    case EMMAShieldGuardState::Attack: Animation = AttackAnimation; break;
    case EMMAShieldGuardState::Dead: Animation = DeathAnimation; break;
    default: break;
    }
    return Animation
        ? FMath::Max(Animation->GetPlayLength(), ShieldMinimumOneShotDuration)
        : ShieldMinimumOneShotDuration;
}

void UMMAShieldGuardBehaviorComponent::PlayStateAnimation()
{
    USkeletalMeshComponent* Mesh = MeshComponent.Get();
    if (!Mesh)
    {
        return;
    }
    UAnimSequence* Animation = nullptr;
    bool bLoop = false;
    switch (CurrentState)
    {
    case EMMAShieldGuardState::Idle:
        // Four-role guards generated before Idle was introduced remain usable
        // until they are regenerated with an explicit Idle assignment.
        Animation = IdleAnimation ? IdleAnimation : PatrolAnimation;
        bLoop = true;
        break;
    case EMMAShieldGuardState::Patrol: Animation = PatrolAnimation; bLoop = true; break;
    case EMMAShieldGuardState::EnGarde: Animation = EnGardeAnimation; bLoop = true; break;
    case EMMAShieldGuardState::Attack: Animation = AttackAnimation; break;
    case EMMAShieldGuardState::Dead: Animation = DeathAnimation; break;
    }
    if (Animation)
    {
        Mesh->PlayAnimation(Animation, bLoop);
    }
}

void UMMAShieldGuardBehaviorComponent::EnterState(EMMAShieldGuardState NewState)
{
    CurrentState = NewState;
    StateElapsedSeconds = 0.0f;
    bAttackHitApplied = false;
    if (NewState == EMMAShieldGuardState::Idle)
    {
        IdleWaitRemaining = PatrolRandom.FRandRange(
            FMath::Min(IdleWaitMinimum, IdleWaitMaximum),
            FMath::Max(IdleWaitMinimum, IdleWaitMaximum));
        PatrolPauseRemaining = 0.0f;
    }
    SetChargeCollisionEnabled(NewState != EMMAShieldGuardState::Dead);
    if (NewState != EMMAShieldGuardState::Dead)
    {
        SetInheritedAIState(0);
        SetInheritedBool(GetOwner(), TEXT("WeaponHitboxActive"), false);
    }
    if (NewState == EMMAShieldGuardState::Idle ||
        NewState == EMMAShieldGuardState::EnGarde ||
        NewState == EMMAShieldGuardState::Attack ||
        NewState == EMMAShieldGuardState::Dead)
    {
        StopMovement();
    }
    PlayStateAnimation();
    ShowDebugMessage(FString::Printf(
        TEXT("Shield guard state: %s"), *UEnum::GetValueAsString(NewState)));
}

bool UMMAShieldGuardBehaviorComponent::DealNativeDamageToTarget(
    AActor* Target,
    bool& bOutDamageApplied) const
{
    bOutDamageApplied = false;
    UActorComponent* Damageable = FindDamageableComponent(Target);
    AActor* Owner = GetOwner();
    if (!Damageable || !Owner)
    {
        return false;
    }
    UFunction* Function = Damageable->FindFunction(TEXT("Deal Damage"));
    if (!Function)
    {
        Function = Damageable->FindFunction(TEXT("Deal_Damage"));
    }
    if (!Function)
    {
        return false;
    }
    TArray<uint8> Parameters;
    Parameters.SetNumZeroed(Function->ParmsSize);
    Function->InitializeStruct(Parameters.GetData());
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property->HasAnyPropertyFlags(CPF_Parm) ||
            Property->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            continue;
        }
        const FString Name = NormalizeShieldPropertyName(Property->GetName());
        if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            if (Name.Contains(TEXT("damagetype")))
            {
                EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                    EnumProperty->ContainerPtrToValuePtr<void>(Parameters.GetData()),
                    static_cast<int64>(OutgoingDamageType));
            }
        }
        else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            if (Name.Contains(TEXT("damagetype")))
            {
                ByteProperty->SetPropertyValue_InContainer(Parameters.GetData(), OutgoingDamageType);
            }
        }
        else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            if (StructProperty->Struct == TBaseStructure<FVector>::Get())
            {
                *StructProperty->ContainerPtrToValuePtr<FVector>(Parameters.GetData()) =
                    Owner->GetActorForwardVector();
            }
        }
        else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
        {
            BoolProperty->SetPropertyValue_InContainer(Parameters.GetData(), false);
        }
        else if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
        {
            if (Owner->IsA(ObjectProperty->PropertyClass))
            {
                ObjectProperty->SetObjectPropertyValue_InContainer(Parameters.GetData(), Owner);
            }
        }
    }
    double Before = 0.0;
    const bool bHadBefore = TryReadShieldHitPoints(Damageable, Before);
    Damageable->ProcessEvent(Function, Parameters.GetData());
    Function->DestroyStruct(Parameters.GetData());
    double After = 0.0;
    const bool bHasAfter = TryReadShieldHitPoints(Damageable, After);
    bOutDamageApplied = !bHadBefore || !bHasAfter || After < Before;
    return true;
}

void UMMAShieldGuardBehaviorComponent::ApplyHitRecoil(AActor* Target) const
{
    ACharacter* TargetCharacter = Cast<ACharacter>(Target);
    AActor* Owner = GetOwner();
    if (!TargetCharacter || !Owner)
    {
        return;
    }
    FVector Away = Target->GetActorLocation() - Owner->GetActorLocation();
    Away.Z = 0.0f;
    if (!Away.Normalize())
    {
        Away = Owner->GetActorForwardVector();
    }
    FVector Velocity = Away * RecoilHorizontalSpeed;
    Velocity.Z = RecoilVerticalSpeed;
    TargetCharacter->LaunchCharacter(Velocity, true, true);
}

void UMMAShieldGuardBehaviorComponent::ApplyChargeImpactKnockback() const
{
    ACharacter* Character = CharacterOwner.Get();
    AActor* Owner = GetOwner();
    if (!Character || !Owner)
    {
        return;
    }
    APawn* Source = FindNearestPlayer(FMath::Max(LoseInterestRadius, 1200.0f));
    FVector Away = Source
        ? Owner->GetActorLocation() - Source->GetActorLocation()
        : -Owner->GetActorForwardVector();
    Away.Z = 0.0f;
    if (!Away.Normalize())
    {
        Away = -Owner->GetActorForwardVector();
    }
    if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Capsule->SetGenerateOverlapEvents(true);
    }
    FVector Velocity = Away * ChargeKnockbackHorizontalSpeed;
    Velocity.Z = ChargeKnockbackVerticalSpeed;
    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_Falling);
    }
    Character->LaunchCharacter(Velocity, true, true);
    ShowDebugMessage(TEXT("Shield guard hit by charge: applying knockback"), FColor::Yellow);
}

void UMMAShieldGuardBehaviorComponent::ObserveIncomingDamage()
{
    UActorComponent* Damageable = DamageableComponent.Get();
    if (!Damageable)
    {
        return;
    }
    double HitPoints = 0.0;
    if (!TryReadShieldHitPoints(Damageable, HitPoints))
    {
        return;
    }
    if (bHasObservedHitPoints && HitPoints < LastObservedHitPoints)
    {
        // Burn is resisted, so a normal Spyro hit-point decrease on this
        // archetype is the Ram/charge impact that should throw the guard back.
        ApplyChargeImpactKnockback();
    }
    LastObservedHitPoints = HitPoints;
    bHasObservedHitPoints = true;
}

void UMMAShieldGuardBehaviorComponent::ApplyAttackHit()
{
    bAttackHitApplied = true;
    AActor* Owner = GetOwner();
    APawn* Target = TargetPawn.Get();
    if (!Owner || !Target)
    {
        return;
    }
    FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
    ToTarget.Z = 0.0f;
    if (ToTarget.SizeSquared() > FMath::Square(AttackHitRange))
    {
        return;
    }
    const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(AttackHalfAngleDegrees));
    if (FVector::DotProduct(Owner->GetActorForwardVector(), ToTarget.GetSafeNormal()) < MinimumDot)
    {
        return;
    }
    bool bDamageApplied = false;
    if (DealNativeDamageToTarget(Target, bDamageApplied) && bDamageApplied)
    {
        ApplyHitRecoil(Target);
    }
}

void UMMAShieldGuardBehaviorComponent::ShowDebugMessage(
    const FString& Message,
    const FColor& Color) const
{
    if (bEnableDebugMessages && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, Color, Message);
    }
}

void UMMAShieldGuardBehaviorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    if (ReadInheritedAIState() == ShieldDeadAIState)
    {
        if (CurrentState != EMMAShieldGuardState::Dead)
        {
            EnterState(EMMAShieldGuardState::Dead);
        }
        ObserveIncomingDamage();
        return;
    }

    // The inherited ShieldEnemy graph disables its shield while its native AI
    // state is Idle. This component owns the replacement state machine, so keep
    // the body-aligned charge blocker enabled throughout every living state.
    SetChargeCollisionEnabled(true);
    ObserveIncomingDamage();

    StateElapsedSeconds += DeltaTime;
    AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaTime);
    switch (CurrentState)
    {
    case EMMAShieldGuardState::Idle:
        TargetPawn = FindNearestPlayer(GuardRadius);
        if (TargetPawn.IsValid())
        {
            EnterState(EMMAShieldGuardState::EnGarde);
            break;
        }
        IdleWaitRemaining = FMath::Max(0.0f, IdleWaitRemaining - DeltaTime);
        if (IdleWaitRemaining <= 0.0f && PatrolRadius > 0.0f && PatrolSpeed > 0.0f)
        {
            BeginPatrolExcursion();
        }
        break;

    case EMMAShieldGuardState::Patrol:
        TargetPawn = FindNearestPlayer(GuardRadius);
        if (TargetPawn.IsValid())
        {
            EnterState(EMMAShieldGuardState::EnGarde);
            break;
        }
        if (PatrolPauseRemaining > 0.0f)
        {
            PatrolPauseRemaining = FMath::Max(0.0f, PatrolPauseRemaining - DeltaTime);
            StopMovement();
            if (PatrolPauseRemaining <= 0.0f)
            {
                PatrolTarget = HomeLocation;
                PatrolTargetElapsed = 0.0f;
                PlayStateAnimation();
            }
            break;
        }
        PatrolTargetElapsed += DeltaTime;
        if (FVector::Dist2D(Owner->GetActorLocation(), PatrolTarget) <= PatrolAcceptanceRadius ||
            PatrolTargetElapsed >= PatrolTargetTimeout)
        {
            StopMovement();
            if (bPatrolReturningHome)
            {
                EnterState(EMMAShieldGuardState::Idle);
            }
            else
            {
                bPatrolReturningHome = true;
                PatrolPauseRemaining = PatrolRandom.FRandRange(
                    FMath::Min(PatrolPauseMinimum, PatrolPauseMaximum),
                    FMath::Max(PatrolPauseMinimum, PatrolPauseMaximum));
                if (IdleAnimation && MeshComponent.IsValid())
                {
                    MeshComponent->PlayAnimation(IdleAnimation, true);
                }
                if (PatrolPauseRemaining <= 0.0f)
                {
                    PatrolTarget = HomeLocation;
                    PatrolTargetElapsed = 0.0f;
                    PlayStateAnimation();
                }
            }
        }
        else
        {
            MoveTowardPatrolTarget(DeltaTime);
        }
        break;

    case EMMAShieldGuardState::EnGarde:
        if (!HasValidTarget(LoseInterestRadius))
        {
            TargetPawn.Reset();
            EnterState(EMMAShieldGuardState::Idle);
            break;
        }
        FaceLocation(TargetPawn->GetActorLocation(), DeltaTime);
        if (FVector::Dist2D(Owner->GetActorLocation(), TargetPawn->GetActorLocation()) <= AttackRange &&
            AttackCooldownRemaining <= 0.0f)
        {
            EnterState(EMMAShieldGuardState::Attack);
        }
        break;

    case EMMAShieldGuardState::Attack:
    {
        if (TargetPawn.IsValid())
        {
            FaceLocation(TargetPawn->GetActorLocation(), DeltaTime);
        }
        const float ContactTime = GetStateDuration() * AttackContactFraction;
        if (!bAttackHitApplied && StateElapsedSeconds >= ContactTime)
        {
            ApplyAttackHit();
        }
        if (StateElapsedSeconds >= GetStateDuration())
        {
            AttackCooldownRemaining = AttackCooldownSeconds;
            if (HasValidTarget(LoseInterestRadius))
            {
                EnterState(EMMAShieldGuardState::EnGarde);
            }
            else
            {
                TargetPawn.Reset();
                EnterState(EMMAShieldGuardState::Idle);
            }
        }
        break;
    }

    case EMMAShieldGuardState::Dead:
        break;
    }
}
