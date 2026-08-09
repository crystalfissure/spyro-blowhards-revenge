#include "MMAHedgeTrimmerBehaviorComponent.h"

#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr uint8 DeadAIState = 3;
constexpr float MinimumOneShotDuration = 0.1f;
constexpr int32 MaximumLocalPlayersToCheck = 4;

UObject* FindWalkingAIObject(AActor* Owner)
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

bool TryReadAIState(UObject* Object, uint8& OutState)
{
    if (!Object)
    {
        return false;
    }
    FProperty* StateProperty = FindFProperty<FProperty>(Object->GetClass(), TEXT("AICharacter_State"));
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(StateProperty))
    {
        OutState = static_cast<uint8>(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(
            EnumProperty->ContainerPtrToValuePtr<void>(Object)));
        return true;
    }
    if (FByteProperty* ByteProperty = CastField<FByteProperty>(StateProperty))
    {
        OutState = ByteProperty->GetPropertyValue_InContainer(Object);
        return true;
    }
    return false;
}

FString NormalizePropertyName(FString Name)
{
    Name.ReplaceInline(TEXT("_"), TEXT(""));
    Name.ReplaceInline(TEXT(" "), TEXT(""));
    Name.ReplaceInline(TEXT("'"), TEXT(""));
    return Name.ToLower();
}

bool TryReadHitPoints(UObject* Object, double& OutValue)
{
    if (!Object)
    {
        return false;
    }
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || NormalizePropertyName(Property->GetName()) != TEXT("hitpoints"))
        {
            continue;
        }
        if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
        {
            OutValue = static_cast<double>(IntProperty->GetPropertyValue_InContainer(Object));
            return true;
        }
        if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
        {
            OutValue = static_cast<double>(FloatProperty->GetPropertyValue_InContainer(Object));
            return true;
        }
        if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
        {
            OutValue = DoubleProperty->GetPropertyValue_InContainer(Object);
            return true;
        }
        if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            OutValue = static_cast<double>(ByteProperty->GetPropertyValue_InContainer(Object));
            return true;
        }
    }
    return false;
}
}

UMMAHedgeTrimmerBehaviorComponent::UMMAHedgeTrimmerBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UMMAHedgeTrimmerBehaviorComponent::BeginPlay()
{
    Super::BeginPlay();
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    CharacterOwner = Character;
    MeshComponent = Character ? Character->GetMesh() : nullptr;
    HomeLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    ConfigureNativeEnemyContract();
    ConfigureIncomingDamageContract();
    ConfigureDefaultDrop();
    EnterState(EMMAHedgeTrimmerState::Idle);
}

void UMMAHedgeTrimmerBehaviorComponent::SetInheritedBool(
    AActor* Owner,
    FName PropertyName,
    bool Value)
{
    if (!Owner)
    {
        return;
    }
    if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Owner->GetClass(), PropertyName))
    {
        Property->SetPropertyValue_InContainer(Owner, Value);
        return;
    }
    if (UObject* WalkingAI = FindWalkingAIObject(Owner))
    {
        if (FBoolProperty* Property = FindFProperty<FBoolProperty>(WalkingAI->GetClass(), PropertyName))
        {
            Property->SetPropertyValue_InContainer(WalkingAI, Value);
        }
    }
}

bool UMMAHedgeTrimmerBehaviorComponent::SetInheritedFloat(
    AActor* Owner,
    FName PropertyName,
    float Value)
{
    const FString Wanted = NormalizePropertyName(PropertyName.ToString());
    auto TrySet = [&Wanted, Value](UObject* Object)
    {
        if (!Object)
        {
            return false;
        }
        for (TFieldIterator<FFloatProperty> It(Object->GetClass()); It; ++It)
        {
            FFloatProperty* Property = *It;
            if (Property && NormalizePropertyName(Property->GetName()) == Wanted)
            {
                Property->SetPropertyValue_InContainer(Object, Value);
                return true;
            }
        }
        return false;
    };
    return TrySet(Owner) || TrySet(FindWalkingAIObject(Owner));
}

void UMMAHedgeTrimmerBehaviorComponent::ShowDebugMessage(
    const FString& Message,
    const FColor& Color) const
{
    if (bEnableDebugMessages && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, Color, Message);
    }
}

