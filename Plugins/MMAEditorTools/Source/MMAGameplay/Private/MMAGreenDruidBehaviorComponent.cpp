#include "MMAGreenDruidBehaviorComponent.h"

#include "MMAGreenDruidPlatform.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/UnrealType.h"

namespace
{
// These names must remain unique across MMAGameplay .cpp files because UE4's
// unity build concatenates translation units into Module.MMAGameplay.cpp.
constexpr uint8 GreenDruidDeadAIState = 3;
constexpr int32 GreenDruidMaximumLocalPlayersToCheck = 8;
}

UMMAGreenDruidBehaviorComponent::UMMAGreenDruidBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UMMAGreenDruidBehaviorComponent::BeginPlay()
{
    Super::BeginPlay();
    CharacterOwner = Cast<ACharacter>(GetOwner());
    MeshComponent = CharacterOwner.IsValid() ? CharacterOwner->GetMesh() : nullptr;

    ConfigureNativeEnemyContract();
    ConfigureIncomingDamageContract();
    ConfigureDefaultDrop();

    ClaimedPlatforms.Reset();
    for (AMMAGreenDruidPlatform* Platform : ControlledPlatforms)
    {
        if (IsValid(Platform) && Platform->ClaimController(this))
        {
            ClaimedPlatforms.Add(Platform);
            Platform->ForceFlat();
        }
    }
    if (ClaimedPlatforms.Num() == 0 && !bMissingLinksWarningIssued)
    {
        bMissingLinksWarningIssued = true;
        UE_LOG(LogTemp, Warning, TEXT("Green Druid %s has no valid controlled platforms."),
            GetOwner() ? *GetOwner()->GetName() : TEXT("<null>"));
    }

    // Persistence can mark the duplicated Base_Enemy contract dead before BeginPlay.
    // Flatten before the first rendered frame and never permit reactivation.
    bDeathObserved = IsOwnerDefeated();
    EnterState(bDeathObserved
        ? EMMAGreenDruidState::DeadReturningFlat
        : EMMAGreenDruidState::Inactive);
    ApplyLiftAlpha();
}

void UMMAGreenDruidBehaviorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (const TWeakObjectPtr<AMMAGreenDruidPlatform>& Platform : ClaimedPlatforms)
    {
        if (Platform.IsValid())
        {
            Platform->ForceFlat();
            Platform->ReleaseController(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void UMMAGreenDruidBehaviorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetOwner())
    {
        return;
    }

    if (!bDeathObserved && IsOwnerDefeated())
    {
        bDeathObserved = true;
        EnterState(EMMAGreenDruidState::DeadReturningFlat);
    }

    const float SafeTransition = FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER);
    const float AlphaStep = DeltaTime / SafeTransition;
    StateElapsedSeconds += DeltaTime;

    if (bDeathObserved)
    {
        LiftAlpha = FMath::Max(0.0f, LiftAlpha - AlphaStep);
        ApplyLiftAlpha();
        return;
    }

    const bool bPlayerInActivationRange = HasPlayerWithin(ActivationRadius);
    const bool bPlayerInDeactivationRange = HasPlayerWithin(
        FMath::Max(ActivationRadius, DeactivationRadius));

    switch (CurrentState)
    {
    case EMMAGreenDruidState::Inactive:
        if (LiftAlpha > 0.0f)
        {
            if (bPlayerInActivationRange)
            {
                bCycleEngaged = true;
                bFlatHoldPending = false;
                EnterState(EMMAGreenDruidState::Raising);
            }
            else
            {
                LiftAlpha = FMath::Max(0.0f, LiftAlpha - AlphaStep);
            }
        }
        else if (bCycleEngaged && bFlatHoldPending)
        {
            if (!bPlayerInDeactivationRange)
            {
                bCycleEngaged = false;
                bFlatHoldPending = false;
            }
            else if (StateElapsedSeconds >= FlatHoldDuration)
            {
                bFlatHoldPending = false;
                EnterState(EMMAGreenDruidState::Raising);
            }
        }
        else if (bPlayerInActivationRange)
        {
            // A fresh approach raises immediately; the flat hold only occurs
            // between complete cycles.
            bCycleEngaged = true;
            EnterState(EMMAGreenDruidState::Raising);
        }
        break;

    case EMMAGreenDruidState::Raising:
        if (!bPlayerInDeactivationRange)
        {
            bCycleEngaged = false;
            EnterState(EMMAGreenDruidState::Inactive);
            break;
        }
        LiftAlpha = FMath::Min(1.0f, LiftAlpha + AlphaStep);
        if (LiftAlpha >= 1.0f)
        {
            EnterState(EMMAGreenDruidState::RaisedHold);
        }
        break;

    case EMMAGreenDruidState::RaisedHold:
        if (!bPlayerInDeactivationRange)
        {
            bCycleEngaged = false;
            EnterState(EMMAGreenDruidState::Inactive);
        }
        else if (StateElapsedSeconds >= RaisedHoldDuration)
        {
            EnterState(EMMAGreenDruidState::Lowering);
        }
        break;

    case EMMAGreenDruidState::Lowering:
        if (!bPlayerInDeactivationRange)
        {
            bCycleEngaged = false;
            EnterState(EMMAGreenDruidState::Inactive);
            break;
        }
        LiftAlpha = FMath::Max(0.0f, LiftAlpha - AlphaStep);
        if (LiftAlpha <= 0.0f)
        {
            bFlatHoldPending = true;
            EnterState(EMMAGreenDruidState::Inactive);
        }
        break;

    case EMMAGreenDruidState::DeadReturningFlat:
        break;
    }

    ApplyLiftAlpha();
}

