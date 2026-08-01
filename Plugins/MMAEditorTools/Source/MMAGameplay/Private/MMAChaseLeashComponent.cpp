#include "MMAChaseLeashComponent.h"

#include "GameFramework/Actor.h"
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
        if (DistanceFromHome <= HomeAcceptanceRadius)
        {
            bForcingReturnHome = false;
            bWasChasing = false;
            ChaseElapsedSeconds = 0.0f;
            return;
        }
        ClearAlertedPlayers();
        if (State != 0)
        {
            ChangeAIState(0);
        }
        return;
    }

    // State 1 is alerted/running at the player; state 2 is the attack phase.
    // Count both so the leash starts when the enemy spots Spyro.
    const bool bChasing = State == 1 || State == 2;
    if (bChasing)
    {
        ChaseElapsedSeconds = bWasChasing ? ChaseElapsedSeconds + DeltaTime : 0.0f;
        bWasChasing = true;
        if (ChaseElapsedSeconds >= MaximumChaseSeconds ||
            DistanceFromHome >= MaximumDistanceFromHome)
        {
            ClearAlertedPlayers();
            ChangeAIState(0);
            bForcingReturnHome = true;
            ChaseElapsedSeconds = 0.0f;
        }
    }
    else
    {
        bWasChasing = false;
        ChaseElapsedSeconds = 0.0f;
    }
}