void UMMAHedgeTrimmerBehaviorComponent::ConfigureNativeEnemyContract()
{
    AActor* Owner = GetOwner();
    // The generic base remains responsible for receiving Spyro's charge/flame
    // damage, death, persistence and drops. Its autonomous alert/attack logic
    // is disabled so it cannot compete with this bespoke state machine.
    SetInheritedBool(Owner, TEXT("Can_Become_Alert"), false);
    SetInheritedBool(Owner, TEXT("Can_Attack"), false);

    // The inherited controller owns yaw by default, which can overwrite the
    // bespoke look-at rotation later in the frame. Let CharacterMovement face
    // the acceleration vector and keep the explicit FaceLocation call as the
    // immediate visual response.
    if (ACharacter* Character = CharacterOwner.Get())
    {
        Character->bUseControllerRotationYaw = false;
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->bOrientRotationToMovement = true;
            Movement->bUseControllerDesiredRotation = false;
            Movement->RotationRate = FRotator(0.0f, RotationSpeedDegrees, 0.0f);
        }
    }
    if (DeathAnimation)
    {
        const float PoofDelay = DeathAnimation->GetPlayLength() + DeathPoofPaddingSeconds;
        if (!SetInheritedFloat(Owner, TEXT("Corpse_Poof_Delay"), PoofDelay))
        {
            ShowDebugMessage(TEXT("Hedge_Trimmer: could not set corpse poof delay"), FColor::Yellow);
        }
    }

    // Hedge_Trimmer owns targeting and attack timing. Keep the duplicated
    // club template's bound overlap dormant so it cannot wake its generic AI
    // graph, which expects an inherited alert target that this enemy does not use.
    if (Owner)
    {
        TArray<UPrimitiveComponent*> PrimitiveComponents;
        Owner->GetComponents(PrimitiveComponents);
        for (UPrimitiveComponent* Primitive : PrimitiveComponents)
        {
            FString Name = Primitive ? Primitive->GetName().ToLower() : FString();
            Name.ReplaceInline(TEXT("_"), TEXT(""));
            Name.ReplaceInline(TEXT(" "), TEXT(""));
            Name.ReplaceInline(TEXT("genvariable"), TEXT(""));
            if (Primitive && Name.Contains(TEXT("weaponhitbox")))
            {
                Primitive->SetGenerateOverlapEvents(false);
                Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
    }
}

void UMMAHedgeTrimmerBehaviorComponent::ConfigureIncomingDamageContract()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    Owner->SetCanBeDamaged(true);

    TArray<UActorComponent*> Components;
    Owner->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (!Component || !Component->GetClass()->GetName().Contains(TEXT("Damageable_Com")))
        {
            continue;
        }
        for (TFieldIterator<FProperty> It(Component->GetClass()); It; ++It)
        {
            FProperty* Property = *It;
            const FString Name = Property ? NormalizePropertyName(Property->GetName()) : FString();
            if (Name == TEXT("invincible"))
            {
                if (FBoolProperty* Invincible = CastField<FBoolProperty>(Property))
                {
                    Invincible->SetPropertyValue_InContainer(Component, false);
                }
            }
            else if (Name == TEXT("damageresistances"))
            {
                if (FArrayProperty* Resistances = CastField<FArrayProperty>(Property))
                {
                    FScriptArrayHelper Values(
                        Resistances,
                        Resistances->ContainerPtrToValuePtr<void>(Component));
                    Values.EmptyValues();
                }
            }
            else if (Name == TEXT("objectshitboxcomponent"))
            {
                if (FObjectPropertyBase* Hitbox = CastField<FObjectPropertyBase>(Property))
                {
                    UPrimitiveComponent* Capsule = CharacterOwner.IsValid()
                        ? CharacterOwner->GetCapsuleComponent()
                        : nullptr;
                    if (Capsule && Capsule->IsA(Hitbox->PropertyClass))
                    {
                        Hitbox->SetObjectPropertyValue_InContainer(Component, Capsule);
                    }
                }
            }
        }
        ShowDebugMessage(TEXT("Hedge_Trimmer: charge + flame vulnerability enabled"), FColor::Green);
        return;
    }
    ShowDebugMessage(TEXT("Hedge_Trimmer: Damageable component missing"), FColor::Red);
}

