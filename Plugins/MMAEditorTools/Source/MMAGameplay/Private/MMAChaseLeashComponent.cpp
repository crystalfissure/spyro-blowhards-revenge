#include "MMAChaseLeashComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/SphereComponent.h"
#include "UObject/UnrealType.h"

UMMAChaseLeashComponent::UMMAChaseLeashComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UMMAChaseLeashComponent::BeginPlay()
{
    Super::BeginPlay();
    AActor* Owner = GetOwner();
    HomeLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
    if (Owner)
    {
        if (FBoolProperty* CanAlert = FindFProperty<FBoolProperty>(
                Owner->GetClass(), TEXT("Can_Become_Alert")))
        {
            bOriginalCanBecomeAlert = CanAlert->GetPropertyValue_InContainer(Owner);
            bCanBecomeAlertWasFound = true;
        }
        bSpawnPointWasFound = FindSpawnPointOwner() != nullptr;
        LockInheritedSpawnPointToHome();
    }
}

uint8 UMMAChaseLeashComponent::ReadAIState() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return 0;
    }

    FProperty* StateProperty = FindFProperty<FProperty>(Owner->GetClass(), TEXT("AICharacter_State"));
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(StateProperty))
    {
        return static_cast<uint8>(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(
            EnumProperty->ContainerPtrToValuePtr<void>(Owner)));
    }
    if (FByteProperty* ByteProperty = CastField<FByteProperty>(StateProperty))
    {
        return ByteProperty->GetPropertyValue_InContainer(Owner);
    }
    return 0;
}

void UMMAChaseLeashComponent::ClearAlertedPlayers() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    FArrayProperty* AlertedPlayers = FindFProperty<FArrayProperty>(
        Owner->GetClass(), TEXT("Players_I_Am_Alert_To"));
    if (!AlertedPlayers)
    {
        return;
    }
    FScriptArrayHelper Helper(
        AlertedPlayers,
        AlertedPlayers->ContainerPtrToValuePtr<void>(Owner));
    Helper.EmptyValues();
}

void UMMAChaseLeashComponent::SetCanBecomeAlert(bool bEnabled) const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    if (FBoolProperty* CanAlert = FindFProperty<FBoolProperty>(
            Owner->GetClass(), TEXT("Can_Become_Alert")))
    {
        CanAlert->SetPropertyValue_InContainer(Owner, bEnabled);
    }
}

UObject* UMMAChaseLeashComponent::FindSpawnPointOwner() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }
    if (FStructProperty* SpawnPoint = FindFProperty<FStructProperty>(
            Owner->GetClass(), TEXT("Spawn_Point")))
    {
        if (SpawnPoint->Struct == TBaseStructure<FVector>::Get())
        {
            return Owner;
        }
    }

    // ChargingAttackEnemy_Base_BP stores Spawn_Point on its nested
    // Walking_AI_Character object, rather than on the enemy actor itself.
    if (FObjectPropertyBase* WalkingAI = FindFProperty<FObjectPropertyBase>(
            Owner->GetClass(), TEXT("Walking_AI_Character")))
    {
        UObject* Candidate = WalkingAI->GetObjectPropertyValue_InContainer(Owner);
        if (Candidate)
        {
            if (FStructProperty* SpawnPoint = FindFProperty<FStructProperty>(
                    Candidate->GetClass(), TEXT("Spawn_Point")))
            {
                if (SpawnPoint->Struct == TBaseStructure<FVector>::Get())
                {
                    return Candidate;
                }
            }
        }
    }
    return nullptr;
}

void UMMAChaseLeashComponent::LockInheritedSpawnPointToHome() const
{
    UObject* SpawnPointOwner = FindSpawnPointOwner();
    if (!SpawnPointOwner)
    {
        return;
    }
    if (FStructProperty* SpawnPoint = FindFProperty<FStructProperty>(
            SpawnPointOwner->GetClass(), TEXT("Spawn_Point")))
    {
        if (SpawnPoint->Struct == TBaseStructure<FVector>::Get())
        {
            *SpawnPoint->ContainerPtrToValuePtr<FVector>(SpawnPointOwner) = HomeLocation;
        }
    }
}

void UMMAChaseLeashComponent::ClampToHomeRadius() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    const FVector CurrentLocation = Owner->GetActorLocation();
    FVector HorizontalOffset = CurrentLocation - HomeLocation;
    HorizontalOffset.Z = 0.0f;
    const float DistanceFromHome = HorizontalOffset.Size();
    if (DistanceFromHome <= MaximumDistanceFromHome || DistanceFromHome <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // This is the final authority for the leash. The base AI can issue a
    // movement request in the same frame as an alert overlap, so simply
    // changing state can allow it to drift past the configured radius.
    const FVector ClampedLocation = HomeLocation +
        HorizontalOffset * (MaximumDistanceFromHome / DistanceFromHome);
    Owner->SetActorLocation(
        FVector(ClampedLocation.X, ClampedLocation.Y, CurrentLocation.Z),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    if (ACharacter* Character = Cast<ACharacter>(Owner))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
    }
}