void UMMAGreenDruidBehaviorComponent::EnterState(EMMAGreenDruidState NewState)
{
    const EMMAGreenDruidState Previous = CurrentState;
    CurrentState = NewState;
    StateElapsedSeconds = 0.0f;
    PlayStateAnimation();

    const bool bChanneling = NewState == EMMAGreenDruidState::Raising ||
        NewState == EMMAGreenDruidState::Lowering;
    if (ChannelParticle)
    {
        UParticleSystemComponent* Particle = ChannelParticleComponent.Get();
        if (bChanneling && !Particle)
        {
            Particle = UGameplayStatics::SpawnEmitterAttached(
                ChannelParticle, GetOwner()->GetRootComponent());
            ChannelParticleComponent = Particle;
        }
        if (Particle)
        {
            bChanneling ? Particle->ActivateSystem() : Particle->DeactivateSystem();
        }
    }
    if (Previous != NewState)
    {
        OnStateChanged.Broadcast(Previous, NewState);
    }
}

void UMMAGreenDruidBehaviorComponent::ApplyLiftAlpha()
{
    for (int32 Index = ClaimedPlatforms.Num() - 1; Index >= 0; --Index)
    {
        if (ClaimedPlatforms[Index].IsValid())
        {
            ClaimedPlatforms[Index]->SetLiftAlpha(LiftAlpha);
        }
        else
        {
            ClaimedPlatforms.RemoveAtSwap(Index, 1, false);
        }
    }
}

void UMMAGreenDruidBehaviorComponent::ForceAllPlatformsFlat()
{
    LiftAlpha = 0.0f;
    ApplyLiftAlpha();
}

void UMMAGreenDruidBehaviorComponent::PlayStateAnimation()
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
    case EMMAGreenDruidState::Inactive:
    case EMMAGreenDruidState::RaisedHold:
        Animation = IdleAnimation;
        bLoop = true;
        break;
    case EMMAGreenDruidState::Raising:
        Animation = RaiseAnimation;
        break;
    case EMMAGreenDruidState::Lowering:
        Animation = LowerAnimation ? LowerAnimation : RaiseAnimation;
        break;
    case EMMAGreenDruidState::DeadReturningFlat:
        Animation = DeathAnimation;
        break;
    }
    if (!Animation)
    {
        return;
    }
    Mesh->PlayAnimation(Animation, bLoop);
    if (CurrentState == EMMAGreenDruidState::Lowering && bPlayLowerAnimationReversed)
    {
        if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
        {
            Instance->SetPosition(Animation->GetPlayLength(), false);
            Instance->SetPlayRate(-1.0f);
        }
    }
}