void UMMAHedgeTrimmerBehaviorComponent::ConfigureDefaultDrop()
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
        FArrayProperty* ItemsProperty = FindFProperty<FArrayProperty>(
            Component->GetClass(), TEXT("Items_to_Drop"));
        FObjectPropertyBase* InnerObject = ItemsProperty
            ? CastField<FObjectPropertyBase>(ItemsProperty->Inner)
            : nullptr;
        if (!ItemsProperty || !InnerObject)
        {
            continue;
        }
        FScriptArrayHelper Items(ItemsProperty, ItemsProperty->ContainerPtrToValuePtr<void>(Component));
        if (Items.Num() == 0)
        {
            const int32 Index = Items.AddValue();
            InnerObject->SetObjectPropertyValue(Items.GetRawPtr(Index), DefaultDropClass.Get());
        }
        return;
    }
}

uint8 UMMAHedgeTrimmerBehaviorComponent::ReadInheritedAIState() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return 0;
    }
    uint8 State = 0;
    if (TryReadAIState(Owner, State) || TryReadAIState(FindWalkingAIObject(Owner), State))
    {
        return State;
    }
    return 0;
}

bool UMMAHedgeTrimmerBehaviorComponent::SetInheritedAIState(uint8 Value) const
{
    auto TrySet = [Value](UObject* Object)
    {
        if (!Object)
        {
            return false;
        }
        FProperty* StateProperty = FindFProperty<FProperty>(
            Object->GetClass(), TEXT("AICharacter_State"));
        if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(StateProperty))
        {
            EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                EnumProperty->ContainerPtrToValuePtr<void>(Object), static_cast<uint64>(Value));
            return true;
        }
        if (FByteProperty* ByteProperty = CastField<FByteProperty>(StateProperty))
        {
            ByteProperty->SetPropertyValue_InContainer(Object, Value);
            return true;
        }
        return false;
    };
    AActor* Owner = GetOwner();
    return TrySet(Owner) || TrySet(FindWalkingAIObject(Owner));
}

UPrimitiveComponent* UMMAHedgeTrimmerBehaviorComponent::FindDamageableHitbox(AActor* Actor)
{
    UActorComponent* Damageable = FindDamageableComponent(Actor);
    if (Damageable)
    {
        for (TFieldIterator<FObjectPropertyBase> It(Damageable->GetClass()); It; ++It)
        {
            FObjectPropertyBase* Property = *It;
            if (Property && NormalizePropertyName(Property->GetName()) == TEXT("objectshitboxcomponent"))
            {
                if (UPrimitiveComponent* Hitbox = Cast<UPrimitiveComponent>(
                        Property->GetObjectPropertyValue_InContainer(Damageable)))
                {
                    return Hitbox;
                }
            }
        }
    }
    return Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
}

UActorComponent* UMMAHedgeTrimmerBehaviorComponent::FindDamageableComponent(AActor* Actor)
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

uint8 UMMAHedgeTrimmerBehaviorComponent::ReadNativeDamageType() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return 1;
    }
    for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || NormalizePropertyName(Property->GetName()) != TEXT("damagetype"))
        {
            continue;
        }
        if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            return static_cast<uint8>(EnumProperty->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(
                EnumProperty->ContainerPtrToValuePtr<void>(Owner)));
        }
        if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            return ByteProperty->GetPropertyValue_InContainer(Owner);
        }
    }
    return 1;
}