void UMMAChaseLeashComponent::MoveDirectlyTowardHome(float DeltaTime) const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    const FVector Current = Owner->GetActorLocation();
    FVector Offset = HomeLocation - Current;
    Offset.Z = 0.0f;
    const float Distance = Offset.Size();
    if (Distance <= KINDA_SMALL_NUMBER)
    {
        return;
    }
    const float Step = FMath::Min(ForcedReturnSpeed * DeltaTime, Distance);
    const FVector Next = Current + Offset.GetSafeNormal() * Step;
    // Parent Run At Player can remain active even after state 0. Drive the
    // return transform directly so that faulty parent movement cannot win.
    Owner->SetActorLocation(FVector(Next.X, Next.Y, Current.Z), false, nullptr,
        ETeleportType::TeleportPhysics);
}

bool UMMAChaseLeashComponent::IsSpyroInsideAlertRadius() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }
    TArray<USphereComponent*> Spheres;
    Owner->GetComponents(Spheres);
    for (USphereComponent* Sphere : Spheres)
    {
        if (!Sphere->GetName().Contains(TEXT("Alert_Radius")))
        {
            continue;
        }
        TArray<AActor*> OverlappingActors;
        Sphere->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
        for (AActor* Actor : OverlappingActors)
        {
            if (Actor && Actor != Owner)
            {
                return true;
            }
        }
    }
    return false;
}

void UMMAChaseLeashComponent::SetProximitySpheresSuppressed(bool bSuppressed)
{
    if (bSuppressed == bProximitySpheresSuppressed)
    {
        return;
    }
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    if (bSuppressed)
    {
        TArray<USphereComponent*> Spheres;
        Owner->GetComponents(Spheres);
        for (USphereComponent* Sphere : Spheres)
        {
            const FString Name = Sphere->GetName();
            if (Name.Contains(TEXT("Alert_Radius")) ||
                Name.Contains(TEXT("Attack_Radius")) ||
                Name.Contains(TEXT("Attacking_Spyro_Radius")))
            {
                SuppressedProximitySpheres.Add(Sphere);
                Sphere->SetGenerateOverlapEvents(false);
                Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
    }
    else
    {
        for (const TWeakObjectPtr<USphereComponent>& Sphere : SuppressedProximitySpheres)
        {
            if (Sphere.IsValid())
            {
                Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Sphere->SetGenerateOverlapEvents(true);
            }
        }
        SuppressedProximitySpheres.Empty();
    }
    bProximitySpheresSuppressed = bSuppressed;
}

bool UMMAChaseLeashComponent::ChangeAIState(uint8 NewState) const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    UFunction* Function = Owner->FindFunction(TEXT("Change AICharacter State"));
    if (!Function)
    {
        Function = Owner->FindFunction(TEXT("Change_AICharacter_State"));
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
        if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                EnumProperty->ContainerPtrToValuePtr<void>(Parameters.GetData()),
                static_cast<int64>(NewState));
            break;
        }
        if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            ByteProperty->SetPropertyValue_InContainer(Parameters.GetData(), NewState);
            break;
        }
    }
    Owner->ProcessEvent(Function, Parameters.GetData());
    Function->DestroyStruct(Parameters.GetData());
    return true;
}

void UMMAChaseLeashComponent::TickComponent(
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

    const uint8 State = ReadAIState();
    const float DistanceFromHome = FVector::Dist2D(Owner->GetActorLocation(), HomeLocation);

    if (bForcingReturnHome)
    {
        ClampToHomeRadius();
        MoveDirectlyTowardHome(DeltaTime);
        if (DistanceFromHome <= HomeAcceptanceRadius)
        {
            SetProximitySpheresSuppressed(false);
            if (bCanBecomeAlertWasFound)
            {
                SetCanBecomeAlert(bOriginalCanBecomeAlert);
            }
            bForcingReturnHome = false;
            bWasChasing = false;
            bEngagementActive = false;
            ChaseElapsedSeconds = 0.0f;
            return;
        }
        // The base enemy Blueprint reacts to Alert_Radius overlap events. It
        // can otherwise re-add Spyro immediately after ClearAlertedPlayers,
        // overriding this component's home-distance safety rule.
        SetProximitySpheresSuppressed(true);
        if (bCanBecomeAlertWasFound)
        {
            SetCanBecomeAlert(false);
        }
        // State 0 returns through the inherited Spawn_Point variable. Keep it
        // pinned to the original placed transform in case base alert logic
        // attempted to update it while the enemy was chasing.
        LockInheritedSpawnPointToHome();
        ClearAlertedPlayers();
        if (State != 0)
        {
            ChangeAIState(0);
        }
        return;
    }

    // Do not rely solely on the parent state enum: ChargeAndAttack can chase
    // without entering its expected attack state until it damages Spyro.
    const bool bEngaged = IsSpyroInsideAlertRadius() || (State != 0 && State != 3);
    if (bEngaged)
    {
        ChaseElapsedSeconds = bEngagementActive ? ChaseElapsedSeconds + DeltaTime : 0.0f;
        bEngagementActive = true;
    }
    else
    {
        bEngagementActive = false;
        ChaseElapsedSeconds = 0.0f;
    }

    if (bEngagementActive && (DistanceFromHome >= MaximumDistanceFromHome ||
        ChaseElapsedSeconds >= MaximumChaseSeconds)
    )
    {
        ClampToHomeRadius();
        SetProximitySpheresSuppressed(true);
        if (bCanBecomeAlertWasFound)
        {
            SetCanBecomeAlert(false);
        }
        LockInheritedSpawnPointToHome();
        ClearAlertedPlayers();
        ChangeAIState(0);
        bForcingReturnHome = true;
        ChaseElapsedSeconds = 0.0f;
    }
}