void UMMAGreenDruidBehaviorComponent::ConfigureNativeEnemyContract()
{
    AActor* Owner = GetOwner();
    SetInheritedBool(Owner, TEXT("Can_Become_Alert"), false);
    SetInheritedBool(Owner, TEXT("Can_Attack"), false);
    SetInheritedFloat(Owner, TEXT("Corpse_Poof_Delay"),
        FMath::Max(TransitionDuration, DeathAnimation ? DeathAnimation->GetPlayLength() : 0.0f) + 0.25f);
    if (ACharacter* Character = CharacterOwner.Get())
    {
        Character->bUseControllerRotationYaw = false;
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->MaxWalkSpeed = 0.0f;
            Movement->DisableMovement();
        }
    }
}

void UMMAGreenDruidBehaviorComponent::ConfigureIncomingDamageContract()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    Owner->SetCanBeDamaged(true);
    UActorComponent* Damageable = FindDamageableComponent(Owner);
    if (!Damageable)
    {
        UE_LOG(LogTemp, Warning, TEXT("Green Druid %s has no Damageable component."), *Owner->GetName());
        return;
    }
    for (TFieldIterator<FProperty> It(Damageable->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        const FString Name = Property ? NormalizePropertyName(Property->GetName()) : FString();
        if (Name == TEXT("invincible"))
        {
            if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
            {
                BoolProperty->SetPropertyValue_InContainer(Damageable, false);
            }
        }
        else if (Name == TEXT("damageresistances"))
        {
            if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
            {
                FScriptArrayHelper Values(ArrayProperty,
                    ArrayProperty->ContainerPtrToValuePtr<void>(Damageable));
                Values.EmptyValues();
            }
        }
        else if (Name == TEXT("objectshitboxcomponent"))
        {
            if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
            {
                UPrimitiveComponent* Capsule = CharacterOwner.IsValid()
                    ? CharacterOwner->GetCapsuleComponent() : nullptr;
                if (Capsule && Capsule->IsA(ObjectProperty->PropertyClass))
                {
                    ObjectProperty->SetObjectPropertyValue_InContainer(Damageable, Capsule);
                }
            }
        }
    }
    TryWriteHitPoints(Damageable, InitialHitPoints);
}

void UMMAGreenDruidBehaviorComponent::ConfigureDefaultDrop()
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
            ? CastField<FObjectPropertyBase>(ItemsProperty->Inner) : nullptr;
        if (!ItemsProperty || !InnerObject)
        {
            continue;
        }
        FScriptArrayHelper Items(ItemsProperty,
            ItemsProperty->ContainerPtrToValuePtr<void>(Component));
        if (Items.Num() == 0)
        {
            const int32 Index = Items.AddValue();
            InnerObject->SetObjectPropertyValue(Items.GetRawPtr(Index), DefaultDropClass.Get());
        }
        return;
    }
}

bool UMMAGreenDruidBehaviorComponent::IsOwnerDefeated() const
{
    uint8 State = 0;
    if (TryReadAIState(GetOwner(), State) || TryReadAIState(FindWalkingAIObject(GetOwner()), State))
    {
        if (State == GreenDruidDeadAIState)
        {
            return true;
        }
    }
    double HitPoints = 0.0;
    UActorComponent* Damageable = FindDamageableComponent(GetOwner());
    return Damageable && TryReadHitPoints(Damageable, HitPoints) && HitPoints <= 0.0;
}

bool UMMAGreenDruidBehaviorComponent::HasPlayerWithin(float Radius) const
{
    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }
    const float RadiusSquared = FMath::Square(FMath::Max(0.0f, Radius));
    for (int32 PlayerIndex = 0; PlayerIndex < GreenDruidMaximumLocalPlayersToCheck; ++PlayerIndex)
    {
        APawn* Player = UGameplayStatics::GetPlayerPawn(this, PlayerIndex);
        if (Player && Player != Owner && !Player->IsActorBeingDestroyed() && !Player->IsHidden() &&
            FVector::DistSquared2D(Owner->GetActorLocation(), Player->GetActorLocation()) <= RadiusSquared)
        {
            return true;
        }
    }
    return false;
}