bool UMMAHedgeTrimmerBehaviorComponent::DealNativeDamageToTarget(
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

    const uint8 DamageType = ReadNativeDamageType();
    TArray<uint8> Parameters;
    Parameters.SetNumZeroed(Function->ParmsSize);
    Function->InitializeStruct(Parameters.GetData());
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            continue;
        }
        const FString Name = NormalizePropertyName(Property->GetName());
        if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            if (Name.Contains(TEXT("damagetype")))
            {
                EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                    EnumProperty->ContainerPtrToValuePtr<void>(Parameters.GetData()),
                    static_cast<uint64>(DamageType));
            }
        }
        else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            if (Name.Contains(TEXT("damagetype")))
            {
                ByteProperty->SetPropertyValue_InContainer(Parameters.GetData(), DamageType);
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
            // Normal enemy attacks are neither super damage nor magnetized damage.
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
    double HitPointsBefore = 0.0;
    const bool bHadHitPointsBefore = TryReadHitPoints(Damageable, HitPointsBefore);
    Damageable->ProcessEvent(Function, Parameters.GetData());
    Function->DestroyStruct(Parameters.GetData());
    double HitPointsAfter = 0.0;
    const bool bHasHitPointsAfter = TryReadHitPoints(Damageable, HitPointsAfter);
    bOutDamageApplied = !bHadHitPointsBefore || !bHasHitPointsAfter || HitPointsAfter < HitPointsBefore;
    return true;
}

void UMMAHedgeTrimmerBehaviorComponent::ApplyHitRecoil(AActor* Target) const
{
    const AActor* Owner = GetOwner();
    ACharacter* TargetCharacter = Cast<ACharacter>(Target);
    if (!Owner || !TargetCharacter)
    {
        return;
    }
    FVector Away = Target->GetActorLocation() - Owner->GetActorLocation();
    Away.Z = 0.0f;
    if (!Away.Normalize())
    {
        Away = Owner->GetActorForwardVector();
        Away.Z = 0.0f;
        Away.Normalize();
    }
    FVector LaunchVelocity = Away * RecoilHorizontalSpeed;
    LaunchVelocity.Z = RecoilVerticalSpeed;
    TargetCharacter->LaunchCharacter(LaunchVelocity, true, true);
}

APawn* UMMAHedgeTrimmerBehaviorComponent::FindNearestPlayer(float Radius) const
{
    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }
    APawn* Best = nullptr;
    float BestSquared = Radius * Radius;
    for (int32 PlayerIndex = 0; PlayerIndex < MaximumLocalPlayersToCheck; ++PlayerIndex)
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
            const AAIController* AI = Cast<AAIController>(CharacterOwner.IsValid()
                ? CharacterOwner->GetController()
                : nullptr);
            if (AI && !AI->LineOfSightTo(Candidate))
            {
                continue;
            }
        }
        Best = Candidate;
        BestSquared = DistanceSquared;
    }
    return Best;
}

bool UMMAHedgeTrimmerBehaviorComponent::HasValidEngagementTarget() const
{
    const AActor* Owner = GetOwner();
    const APawn* Target = TargetPawn.Get();
    if (!Owner || !Target || Target->IsActorBeingDestroyed() || Target->IsHidden())
    {
        return false;
    }
    return FVector::DistSquared2D(Owner->GetActorLocation(), Target->GetActorLocation()) <=
        FMath::Square(LoseInterestRadius);
}

void UMMAHedgeTrimmerBehaviorComponent::FaceLocation(
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

void UMMAHedgeTrimmerBehaviorComponent::MoveTowardActor(
    APawn* Target,
    float DeltaTime) const
{
    ACharacter* Character = CharacterOwner.Get();
    if (!Character || !Target)
    {
        return;
    }
    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = ChaseSpeed;
    }
    FaceLocation(Target->GetActorLocation(), DeltaTime);
    FVector Direction = Target->GetActorLocation() - Character->GetActorLocation();
    Direction.Z = 0.0f;
    Character->AddMovementInput(Direction.GetSafeNormal(), 1.0f);
}

void UMMAHedgeTrimmerBehaviorComponent::MoveTowardHome(float DeltaTime) const
{
    ACharacter* Character = CharacterOwner.Get();
    if (!Character)
    {
        return;
    }
    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = ReturnHomeSpeed;
    }
    FaceLocation(HomeLocation, DeltaTime);
    FVector Direction = HomeLocation - Character->GetActorLocation();
    Direction.Z = 0.0f;
    Character->AddMovementInput(Direction.GetSafeNormal(), 1.0f);
}

void UMMAHedgeTrimmerBehaviorComponent::StopMovement() const
{
    ACharacter* Character = CharacterOwner.Get();
    if (!Character)
    {
        return;
    }
    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }
}

float UMMAHedgeTrimmerBehaviorComponent::GetStateDuration() const
{
    UAnimSequence* Animation = nullptr;
    switch (CurrentState)
    {
    case EMMAHedgeTrimmerState::Notice: Animation = NoticeAnimation; break;
    case EMMAHedgeTrimmerState::Attack: Animation = AttackAnimation; break;
    case EMMAHedgeTrimmerState::Dead: Animation = DeathAnimation; break;
    default: break;
    }
    return Animation ? FMath::Max(Animation->GetPlayLength(), MinimumOneShotDuration) : MinimumOneShotDuration;
}

void UMMAHedgeTrimmerBehaviorComponent::PlayStateAnimation()
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
    case EMMAHedgeTrimmerState::Idle: Animation = IdleAnimation; bLoop = true; break;
    case EMMAHedgeTrimmerState::Notice: Animation = NoticeAnimation; break;
    case EMMAHedgeTrimmerState::Chase: Animation = ChaseAnimation; bLoop = true; break;
    case EMMAHedgeTrimmerState::Attack: Animation = AttackAnimation; break;
    case EMMAHedgeTrimmerState::ReturnHome: Animation = ReturnHomeAnimation; bLoop = true; break;
    case EMMAHedgeTrimmerState::Dead: Animation = DeathAnimation; break;
    }
    if (Animation)
    {
        Mesh->PlayAnimation(Animation, bLoop);
    }
}

void UMMAHedgeTrimmerBehaviorComponent::EnterState(EMMAHedgeTrimmerState NewState)
{
    const EMMAHedgeTrimmerState Previous = CurrentState;
    CurrentState = NewState;
    StateElapsedSeconds = 0.0f;
    bAttackHitApplied = false;

    // Keep the generic club AI dormant for every living bespoke state. Its
    // state-2 graph assumes Get Nearest Player I'm Alert To is valid.
    switch (NewState)
    {
    case EMMAHedgeTrimmerState::Idle:
    case EMMAHedgeTrimmerState::ReturnHome:
        SetInheritedAIState(0);
        SetInheritedBool(GetOwner(), TEXT("WeaponHitboxActive"), false);
        break;
    case EMMAHedgeTrimmerState::Notice:
    case EMMAHedgeTrimmerState::Chase:
    case EMMAHedgeTrimmerState::Attack:
        SetInheritedAIState(0);
        SetInheritedBool(GetOwner(), TEXT("WeaponHitboxActive"), false);
        break;
    case EMMAHedgeTrimmerState::Dead:
        SetInheritedBool(GetOwner(), TEXT("WeaponHitboxActive"), false);
        break;
    }
    if (NewState == EMMAHedgeTrimmerState::Idle ||
        NewState == EMMAHedgeTrimmerState::Notice ||
        NewState == EMMAHedgeTrimmerState::Attack ||
        NewState == EMMAHedgeTrimmerState::Dead)
    {
        StopMovement();
    }
    PlayStateAnimation();
    ShowDebugMessage(FString::Printf(
        TEXT("Hedge_Trimmer state: %s"),
        *UEnum::GetValueAsString(NewState)));
    if (Previous != NewState)
    {
        OnStateChanged.Broadcast(Previous, NewState);
    }
}

void UMMAHedgeTrimmerBehaviorComponent::ApplyAttackHit()
{
    bAttackHitApplied = true;
    AActor* Owner = GetOwner();
    APawn* Target = TargetPawn.Get();
    if (!Owner || !Target)
    {
        ShowDebugMessage(TEXT("Hedge_Trimmer attack: no target"), FColor::Yellow);
        return;
    }
    FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
    ToTarget.Z = 0.0f;
    if (ToTarget.SizeSquared() > FMath::Square(AttackHitRange))
    {
        ShowDebugMessage(FString::Printf(
            TEXT("Hedge_Trimmer attack MISS (range %.0f / %.0f)"),
            ToTarget.Size(), AttackHitRange), FColor::Yellow);
        return;
    }
    const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(AttackHalfAngleDegrees));
    if (FVector::DotProduct(Owner->GetActorForwardVector(), ToTarget.GetSafeNormal()) < MinimumDot)
    {
        ShowDebugMessage(TEXT("Hedge_Trimmer attack MISS (outside attack cone)"), FColor::Yellow);
        return;
    }

    bool bDamageApplied = false;
    if (!DealNativeDamageToTarget(Target, bDamageApplied))
    {
        ShowDebugMessage(TEXT("Hedge_Trimmer attack ERROR (Spyro Damageable/Deal Damage missing)"), FColor::Red);
        return;
    }
    if (bDamageApplied)
    {
        ApplyHitRecoil(Target);
        ShowDebugMessage(TEXT("Hedge_Trimmer attack: 1 hit + recoil"), FColor::Green);
    }
    else
    {
        ShowDebugMessage(TEXT("Hedge_Trimmer attack resisted: no recoil"), FColor::Yellow);
    }
}