UObject* UMMAGreenDruidBehaviorComponent::FindWalkingAIObject(AActor* Owner)
{
    if (!Owner)
    {
        return nullptr;
    }
    if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
        Owner->GetClass(), TEXT("Walking_AI_Character")))
    {
        return Property->GetObjectPropertyValue_InContainer(Owner);
    }
    return nullptr;
}

UActorComponent* UMMAGreenDruidBehaviorComponent::FindDamageableComponent(AActor* Owner)
{
    if (!Owner)
    {
        return nullptr;
    }
    TArray<UActorComponent*> Components;
    Owner->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (Component && (Component->GetClass()->GetName().Contains(TEXT("Damageable_Com")) ||
            Component->FindFunction(TEXT("Deal Damage")) || Component->FindFunction(TEXT("Deal_Damage"))))
        {
            return Component;
        }
    }
    return nullptr;
}

FString UMMAGreenDruidBehaviorComponent::NormalizePropertyName(FString Name)
{
    Name.ReplaceInline(TEXT("_"), TEXT(""));
    Name.ReplaceInline(TEXT(" "), TEXT(""));
    Name.ReplaceInline(TEXT("'"), TEXT(""));
    return Name.ToLower();
}

bool UMMAGreenDruidBehaviorComponent::TryReadHitPoints(UObject* Object, double& OutValue)
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
        if (FIntProperty* P = CastField<FIntProperty>(Property)) { OutValue = P->GetPropertyValue_InContainer(Object); return true; }
        if (FFloatProperty* P = CastField<FFloatProperty>(Property)) { OutValue = P->GetPropertyValue_InContainer(Object); return true; }
        if (FDoubleProperty* P = CastField<FDoubleProperty>(Property)) { OutValue = P->GetPropertyValue_InContainer(Object); return true; }
        if (FByteProperty* P = CastField<FByteProperty>(Property)) { OutValue = P->GetPropertyValue_InContainer(Object); return true; }
    }
    return false;
}

bool UMMAGreenDruidBehaviorComponent::TryWriteHitPoints(UObject* Object, double Value)
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
        if (FIntProperty* P = CastField<FIntProperty>(Property)) { P->SetPropertyValue_InContainer(Object, FMath::RoundToInt(Value)); return true; }
        if (FFloatProperty* P = CastField<FFloatProperty>(Property)) { P->SetPropertyValue_InContainer(Object, Value); return true; }
        if (FDoubleProperty* P = CastField<FDoubleProperty>(Property)) { P->SetPropertyValue_InContainer(Object, Value); return true; }
        if (FByteProperty* P = CastField<FByteProperty>(Property)) { P->SetPropertyValue_InContainer(Object, FMath::Clamp(FMath::RoundToInt(Value), 0, 255)); return true; }
    }
    return false;
}

bool UMMAGreenDruidBehaviorComponent::TryReadAIState(UObject* Object, uint8& OutState)
{
    if (!Object)
    {
        return false;
    }
    FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), TEXT("AICharacter_State"));
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        OutState = static_cast<uint8>(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(
            EnumProperty->ContainerPtrToValuePtr<void>(Object)));
        return true;
    }
    if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
    {
        OutState = ByteProperty->GetPropertyValue_InContainer(Object);
        return true;
    }
    return false;
}

void UMMAGreenDruidBehaviorComponent::SetInheritedBool(AActor* Owner, FName PropertyName, bool Value)
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
    UObject* WalkingAI = FindWalkingAIObject(Owner);
    if (WalkingAI)
    {
        if (FBoolProperty* Property = FindFProperty<FBoolProperty>(WalkingAI->GetClass(), PropertyName))
        {
            Property->SetPropertyValue_InContainer(WalkingAI, Value);
        }
    }
}

bool UMMAGreenDruidBehaviorComponent::SetInheritedFloat(
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