void UMMAHedgeTrimmerBehaviorComponent::ForceReturnHome()
{
    if (CurrentState != EMMAHedgeTrimmerState::Dead)
    {
        TargetPawn.Reset();
        EnterState(EMMAHedgeTrimmerState::ReturnHome);
    }
}

void UMMAHedgeTrimmerBehaviorComponent::TickComponent(
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
    if (ReadInheritedAIState() == DeadAIState)
    {
        if (CurrentState != EMMAHedgeTrimmerState::Dead)
        {
            EnterState(EMMAHedgeTrimmerState::Dead);
        }
        return;
    }

    StateElapsedSeconds += DeltaTime;
    AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaTime);

    switch (CurrentState)
    {
    case EMMAHedgeTrimmerState::Idle:
        TargetPawn = FindNearestPlayer(DetectionRadius);
        if (TargetPawn.IsValid())
        {
            EnterState(EMMAHedgeTrimmerState::Notice);
        }
        break;

    case EMMAHedgeTrimmerState::Notice:
        if (TargetPawn.IsValid())
        {
            FaceLocation(TargetPawn->GetActorLocation(), DeltaTime);
        }
        if (StateElapsedSeconds >= GetStateDuration())
        {
            EnterState(HasValidEngagementTarget()
                ? EMMAHedgeTrimmerState::Chase
                : EMMAHedgeTrimmerState::ReturnHome);
        }
        break;

    case EMMAHedgeTrimmerState::Chase:
    {
        const float DistanceFromHome = FVector::Dist2D(Owner->GetActorLocation(), HomeLocation);
        if (!HasValidEngagementTarget() || DistanceFromHome >= MaximumDistanceFromHome)
        {
            TargetPawn.Reset();
            EnterState(EMMAHedgeTrimmerState::ReturnHome);
            break;
        }
        const float TargetDistance = FVector::Dist2D(
            Owner->GetActorLocation(), TargetPawn->GetActorLocation());
        if (TargetDistance <= AttackRange && AttackCooldownRemaining <= 0.0f)
        {
            EnterState(EMMAHedgeTrimmerState::Attack);
        }
        else
        {
            MoveTowardActor(TargetPawn.Get(), DeltaTime);
        }
        break;
    }

    case EMMAHedgeTrimmerState::Attack:
        if (TargetPawn.IsValid())
        {
            FaceLocation(TargetPawn->GetActorLocation(), DeltaTime);
        }
        if (!bAttackHitApplied && StateElapsedSeconds >= AttackContactSeconds)
        {
            ApplyAttackHit();
        }
        if (StateElapsedSeconds >= GetStateDuration())
        {
            AttackCooldownRemaining = AttackCooldownSeconds;
            EnterState(HasValidEngagementTarget()
                ? EMMAHedgeTrimmerState::Chase
                : EMMAHedgeTrimmerState::ReturnHome);
        }
        break;

    case EMMAHedgeTrimmerState::ReturnHome:
        if (FVector::Dist2D(Owner->GetActorLocation(), HomeLocation) <= HomeAcceptanceRadius)
        {
            StopMovement();
            Owner->SetActorLocation(FVector(
                HomeLocation.X, HomeLocation.Y, Owner->GetActorLocation().Z));
            EnterState(EMMAHedgeTrimmerState::Idle);
        }
        else
        {
            MoveTowardHome(DeltaTime);
        }
        break;

    case EMMAHedgeTrimmerState::Dead:
        break;
    }
}
