#include "TownSquareEnemyBehaviorComponent.h"
#include "TownSquareEnemyAnimInstance.h"
#include "GameFramework/Controller.h"

#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/ConstructorHelpers.h"

namespace TownSquare
{
FString Key(FString Name)
{
    Name = Name.ToLower();
    Name.ReplaceInline(TEXT(" "), TEXT(""));
    Name.ReplaceInline(TEXT("_"), TEXT(""));
    Name.ReplaceInline(TEXT("'"), TEXT(""));
    return Name;
}
FProperty* Property(UObject* Object, const TCHAR* Name)
{
    if (Object) for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
        if (Key(It->GetName()) == Key(Name)) return *It;
    return nullptr;
}
bool Bool(UObject* Object, const TCHAR* Name)
{
    auto* P = CastField<FBoolProperty>(Property(Object, Name));
    return P && P->GetPropertyValue_InContainer(Object);
}
void SetBool(UObject* Object, const TCHAR* Name, bool Value)
{
    if (auto* P = CastField<FBoolProperty>(Property(Object, Name))) P->SetPropertyValue_InContainer(Object, Value);
}
int32 Number(UObject* Object, const TCHAR* Name)
{
    auto* P = CastField<FNumericProperty>(Property(Object, Name));
    if (!P) return 0;
    const void* V = P->ContainerPtrToValuePtr<void>(Object);
    return P->IsInteger() ? int32(P->GetSignedIntPropertyValue(V)) : int32(P->GetFloatingPointPropertyValue(V));
}
void SetNumber(UObject* Object, const TCHAR* Name, double Value)
{
    if (auto* P = CastField<FNumericProperty>(Property(Object, Name)))
    {
        void* V = P->ContainerPtrToValuePtr<void>(Object);
        if (P->IsInteger()) P->SetIntPropertyValue(V, int64(Value));
        else P->SetFloatingPointPropertyValue(V, Value);
    }
}
UObject* ObjectValue(UObject* Object, const TCHAR* Name)
{
    auto* P = CastField<FObjectPropertyBase>(Property(Object, Name));
    return P ? P->GetObjectPropertyValue_InContainer(Object) : nullptr;
}
void Call(UObject* Object, const TCHAR* Name)
{
    if (Object) if (UFunction* F = Object->FindFunction(Name))
    {
        FStructOnScope Params(F);
        Object->ProcessEvent(F, Params.GetStructMemory());
    }
}
void Bind(UObject* Source, const TCHAR* Name, UObject* Target, FName Function, bool bRemove = false)
{
    if (auto* P = CastField<FMulticastDelegateProperty>(Property(Source, Name)))
    {
        FScriptDelegate Delegate;
        Delegate.BindUFunction(Target, Function);
        P->RemoveDelegate(Delegate, Source);
        if (!bRemove) P->AddDelegate(Delegate, Source);
    }
}
}



namespace { constexpr float TownSquareStep = 1.f/30.f;
const int32 BullFrames[] = {60,10,28,23,30,1,69,13,10,23};
const int32 ToreadorFrames[] = {90,10,21,11,6,10,8,9,14,9};
float TownSquareDistance(const FVector& V) { return FMath::Max(FMath::Abs(V.X), FMath::Abs(V.Y)) + .375f * FMath::Min(FMath::Abs(V.X), FMath::Abs(V.Y)); }
float TownSquareAngle(const FVector& V) { return FMath::RoundToFloat(V.Rotation().Yaw*256.f/360.f)*360.f/256.f; }
}

UTownSquareEnemyBehaviorComponent::UTownSquareEnemyBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}
UToreadorBehaviorComponent::UToreadorBehaviorComponent()
{
    // Legacy Town Square circuit. Only Toreadors own authored route settings.
    RoutePoints = {FVector(-2232,2723,0), FVector(-3226,7813,0), FVector(2918,3717,0)};
}
UBullBehaviorComponent::UBullBehaviorComponent()
{
    static ConstructorHelpers::FObjectFinder<USoundBase> Impact(TEXT("/Game/SpyroContent/Global_Assets/Global_Characters/Playable_Characters/Spyro/Audio/S1_HitWall.S1_HitWall"));
    GoreImpactSound=Impact.Object;
}
USplineComponent* UToreadorBehaviorComponent::GetRunPath() const
{
    if (GetOwner())
        for (auto* C:GetOwner()->GetComponents())
            if (C && C->GetFName()==TEXT("RunPath")) return Cast<USplineComponent>(C);
    return nullptr;
}
UTownSquareEnemyBehaviorComponent* UTownSquareEnemyBehaviorComponent::TerritoryOwner() const
{
    if (bBullPatrolInitialized) return const_cast<UTownSquareEnemyBehaviorComponent*>(this);
#if WITH_EDITOR
    if (IsBull() && !HasBegunPlay() && GetWorld())
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
            if (auto* Toreador=It->FindComponentByClass<UToreadorBehaviorComponent>())
                if (Toreador->LinkedBull==GetOwner()) return Toreador;
#endif
    return IsBull() && IsValid(Partner) ? Partner : const_cast<UTownSquareEnemyBehaviorComponent*>(this);
}
FVector UTownSquareEnemyBehaviorComponent::GetRoamCenter() const
{
    if (bBullPatrolInitialized) return BullPatrolOrigin.GetLocation();
    const auto* Territory = TerritoryOwner();
    return Territory->HasBegunPlay() ? Territory->RouteOrigin.GetLocation() : Territory->GetOwner()->GetActorLocation();
}
AActor* UTownSquareEnemyBehaviorComponent::GetPartner() const { return IsValid(Partner) ? Partner->GetOwner() : nullptr; }
FString UTownSquareEnemyBehaviorComponent::ValidatePlacement() const
{
    if (Animations.Num() != 10 || Animations.Contains(nullptr)) return TEXT("Assign all ten reference animations.");
    if (bPairConflict) return TEXT("Linked Bull is already assigned to another Toreador.");
    if (const auto* T = Cast<UToreadorBehaviorComponent>(this))
    {
        if (T->LinkedBull && !T->LinkedBull->FindComponentByClass<UBullBehaviorComponent>()) return TEXT("LinkedBull must contain BullBehavior.");
        if (T->bUseRunPath)
        {
            const auto* S=T->GetRunPath();
            if (!S || S->GetNumberOfSplinePoints()<2 || !S->IsClosedLoop() || S->GetSplineLength()<1.f)
                return TEXT("RunPath needs at least two points and Closed Loop enabled.");
            return FString();
        }
        if (T->RoutePoints.Num()<2) return TEXT("Author at least two Toreador route points or enable RunPath.");
    }
    if (bFollowingRunPath) return FString();
    if (HasBegunPlay() && bLimitRoaming && FVector::Dist2D(GetOwner()->GetActorLocation(), GetRoamCenter()) > RoamCenterLimit())
        return TEXT("Body starts outside its territory. Move it inside the preview.");
    return FString();
}
void UTownSquareEnemyBehaviorComponent::SetPairMovementIgnored(UTownSquareEnemyBehaviorComponent* Other, bool bIgnore)
{
    if (!IsValid(Other)) return;
    auto* A=Cast<ACharacter>(GetOwner()); auto* B=Cast<ACharacter>(Other->GetOwner());
    if (!A || !B) return;
    A->GetCapsuleComponent()->IgnoreActorWhenMoving(B,bIgnore);
    B->GetCapsuleComponent()->IgnoreActorWhenMoving(A,bIgnore);
}
void UTownSquareEnemyBehaviorComponent::ReconcilePair()
{
    if (Partner && (!IsValid(Partner) || !IsValid(Partner->GetOwner()))) Partner = nullptr;
    if (IsBull())
    {
        // Explicitly clearing/changing a living Toreador's link restores the Bull's patrol.
        if (auto* T = Cast<UToreadorBehaviorComponent>(Partner))
            if (T->LinkedBull != GetOwner())
            {
                SetPairMovementIgnored(T,false);
                if (T->Partner == this) T->Partner = nullptr;
                Partner = nullptr;
            }
        return;
    }
    auto* T = Cast<UToreadorBehaviorComponent>(this);
    auto* Bull = IsValid(T->LinkedBull) ? T->LinkedBull->FindComponentByClass<UBullBehaviorComponent>() : nullptr;
    if (Partner != Bull && IsValid(Partner) && Partner->Partner == this)
    { SetPairMovementIgnored(Partner,false); Partner->Partner = nullptr; }
    Partner = nullptr;
    bPairConflict = Bull && IsValid(Bull->Partner) && Bull->Partner != this;
    if (Bull && !bPairConflict) { Partner = Bull; Bull->Partner = this; SetPairMovementIgnored(Bull,true); }
}
void UTownSquareEnemyBehaviorComponent::SetStandaloneBullPatrol()
{
    const float HalfDistance=FMath::Max(200.f,CastChecked<UBullBehaviorComponent>(this)->PatrolDistance)*.5f;
    const float Extent=HalfDistance/WorldUnitsPerOriginalUnit;
    ActiveRoutePoints={FVector(Extent,0,0),FVector(-Extent,0,0)};
    ActiveRouteYaw=0; ActiveRoamRadius=HalfDistance+500.f;
    BullPatrolOrigin=RouteOrigin;
    CaptureRunPath(nullptr);
}
void UTownSquareEnemyBehaviorComponent::UpdateBullPatrol()
{
    auto* Bull = Cast<UBullBehaviorComponent>(this);
    if (!Bull) return;
    // Resolve explicit links before choosing the initial route, regardless of tick order.
    if (!bBullPatrolInitialized)
    {
        if (!IsValid(Partner))
            for (TActorIterator<AActor> It(GetWorld()); It; ++It)
                if (auto* Toreador=It->FindComponentByClass<UToreadorBehaviorComponent>())
                    if (Toreador->LinkedBull==GetOwner()) Toreador->ReconcilePair();
        SetStandaloneBullPatrol();
        Bull->ActiveMovementPattern=EBullMovementPattern::BackAndForth;
        bBullPatrolInitialized=true;
    }
    bool bChanged=false;
    if (IsValid(Partner) && Partner != BullPatrolSource)
    {
        BullPatrolSource=Partner; BullPatrolOrigin=Partner->RouteOrigin;
        ActiveRoutePoints=Partner->ActiveRoutePoints; ActiveRouteYaw=Partner->ActiveRouteYaw; ActiveRoamRadius=Partner->ActiveRoamRadius;
        CaptureRunPath(Partner->RuntimeRunPath);
        Bull->ActiveMovementPattern=EBullMovementPattern::Loop; bChanged=true;
    }
    else if (auto* Source=Cast<UToreadorBehaviorComponent>(BullPatrolSource))
    {
        // Death/destruction preserves the established circuit. An explicit unlink
        // of a still-existing actor is a different authoring action.
        if (IsValid(Source) && IsValid(Source->GetOwner()) && Source->LinkedBull != GetOwner())
        {
            BullPatrolSource=nullptr; SetStandaloneBullPatrol();
            Bull->ActiveMovementPattern=EBullMovementPattern::BackAndForth; bChanged=true;
        }
    }
    if (bChanged)
    {
        CurrentRouteNode=LastReachedRouteNode=BlockedTicks=0; RouteDirection=1;
        if (!bDefeated) { State=ETownSquareEnemyState::Pursuit; SlideDisplacement=0; SelectClip(1); }
    }
}
void UTownSquareEnemyBehaviorComponent::BeginPlay()
{
    Super::BeginPlay();
    Character = Cast<ACharacter>(GetOwner());
    if (!Character) { SetComponentTickEnabled(false); return; }
    Mesh = Character->GetMesh();
    TArray<UActorComponent*> Components; Character->GetComponents(Components);
    for (auto* C : Components)
    {
        if (C->GetClass()->GetName() == TEXT("Damageable_Com_C")) Damageable = C;
        if (C->GetClass()->GetName() == TEXT("Walking_AI_Character_C")) WalkingAI = C;
        if (C->GetClass()->GetName() == TEXT("Drops_Items_C")) Dropper = C;
        if (C->GetFName() == TEXT("TownSquareBodyCollision")) BodyCollision = Cast<UBoxComponent>(C);
        if (C->GetFName() == TEXT("TownSquareChargeSensor")) ChargeSensor = Cast<UBoxComponent>(C);
        if (C->GetFName() == TEXT("Attack_Radius")) if (auto* Attack=Cast<UPrimitiveComponent>(C))
        { Attack->SetGenerateOverlapEvents(false); Attack->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    }
    if (!Damageable || !WalkingAI || !Dropper || !Mesh || !BodyCollision || !ChargeSensor || Animations.Num() != 10 || Animations.Contains(nullptr))
    { UE_LOG(LogTemp, Error, TEXT("Town Square enemy %s has incomplete contracts/assets."), *Character->GetName()); SetComponentTickEnabled(false); return; }
    RouteOrigin = FTransform(FRotator(0,Character->GetActorRotation().Yaw,0),Character->GetActorLocation());
    if (auto* T=Cast<UToreadorBehaviorComponent>(this))
    {
        ActiveRoutePoints=T->RoutePoints; ActiveRouteYaw=T->RouteYaw; ActiveRoamRadius=T->RoamRadius;
        if (T->bUseRunPath)
        {
            CaptureRunPath(T->GetRunPath());
            if (!bFollowingRunPath) UE_LOG(LogTemp,Warning,TEXT("%s: RunPath needs at least two points and a closed, nonzero-length loop; using legacy route."),*GetOwner()->GetName());
        }
    }
    PreviousLocation = Character->GetActorLocation(); HeadingDegrees = PreviousHeading = Character->GetActorRotation().Yaw;
    MeshRelativeLocation = Mesh->GetRelativeLocation(); Random.Initialize(GetTypeHash(Character->GetFName()));
    Mesh->AddTickPrerequisiteComponent(this);
    BindContracts();
    ChargeSensor->OnComponentBeginOverlap.AddUniqueDynamic(this, &UTownSquareEnemyBehaviorComponent::OnChargeSensorOverlap);
    TownSquare::SetNumber(Damageable,TEXT("Hit Points"),1);
    TownSquare::SetBool(WalkingAI,TEXT("Poofs_On_Death"),false);
    TownSquare::SetNumber(WalkingAI,TEXT("Death Launch Upwards Force"),0);
    TownSquare::SetNumber(WalkingAI,TEXT("Death Launch Forwards Force"),0);
    TownSquare::SetNumber(Character,TEXT("Corpse Poof Delay"),100000.f);
    TakeMovementControl(); SelectClip(0,false);
}
void UTownSquareEnemyBehaviorComponent::TakeMovementControl()
{
    Character->SetActorTickEnabled(false); WalkingAI->SetComponentTickEnabled(false);
    if (auto* Controller = Character->GetController()) Controller->StopMovement();
    auto* Move = Character->GetCharacterMovement(); Move->DisableMovement(); Move->SetComponentTickEnabled(false); Move->bEnablePhysicsInteraction = false;
    Character->ConsumeMovementInputVector();
    UClass* AnimClass = IsBull() ? UBullAnimInstance::StaticClass() : UToreadorAnimInstance::StaticClass();
    if (Mesh->GetAnimClass() != AnimClass) Mesh->SetAnimInstanceClass(AnimClass);
}
void UTownSquareEnemyBehaviorComponent::BindContracts()
{
    TownSquare::Bind(Damageable,TEXT("Call Deal_Damage"),WalkingAI,TEXT("On Damaged"),true);
    TownSquare::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(UTownSquareEnemyBehaviorComponent,OnAcceptedDamage));
    TownSquare::Bind(Dropper,TEXT("Item Dropper Successfully Reset"),this,GET_FUNCTION_NAME_CHECKED(UTownSquareEnemyBehaviorComponent,OnDropperReset));
}
void UTownSquareEnemyBehaviorComponent::SelectClip(int32 Clip, bool Blend)
{
    Clip = FMath::Clamp(Clip,0,9);
    if (Blend && NextClip == Clip) return;
    if (Blend) { CurrentClip = NextClip; CurrentFrame = NextFrame; NextClip = Clip; NextFrame = 0; Progress = ProgressPerStep = 16; }
    else { CurrentClip = NextClip = Clip; CurrentFrame = 0; NextFrame = (IsBull() && Clip == 5) ? 0 : 1; Progress = 0; ProgressPerStep = IsBull() ? (Clip == 5 ? 0 : (Clip == 6 || Clip == 7 ? 64 : 32)) : 32; EmitFrameSound(); }
}
bool UTownSquareEnemyBehaviorComponent::AdvanceAnimation()
{
    Progress += ProgressPerStep;
    if (Progress < 64) return false;
    Progress &= 63;
    bool Complete = false;
    if (CurrentClip != NextClip)
    { CurrentClip = NextClip; CurrentFrame = NextFrame; NextFrame = (IsBull() && CurrentClip == 5) ? 0 : 1; Progress = 0; ProgressPerStep = IsBull() ? (CurrentClip == 5 ? 0 : (CurrentClip == 6 || CurrentClip == 7 ? 64 : 32)) : 32; }
    else
    { CurrentFrame = NextFrame; ++NextFrame; if (NextFrame >= (IsBull() ? BullFrames[CurrentClip] : ToreadorFrames[CurrentClip])) { NextFrame = 0; Complete = true; } }
    EmitFrameSound(); return Complete;
}
void UTownSquareEnemyBehaviorComponent::GetPoseInputs(UAnimSequence*& A,UAnimSequence*& B,float& TimeA,float& TimeB,float& Alpha) const
{
    A = Animations.IsValidIndex(CurrentClip) ? Animations[CurrentClip] : nullptr;
    B = Animations.IsValidIndex(NextClip) ? Animations[NextClip] : nullptr;
    const int32* Frames = IsBull() ? BullFrames : ToreadorFrames;
    TimeA = A ? A->GetPlayLength()*CurrentFrame/FMath::Max(1,Frames[CurrentClip]-1) : 0;
    TimeB = B ? B->GetPlayLength()*NextFrame/FMath::Max(1,Frames[NextClip]-1) : 0;
    Alpha = FMath::Clamp((Progress + Accumulator/TownSquareStep*ProgressPerStep)/64.f,0.f,1.f);
}
void UTownSquareEnemyBehaviorComponent::EmitFrameSound()
{
    int32 Slot = INDEX_NONE;
    if (IsBull())
    {
        if (CurrentClip == 1 || CurrentClip == 8) { if (CurrentFrame == 0) Slot=0; if (CurrentFrame == 4) Slot=1; }
        if (CurrentClip == 4) { if (CurrentFrame == 0) Slot=2; if (CurrentFrame == 9) Slot=3; }
        if (CurrentClip == 6 && CurrentFrame == 1) Slot=2;
        if (CurrentClip == 8 && CurrentFrame == 3) Slot=2;
        if (CurrentClip == 9) { if (CurrentFrame == 0) Slot=2; if (CurrentFrame == 6) Slot=3; }
    }
    else
    {
        if (CurrentClip == 1) { if (CurrentFrame == 1) Slot=0; if (CurrentFrame == 7) Slot=1; }
        if (CurrentClip == 3 && CurrentFrame == 1) Slot=2;
        if (CurrentClip == 5 && CurrentFrame == 2) Slot=3;
        if (CurrentClip == 8 && CurrentFrame == 4) Slot=4;
    }
    if (Slot == INDEX_NONE) return;
    if (SoundCueHistory.Num() == 256) SoundCueHistory.RemoveAt(0);
    SoundCueHistory.Add(Slot);
    PlayingSounds.RemoveAll([](UAudioComponent* C){ return !IsValid(C) || !C->IsPlaying(); });
    if (OriginalSounds.IsValidIndex(Slot) && OriginalSounds[Slot])
        if (auto* Audio = UGameplayStatics::SpawnSoundAttached(OriginalSounds[Slot],Character->GetRootComponent(),NAME_None,FVector::ZeroVector,EAttachLocation::KeepRelativeOffset,true,1,1,0,SoundAttenuation)) PlayingSounds.Add(Audio);
}
void UTownSquareEnemyBehaviorComponent::StopSounds() { for (auto* A:PlayingSounds) if (IsValid(A)) A->Stop(); PlayingSounds.Reset(); }
float UTownSquareEnemyBehaviorComponent::OriginalDistanceTo(const FVector& P) const { return TownSquareDistance(P-Character->GetActorLocation())/WorldUnitsPerOriginalUnit; }
FVector UTownSquareEnemyBehaviorComponent::FloorPosition(AActor* Actor) const
{
    FVector P=Actor->GetActorLocation(); if (auto* C=Cast<ACharacter>(Actor)) P.Z-=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); return P;
}
bool UTownSquareEnemyBehaviorComponent::Face(const FVector& Point,float TurnUnits,float WithinUnits)
{
    const float Error=FMath::FindDeltaAngleDegrees(HeadingDegrees,TownSquareAngle(Point-Character->GetActorLocation()));
    HeadingDegrees=FMath::UnwindDegrees(HeadingDegrees+FMath::Clamp(Error,-TurnUnits*360.f/256.f,TurnUnits*360.f/256.f));
    return FMath::Abs(Error)<WithinUnits*360.f/256.f;
}
void UTownSquareEnemyBehaviorComponent::CaptureRunPath(const USplineComponent* Source)
{
    RuntimeRunPath=nullptr; bFollowingRunPath=false; RunPathLength=RunPathDistance=0; bLimitRoaming=true;
    if (!Source || Source->GetNumberOfSplinePoints()<2 || !Source->IsClosedLoop() || Source->GetSplineLength()<1.f) return;
    RuntimeRunPath=NewObject<USplineComponent>(GetOwner(),NAME_None,RF_Transient);
    RuntimeRunPath->SplineCurves=Source->SplineCurves;
    RuntimeRunPath->SetWorldTransform(Source->GetComponentTransform());
    RuntimeRunPath->SetClosedLoop(true); RuntimeRunPath->UpdateSpline();
    RunPathLength=RuntimeRunPath->GetSplineLength(); bFollowingRunPath=true;
    // The authored spline bounds the route. Do not shrink or clip it to the legacy circle.
    bLimitRoaming=false; ResetRunPathProgress();
}
void UTownSquareEnemyBehaviorComponent::ResetRunPathProgress()
{
    bJoiningRunPath=true;
    if (RuntimeRunPath)
        RunPathDistance=RuntimeRunPath->GetDistanceAlongSplineAtSplineInputKey(RuntimeRunPath->FindInputKeyClosestToWorldLocation(GetOwner()->GetActorLocation()));
}
void UTownSquareEnemyBehaviorComponent::FollowRunPath(float Speed)
{
    if (!RuntimeRunPath || RunPathLength<1.f) return;
    const float Step=Speed*WorldUnitsPerOriginalUnit;
    auto Wrap=[this](float Distance) { const float D=FMath::Fmod(Distance,RunPathLength); return D<0?D+RunPathLength:D; };
    const FVector Start=Character->GetActorLocation();
    const FVector Anchor=RuntimeRunPath->GetLocationAtDistanceAlongSpline(RunPathDistance,ESplineCoordinateSpace::World);
    if (FVector::Dist2D(Start,Anchor)<3.f) bJoiningRunPath=false;
    const float NextDistance=bJoiningRunPath?RunPathDistance:Wrap(RunPathDistance+Step*RouteDirection);
    const FVector Target=RuntimeRunPath->GetLocationAtDistanceAlongSpline(NextDistance,ESplineCoordinateSpace::World);
    const FVector Direction=(Target-Start).GetSafeNormal2D();
    if (Face(Target,30,1.f)) GroundMove(FMath::Min(Step,FVector::Dist2D(Start,Target))/WorldUnitsPerOriginalUnit,HeadingDegrees);
    if (!bJoiningRunPath)
    {
        if (FVector::Dist2D(Character->GetActorLocation(),Target)<3.f) RunPathDistance=NextDistance;
        else RunPathDistance=Wrap(RunPathDistance+FMath::Clamp(FVector::DotProduct(LastStepDelta,Direction),0.f,Step)*RouteDirection);
    }
    CurrentRouteNode=(FMath::FloorToInt(RuntimeRunPath->SplineCurves.ReparamTable.Eval(RunPathDistance,0.f))+1)%RuntimeRunPath->GetNumberOfSplinePoints();
    if (!bPlayerBlocked && RequestedStepDelta.Size2D()>.1f && LastStepDelta.Size2D()<.5f) ++BlockedTicks; else BlockedTicks=0;
    if (BlockedTicks>=12) { RouteDirection=-RouteDirection; BlockedTicks=0; ++RecoveryCount; }
}
void UTownSquareEnemyBehaviorComponent::FollowRoute(float Speed, bool bBackAndForth)
{
    if (bFollowingRunPath) { FollowRunPath(Speed); return; }
    auto* Territory=TerritoryOwner(); const int32 N=Territory->ActiveRoutePoints.Num();
    if (N<2) return;
    CurrentRouteNode=FMath::Clamp(CurrentRouteNode,0,N-1);
    if (OriginalDistanceTo(RoutePosition(CurrentRouteNode))<FMath::Max(30.f,128.f*RouteFitScale))
    {
        LastReachedRouteNode=CurrentRouteNode;
        const bool bEndpoint = bBackAndForth && (CurrentRouteNode == 0 || CurrentRouteNode == N-1);
        if (bBackAndForth)
        {
            if (CurrentRouteNode == 0) RouteDirection=1;
            else if (CurrentRouteNode == N-1) RouteDirection=-1;
        }
        CurrentRouteNode=(CurrentRouteNode+RouteDirection+N)%N; BlockedTicks=0;
        if (bEndpoint)
        {
            State=ETownSquareEnemyState::Turn; SlideDisplacement=Speed; SelectClip(2);
            return;
        }
    }
    // A straight patrol must finish facing its leg before translating. Moving
    // through the initial half-turn bends it away from its authored line.
    if (Face(RoutePosition(CurrentRouteNode),30,bBackAndForth ? 1.f : 128.f)) GroundMove(Speed,HeadingDegrees);
    if (!bPlayerBlocked && RequestedStepDelta.Size2D()>0.1f && LastStepDelta.Size2D()<0.5f) ++BlockedTicks; else BlockedTicks=0;
    if (BlockedTicks>=12) { RouteDirection=-RouteDirection; CurrentRouteNode=LastReachedRouteNode; BlockedTicks=0; ++RecoveryCount; }
}
bool UTownSquareEnemyBehaviorComponent::CanBullGore(float Cone) const
{
    if (!IsValid(Pursuer) || OriginalDistanceTo(Pursuer->GetActorLocation())>=1536 ||
        FMath::Abs(FloorPosition(Pursuer).Z-FloorPosition(Character).Z)>800*WorldUnitsPerOriginalUnit ||
        FMath::Abs(FMath::FindDeltaAngleDegrees(HeadingDegrees,TownSquareAngle(Pursuer->GetActorLocation()-Character->GetActorLocation())))>=Cone) return false;
    if (auto* P=CastField<FByteProperty>(TownSquare::Property(Pursuer,TEXT("Player_State"))))
        if (P->Enum && P->GetPropertyValue_InContainer(Pursuer)==P->Enum->GetValueByNameString(TEXT("NewEnumerator4"))) return false;
    FHitResult Wall; FCollisionQueryParams Query(SCENE_QUERY_STAT(TownSquareGore),false,Character);
    Query.AddIgnoredActor(Pursuer);
    if (IsValid(Partner)) Query.AddIgnoredActor(Partner->GetOwner());
    return !GetWorld()->LineTraceSingleByChannel(Wall,Character->GetActorLocation(),Pursuer->GetActorLocation(),ECC_Visibility,Query);
}
void UTownSquareEnemyBehaviorComponent::StepBull(bool Complete)
{
    if (State==ETownSquareEnemyState::Inverting)
    {
        if (CurrentFrame<5) HeadingDegrees+=360.f/128.f;
        if (SlideDisplacement>=16) { SlideDisplacement-=15; GroundMove(SlideDisplacement,PreviousHeading); }
        if (Complete) { State=ETownSquareEnemyState::Stuck; SelectClip(5,false); StateTicks=0; Cooldown=Random.RandRange(64,127); }
        return;
    }
    if (State==ETownSquareEnemyState::Stuck)
    {
        if (CurrentClip==5 && Cooldown==0)
        {
            if (IsValid(Pursuer) && OriginalDistanceTo(Pursuer->GetActorLocation())>10240) { FinishCorpse(); return; }
            SelectClip(6,false); CurrentFrame=49; NextFrame=50;
        }
        else if (CurrentClip==6 && CurrentFrame>=44 && CurrentFrame<50)
        { SelectClip(5,false); Cooldown=Random.RandRange(64,127); }
        return;
    }
    if (State==ETownSquareEnemyState::Dying) { if (Complete) FinishCorpse(); return; }
    const auto Pattern = CastChecked<UBullBehaviorComponent>(this)->ActiveMovementPattern;
    if (State==ETownSquareEnemyState::Attack)
    {
        // Track during the wind-up, then commit the horns so a dodge can miss.
        if (IsValid(Pursuer) && (CurrentClip!=8 || CurrentFrame<3)) Face(Pursuer->GetActorLocation(),6);
        if (CurrentClip==8 && CurrentFrame>=3 && CurrentFrame<=5) HitPlayer(1536,60);
        if (Complete && CurrentClip==8) { State=ETownSquareEnemyState::Pursuit; SelectClip(1); Cooldown=30; }
        return;
    }
    auto TryGore=[this]()
    {
        // Detection and contact use the same reach. Do not swing at unreachable Spyro.
        if (!CanBullGore(60)) return false;
        SlideDisplacement=0; BlockedTicks=0;
        if (Cooldown==0)
        { State=ETownSquareEnemyState::Attack; SelectClip(8); bHitThisAttack=false; StateTicks=0; }
        else
        { State=ETownSquareEnemyState::Idle; SelectClip(0); Face(Pursuer->GetActorLocation(),6); }
        return true;
    };
    if (TryGore()) return;
    if (State==ETownSquareEnemyState::Turn)
    {
        if (SlideDisplacement>0) { GroundMove(SlideDisplacement,HeadingDegrees); SlideDisplacement=FMath::Max(0.f,SlideDisplacement-8.f); }
        if (Complete && CurrentClip==2)
        {
            HeadingDegrees=TownSquareAngle(RoutePosition(CurrentRouteNode)-Character->GetActorLocation());
            State=ETownSquareEnemyState::Pursuit; SelectClip(1);
        }
        return;
    }
    State=ETownSquareEnemyState::Pursuit; SelectClip(1); FollowRoute(100,Pattern==EBullMovementPattern::BackAndForth);
    TryGore();
}
void UTownSquareEnemyBehaviorComponent::StepToreador(bool Complete)
{
    if (State==ETownSquareEnemyState::Dying)
    {
        DeathLift+=DeathVerticalSpeed*WorldUnitsPerOriginalUnit; DeathVerticalSpeed-=15;
        if (Complete && CurrentClip==3) SelectClip(4);
        if (DeathVerticalSpeed < -180) FinishCorpse();
        return;
    }
    const bool Paired=IsValid(Partner) && !Partner->bDefeated;
    const float BullDistance=Paired ? OriginalDistanceTo(Partner->GetOwner()->GetActorLocation()) : BIG_NUMBER;
    if (Paired && BullDistance<4096 && (State==ETownSquareEnemyState::Idle || State==ETownSquareEnemyState::Attack))
    { State=ETownSquareEnemyState::React; SelectClip(5); StateTicks=0; }
    if (State==ETownSquareEnemyState::React)
    {
        if (!Paired) { State=ETownSquareEnemyState::Idle; SelectClip(0); return; }
        HeadingDegrees=FMath::FixedTurn(HeadingDegrees,Partner->HeadingDegrees,30*360.f/256.f);
        if (Complete) { State=ETownSquareEnemyState::Pursuit; SelectClip(1); StateTicks=0; }
        return;
    }
    if (State==ETownSquareEnemyState::Pursuit)
    {
        // Hysteresis lets it stop once safely ahead instead of running forever
        // on a route which does not pass through its original idle position.
        if (!Paired || BullDistance>6144 || (StateTicks>30 && OriginalDistanceTo(RouteOrigin.GetLocation())<221)) { State=ETownSquareEnemyState::Idle; SelectClip(0); return; }
        FollowRoute(160); return;
    }
    if (State==ETownSquareEnemyState::Attack)
    {
        if (IsValid(Pursuer)) Face(Pursuer->GetActorLocation(),4);
        // Original model 395: only Anim8 frames 5,6,7 select collision group 1.
        if (CurrentClip==8 && CurrentFrame>=5 && CurrentFrame<=7) HitPlayer(2048,60);
        if (Complete) { State=ETownSquareEnemyState::Idle; SelectClip(0); Cooldown=150; }
        return;
    }
    if (IsValid(Pursuer) && OriginalDistanceTo(Pursuer->GetActorLocation())<8192) Face(Pursuer->GetActorLocation(),10);
    if (!Paired && Cooldown==0 && IsValid(Pursuer) && OriginalDistanceTo(Pursuer->GetActorLocation())<2048 &&
        FMath::Abs(FloorPosition(Pursuer).Z-FloorPosition(Character).Z)<800*WorldUnitsPerOriginalUnit)
    { State=ETownSquareEnemyState::Attack; SelectClip(8); bHitThisAttack=false; }
}
void UTownSquareEnemyBehaviorComponent::StepOriginal()
{
    ++SimulationTicks; ++StateTicks; if (Cooldown>0) --Cooldown;
    PreviousLocation=Character->GetActorLocation(); PreviousHeading=HeadingDegrees;
    LastStepDelta=RequestedStepDelta=FVector::ZeroVector;
    bPlayerBlocked=bStartedOverlappingPlayer=bBoundaryClipped=bTerrainBlocked=bFloorRejected=false;
    RouteFitScale=CalculateRouteFit();
    if (State==ETownSquareEnemyState::Dead) return;
    const bool Complete=AdvanceAnimation();
    if (IsBull()) StepBull(Complete); else StepToreador(Complete);
    Character->GetCharacterMovement()->Velocity=LastStepDelta/TownSquareStep;
}
void UTownSquareEnemyBehaviorComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    if (!Character || !WalkingAI || !BodyCollision || !GetOwner()->HasAuthority()) return;
    if (bFirstTick) { BindContracts(); bFirstTick=false; }
    ReconcilePair(); UpdateBullPatrol(); TakeMovementControl();
    if (!IsValid(Pursuer)) Pursuer=UGameplayStatics::GetPlayerPawn(this,0);
    if (TownSquare::Bool(Damageable,TEXT("Frozen")) || TownSquare::Bool(Damageable,TEXT("Paralyzed_by_Fear")) || TownSquare::Bool(Dropper,TEXT("Reset_in_Progress"))) return;
    Accumulator+=FMath::Max(0.f,DeltaTime); int32 Steps=0;
    while (Accumulator+KINDA_SMALL_NUMBER>=TownSquareStep && Steps++<30) { Accumulator=FMath::Max(0.f,Accumulator-TownSquareStep); StepOriginal(); }
    Accumulator=FMath::Min(Accumulator,TownSquareStep);
    const float Alpha=Accumulator/TownSquareStep;
    Mesh->SetWorldLocation(FMath::Lerp(PreviousLocation,Character->GetActorLocation(),Alpha)+Character->GetActorTransform().TransformVector(MeshRelativeLocation)+FVector(0,0,DeathLift));
    Mesh->SetWorldRotation(FRotator(0,PreviousHeading+FMath::FindDeltaAngleDegrees(PreviousHeading,HeadingDegrees)*Alpha+MeshForwardYaw,0));
    DrawMovementDebug();
}
void UTownSquareEnemyBehaviorComponent::OnChargeSensorOverlap(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool bSweep,const FHitResult& Hit)
{
    if (!bDefeated && Other && Other!=Character && OtherComponent && OtherComponent->GetCollisionObjectType()==ECC_GameTraceChannel4)
    { auto* Capsule=Character->GetCapsuleComponent(); Capsule->OnComponentBeginOverlap.Broadcast(Capsule,Other,OtherComponent,BodyIndex,bSweep,Hit); }
}
void UTownSquareEnemyBehaviorComponent::OnAcceptedDamage()
{
    if (!GetOwner()->HasAuthority() || State==ETownSquareEnemyState::Dead || State==ETownSquareEnemyState::Dying ||
        TownSquare::Bool(Damageable,TEXT("Invincible")) || TownSquare::Bool(Damageable,TEXT("Frozen")) || TownSquare::Bool(Dropper,TEXT("Reset_in_Progress"))) return;
    // Audited Damage_Types values: Burn=2, Ram=5. Display labels are not runtime identifiers.
    const int32 Type=TownSquare::Number(Damageable,TEXT("Deal Damage - Damage Type"));
    const bool Flame=Type==2;
    const bool Charge=Type==5;
    if (!Flame && !Charge) return;
    if (bDefeated) { if (IsBull() && Flame && (State==ETownSquareEnemyState::Stuck || State==ETownSquareEnemyState::Inverting)) { State=ETownSquareEnemyState::Dying; SelectClip(9); } return; }
    Defeat(Flame);
}
void UTownSquareEnemyBehaviorComponent::Defeat(bool Flame)
{
    bDefeated=true; StateTicks=0; bHitThisAttack=true; BlockedTicks=0;
    TownSquare::SetNumber(Damageable,TEXT("Hit Points"),0);
    DropGemRange(0,1);
    // Publish the existing successful-damage/death event once, with custom presentation and no second payout.
    const bool Suppressed=TownSquare::Bool(Dropper,TEXT("Cannot_Drop_Items"));
    TownSquare::SetBool(Dropper,TEXT("Cannot_Drop_Items"),true);
    if (auto* P=CastField<FMulticastDelegateProperty>(TownSquare::Property(Damageable,TEXT("Damage Was Successfully Dealt"))))
        if (auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(Damageable))) D->ProcessMulticastDelegate<UObject>(nullptr);
    TownSquare::SetBool(Dropper,TEXT("Cannot_Drop_Items"),Suppressed);
    TakeMovementControl();
    if (IsBull()) { State=Flame ? ETownSquareEnemyState::Dying : ETownSquareEnemyState::Inverting; SelectClip(Flame?9:4, !Flame); SlideDisplacement=Flame?0:250; }
    else { State=ETownSquareEnemyState::Dying; SelectClip(3); DeathVerticalSpeed=200; }
    // A defeated Bull may remain visible and flameable, but it cannot attack or trigger charges.
    ChargeSensor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void UTownSquareEnemyBehaviorComponent::FinishCorpse()
{
    if (bCorpseFinished) return;
    bCorpseFinished=true; State=ETownSquareEnemyState::Dead; StopSounds();
    Character->SetActorHiddenInGame(true); Character->SetActorEnableCollision(false);
    if (auto* P=CastField<FMulticastDelegateProperty>(TownSquare::Property(WalkingAI,TEXT("Corpse Poofed"))))
        if (auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(WalkingAI))) D->ProcessMulticastDelegate<UObject>(nullptr);
}
void UTownSquareEnemyBehaviorComponent::OnDropperReset()
{
    StopSounds(); SoundCueHistory.Reset(); ReleasedGemIndices.Reset(); GemsSpawned=0;
    bDefeated=bHitThisAttack=bCorpseFinished=false; State=ETownSquareEnemyState::Idle;
    Accumulator=SlideDisplacement=DeathLift=DeathVerticalSpeed=0; CurrentRouteNode=LastReachedRouteNode=StateTicks=Cooldown=BlockedTicks=RecoveryCount=0; RouteDirection=1;
    Character->SetActorLocationAndRotation(RouteOrigin.GetLocation(),RouteOrigin.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);
    ResetRunPathProgress();
    if (auto* Bull=Cast<UBullBehaviorComponent>(this)) Bull->GoreImpactCount=0;
    Character->SetActorHiddenInGame(false); Character->SetActorEnableCollision(true);
    HeadingDegrees=PreviousHeading=RouteOrigin.Rotator().Yaw; PreviousLocation=Character->GetActorLocation();
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); ChargeSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TownSquare::SetNumber(Damageable,TEXT("Hit Points"),1); TakeMovementControl(); SelectClip(0,false); ReconcilePair();
}
void UTownSquareEnemyBehaviorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    SetPairMovementIgnored(Partner,false);
    if (IsValid(Partner) && Partner->Partner==this) Partner->Partner=nullptr;
    StopSounds();
    if (ChargeSensor) ChargeSensor->OnComponentBeginOverlap.RemoveDynamic(this,&UTownSquareEnemyBehaviorComponent::OnChargeSensorOverlap);
    TownSquare::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(UTownSquareEnemyBehaviorComponent,OnAcceptedDamage),true);
    TownSquare::Bind(Dropper,TEXT("Item Dropper Successfully Reset"),this,GET_FUNCTION_NAME_CHECKED(UTownSquareEnemyBehaviorComponent,OnDropperReset),true);
    Super::EndPlay(Reason);
}
float UTownSquareEnemyBehaviorComponent::CalculateRouteFit() const
{
    if (bFollowingRunPath) return 1.f;
    const auto* T=TerritoryOwner(); float Extent=1;
    for (const FVector& P:T->ActiveRoutePoints) Extent=FMath::Max(Extent,P.Size2D()*WorldUnitsPerOriginalUnit);
    return FMath::Min(1.f,RoamCenterLimit()*.8f/Extent);
}
FVector UTownSquareEnemyBehaviorComponent::RoutePosition(int32 Index) const
{
    if (RuntimeRunPath) return RuntimeRunPath->GetLocationAtSplinePoint(Index,ESplineCoordinateSpace::World);
    const auto* T=TerritoryOwner();
    if (!T->ActiveRoutePoints.IsValidIndex(Index)) return GetRoamCenter();
    FVector P=T->ActiveRoutePoints[Index]*WorldUnitsPerOriginalUnit; P.X*=RouteFitScale; P.Y*=RouteFitScale;
    const FTransform& Origin = bBullPatrolInitialized ? BullPatrolOrigin : T->RouteOrigin;
    return Origin.TransformPosition(FRotator(0,T->ActiveRouteYaw,0).RotateVector(P));
}
void UTownSquareEnemyBehaviorComponent::DrawMovementDebug() const
{
#if ENABLE_DRAW_DEBUG
    if (!bDrawMovementDebug) return;
    if (RuntimeRunPath)
    {
        for (int32 I=0;I<128;++I)
            DrawDebugLine(GetWorld(),RuntimeRunPath->GetLocationAtDistanceAlongSpline(RunPathLength*I/128,ESplineCoordinateSpace::World),RuntimeRunPath->GetLocationAtDistanceAlongSpline(RunPathLength*(I+1)/128,ESplineCoordinateSpace::World),FColor::Yellow);
        return;
    }
    DrawDebugCircle(GetWorld(),GetRoamCenter(),TerritoryOwner()->ActiveRoamRadius,64,FColor::Cyan,false,0,0,2,FVector::ForwardVector,FVector::RightVector,false);
    const auto* Bull=Cast<UBullBehaviorComponent>(this);
    const int32 Num=TerritoryOwner()->ActiveRoutePoints.Num();
    const bool bOpen= Bull && Bull->ActiveMovementPattern==EBullMovementPattern::BackAndForth;
    for (int32 I=0;I<Num-(bOpen?1:0);++I)
        DrawDebugLine(GetWorld(),RoutePosition(I),RoutePosition((I+1)%TerritoryOwner()->ActiveRoutePoints.Num()),FColor::Yellow);
    DrawDebugString(GetWorld(),Character->GetActorLocation()+FVector(0,0,160),ValidatePlacement(),nullptr,FColor::Red,0,true);
#endif
}
void UTownSquareEnemyBehaviorComponent::HitPlayer(float Range,float Cone)
{
    if (bDefeated || bHitThisAttack || !IsValid(Pursuer) || OriginalDistanceTo(Pursuer->GetActorLocation())>=Range ||
        FMath::Abs(FloorPosition(Pursuer).Z-FloorPosition(Character).Z)>800*WorldUnitsPerOriginalUnit ||
        FMath::Abs(FMath::FindDeltaAngleDegrees(HeadingDegrees,TownSquareAngle(Pursuer->GetActorLocation()-Character->GetActorLocation())))>=Cone) return;
    // Charge wins this contact. Otherwise the broad horn/cape test can put Spyro
    // into hurt state before the narrower inherited ram sensor receives him.
    if (auto* PlayerState=CastField<FByteProperty>(TownSquare::Property(Pursuer,TEXT("Player_State"))))
        if (PlayerState->Enum && PlayerState->GetPropertyValue_InContainer(Pursuer)==PlayerState->Enum->GetValueByNameString(TEXT("NewEnumerator4"))) return;
    FHitResult Wall; FCollisionQueryParams Query(SCENE_QUERY_STAT(TownSquareAttack),false,Character); Query.AddIgnoredActor(Pursuer);
    if (IsValid(Partner)) Query.AddIgnoredActor(Partner->GetOwner());
    if (GetWorld()->LineTraceSingleByChannel(Wall,Character->GetActorLocation(),Pursuer->GetActorLocation(),ECC_Visibility,Query)) return;
    UFunction* F=Pursuer->FindFunction(TEXT("Deal Damage to Player"));
    if (!F) return;
    FStructOnScope Params(F);
    const auto* Bull=Cast<UBullBehaviorComponent>(this);
    UObject* PlayerDamageable=TownSquare::ObjectValue(Pursuer,TEXT("Damageable"));
    const int32 HealthBefore=TownSquare::Number(PlayerDamageable,TEXT("Hit Points"));
    FVector HitDirection=FRotator(0,HeadingDegrees,0).Vector();
    if (Bull)
    {
        const FVector Away=(Pursuer->GetActorLocation()-Character->GetActorLocation()).GetSafeNormal2D();
        if (!Away.IsNearlyZero()) HitDirection=Away;
    }
    for (TFieldIterator<FProperty> It(F);It && It->HasAnyPropertyFlags(CPF_Parm);++It)
    {
        void* V=It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
        // Existing player Ram reaction supplies the aerial recoil, landing and hurt sound.
        if (auto* P=CastField<FByteProperty>(*It)) P->SetPropertyValue(V,Bull?5:1);
        if (auto* P=CastField<FStructProperty>(*It)) if (P->Struct==TBaseStructure<FVector>::Get()) *static_cast<FVector*>(V)=HitDirection;
        if (auto* P=CastField<FObjectPropertyBase>(*It)) P->SetObjectPropertyValue(V,Character);
    }
    bHitThisAttack=true; Pursuer->ProcessEvent(F,Params.GetStructMemory());
    if (Bull && TownSquare::Number(PlayerDamageable,TEXT("Hit Points"))<HealthBefore)
    {
        ++CastChecked<UBullBehaviorComponent>(this)->GoreImpactCount;
        if (Bull->GoreImpactSound)
            if (auto* Audio=UGameplayStatics::SpawnSoundAtLocation(this,Bull->GoreImpactSound,Pursuer->GetActorLocation(),FRotator::ZeroRotator,Bull->GoreImpactVolume,Bull->GoreImpactPitch,0,SoundAttenuation)) PlayingSounds.Add(Audio);
    }
}

bool UTownSquareEnemyBehaviorComponent::SweepPlayerBody(const FVector& Delta, FHitResult& Contact, bool& bInitialOverlap) const
{
    Contact = FHitResult(1.f);
    bInitialOverlap = false;
    if (!BodyCollision || Delta.IsNearlyZero()) return false;
    const FVector Center = BodyCollision->GetComponentLocation();
    const FQuat Rotation = BodyCollision->GetComponentQuat();
    const FCollisionShape Shape = FCollisionShape::MakeBox(BodyCollision->GetScaledBoxExtent());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TownSquareMovement), false, Character);
    // A paired enemy is not a player body. Both may traverse the same waypoint.
    if (IsValid(Partner)) Query.AddIgnoredActor(Partner->GetOwner());
    FCollisionObjectQueryParams Players;
    Players.AddObjectTypesToQuery(ECC_Pawn);
    Players.AddObjectTypesToQuery(ECC_GameTraceChannel4);
    TArray<FHitResult> Contacts;
    GetWorld()->SweepMultiByObjectType(Contacts, Center, Center + Delta, Rotation, Players, Shape, Query);
    bool bBlocked = false;
    for (const FHitResult& Hit : Contacts)
    {
        UPrimitiveComponent* Other = Hit.GetComponent();
        // Object queries include overlap-only collection/damage sensors. Only the
        // two components' real blocking responses should stop the solid body.
        if (!Other || BodyCollision->GetCollisionResponseToChannel(Other->GetCollisionObjectType()) != ECR_Block ||
            Other->GetCollisionResponseToChannel(BodyCollision->GetCollisionObjectType()) != ECR_Block) continue;
        if (Hit.bStartPenetrating || Hit.Time <= KINDA_SMALL_NUMBER)
        {
            bInitialOverlap |= Hit.bStartPenetrating;
            FVector Outward = Hit.Normal.GetSafeNormal2D();
            if (Outward.IsNearlyZero()) Outward = (Center - Other->GetComponentLocation()).GetSafeNormal2D();
            if (Outward.IsNearlyZero()) Outward = Delta.GetSafeNormal2D();
            FMTDResult StartMTD, EndMTD;
            const bool bStart = Other->ComputePenetration(StartMTD, Shape, Center, Rotation);
            const bool bEnd = Other->ComputePenetration(EndMTD, Shape, Center + Delta, Rotation);
            // Do not zero every time-zero hit: a swept horizontal step away from
            // an existing overlap is safe if it cannot deepen that penetration.
            if (FVector::DotProduct(Delta, Outward) > KINDA_SMALL_NUMBER &&
                (!bEnd || (bStart && EndMTD.Distance <= StartMTD.Distance + KINDA_SMALL_NUMBER))) continue;
        }
        if (!bBlocked || Hit.Time < Contact.Time) { Contact = Hit; bBlocked = true; }
    }
    return bBlocked;
}
bool UTownSquareEnemyBehaviorComponent::ProjectGroundMove(const FVector& HorizontalDelta, FVector& GroundDelta)
{
    const FVector Before = Character->GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TownSquareFloor), false, Character);
    // Its partner's body/sensor must not mask the terrain support rays either.
    if (IsValid(Partner)) Query.AddIgnoredActor(Partner->GetOwner());
    // A player must never become this enemy's floor, even if its object channel changes.
    if (IsValid(Pursuer)) Query.AddIgnoredActor(Pursuer);
    // A roll drops a gem directly above the actor before moving. That gem must
    // never become temporary terrain when player contact shortens the first step.
    if (auto* Items = CastField<FArrayProperty>(TownSquare::Property(Dropper, TEXT("Items_I_Dropped"))))
    {
        if (auto* Item = CastField<FObjectPropertyBase>(Items->Inner))
        {
            FScriptArrayHelper Array(Items, Items->ContainerPtrToValuePtr<void>(Dropper));
            for (int32 I = 0; I < Array.Num(); ++I)
                if (AActor* DroppedActor = Cast<AActor>(Item->GetObjectPropertyValue(Array.GetRawPtr(I))))
                    Query.AddIgnoredActor(DroppedActor);
        }
    }
    FCollisionObjectQueryParams GroundTypes;
    GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic); GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FVector Probe = Before + HorizontalDelta - FVector(0,0,HalfHeight);
    TArray<FHitResult> Floors;
    GetWorld()->LineTraceMultiByObjectType(Floors, Probe + FVector(0,0,1024.f * WorldUnitsPerOriginalUnit),
        Probe - FVector(0,0,5000.f * WorldUnitsPerOriginalUnit), GroundTypes, Query);
    const FHitResult* Floor = Floors.FindByPredicate([Capsule](const FHitResult& Hit)
    {
        const UPrimitiveComponent* Surface = Hit.GetComponent();
        return Surface && !Cast<APawn>(Hit.GetActor()) && Hit.ImpactNormal.Z >= 0.5f &&
            Capsule->GetCollisionResponseToChannel(Surface->GetCollisionObjectType()) == ECR_Block &&
            Surface->GetCollisionResponseToChannel(Capsule->GetCollisionObjectType()) == ECR_Block;
    });
    if (!Floor) return false;
    // Require support beneath the leading body edges as well as the root. A
    // center-only ray lets half the enemy hang over a ledge before it stops.
    const float SupportRadius=Capsule->GetScaledCapsuleRadius();
    for (const FVector& Offset : {FVector(SupportRadius,0,0),FVector(-SupportRadius,0,0),FVector(0,SupportRadius,0),FVector(0,-SupportRadius,0)})
    {
        FHitResult Support;
        const FVector Point=Floor->ImpactPoint+Offset;
        if (!GetWorld()->LineTraceSingleByObjectType(Support,Point+FVector(0,0,600*WorldUnitsPerOriginalUnit),Point-FVector(0,0,600*WorldUnitsPerOriginalUnit),GroundTypes,Query) ||
            Cast<APawn>(Support.GetActor()) || Support.ImpactNormal.Z<.5f) return false;
    }
    LastFloorActor = Floor->GetActor() ? Floor->GetActor()->GetFName() : NAME_None;
    LastFloorComponent = Floor->GetComponent()->GetFName();
    LastFloorHeight = Floor->ImpactPoint.Z;
    // The bottom hemisphere touches a sloped plane away from the centerline.
    // For an upright capsule, plane support is H-R+R/NormalZ, not simply H.
    // Without this clearance even shallow seams leave the swept capsule embedded.
    const float SlopeClearance = Capsule->GetScaledCapsuleRadius() *
        (1.f / FMath::Max(0.5f, Floor->ImpactNormal.Z) - 1.f);
    const float Difference = Floor->ImpactPoint.Z + HalfHeight + SlopeClearance + 2.f - Before.Z;
    GroundDelta = HorizontalDelta;
    if (FMath::Abs(Difference) > 600.f * WorldUnitsPerOriginalUnit) return false;
    GroundDelta.Z = Difference;
    return true;
}
void UTownSquareEnemyBehaviorComponent::GroundMove(float OriginalDistance, float Direction)
{
    const FVector Before = Character->GetActorLocation();
    RequestedStepDelta = FRotator(0, Direction, 0).Vector() * OriginalDistance * WorldUnitsPerOriginalUnit;
    FVector Pending = RequestedStepDelta;
    float RemainingDistance = Pending.Size2D();
    auto* Movement = Character->GetCharacterMovement();
    // Bound collision work and total travel by this original 30Hz step. Every slide
    // is separately checked against players, floor, world geometry and territory.
    for (int32 Iteration = 0; Iteration < 3; ++Iteration)
    {
        const FVector SegmentStart = Character->GetActorLocation();
        const FVector Contained = ConstrainRoamMove(SegmentStart, Pending);
        bBoundaryClipped |= !Contained.Equals(Pending, 0.01f);
        Pending = Contained;
        FVector ProjectedPending;
        if (!ProjectGroundMove(Pending, ProjectedPending)) { bFloorRejected = true; break; }
        FHitResult PlayerContact;
        bool bInitialOverlap = false;
        const bool bHitPlayer = SweepPlayerBody(ProjectedPending, PlayerContact, bInitialOverlap);
        bStartedOverlappingPlayer |= bInitialOverlap;
        FVector Allowed = Pending;
        if (bHitPlayer)
        {
            bPlayerBlocked = true;
            LastPlayerContactNormal = PlayerContact.Normal;
            LastPlayerContactComponent = PlayerContact.GetComponent() ? PlayerContact.GetComponent()->GetFName() : NAME_None;
            // A small world-space skin is independent of frame rate and step length.
            Allowed *= FMath::Max(0.f, PlayerContact.Time - 0.1f / FMath::Max(0.1f, Pending.Size2D()));
        }
        FVector GroundDelta;
        if (!ProjectGroundMove(Allowed, GroundDelta)) { bFloorRejected = true; break; }
        // Floor projection may change the vertical part after contact clipping.
        // Check that actual path too, including sloped ground beneath a player.
        FHitResult ProjectedContact;
        bool bProjectedOverlap = false;
        const bool bProjectedPlayerHit = SweepPlayerBody(GroundDelta, ProjectedContact, bProjectedOverlap);
        if (bProjectedPlayerHit)
        {
            bPlayerBlocked = true;
            LastPlayerContactNormal = ProjectedContact.Normal;
            LastPlayerContactComponent = ProjectedContact.GetComponent() ? ProjectedContact.GetComponent()->GetFName() : NAME_None;
            GroundDelta *= FMath::Max(0.f, ProjectedContact.Time - 0.1f / FMath::Max(0.1f, GroundDelta.Size()));
            PlayerContact = ProjectedContact;
            FVector SupportedDelta;
            // Clipping a 3D move does not necessarily preserve floor height on
            // slopes. Reject rather than applying an unswept vertical correction
            // or retaining an unsupported/intersecting end position.
            if (!ProjectGroundMove(FVector(GroundDelta.X, GroundDelta.Y, 0), SupportedDelta) ||
                !FMath::IsNearlyEqual(SupportedDelta.Z, GroundDelta.Z, 0.1f))
            { bFloorRejected = true; break; }
        }
        bStartedOverlappingPlayer |= bProjectedOverlap;
        FHitResult WorldContact;
        {
            FScopedMovementUpdate ScopedMove(Character->GetCapsuleComponent(), EScopedUpdate::DeferredUpdates);
            // An explicit sweep avoids SafeMove's unscheduled depenetration/launch.
            // MoveComponent already permits movement out of a root overlap.
            Movement->MoveUpdatedComponent(GroundDelta, Character->GetActorQuat(), true, &WorldContact);
            const float DistanceBefore = FVector::DistSquared2D(SegmentStart, GetRoamCenter());
            const float DistanceAfter = FVector::DistSquared2D(Character->GetActorLocation(), GetRoamCenter());
            const float LimitSquared = FMath::Square(RoamCenterLimit() + 0.01f);
            if (bLimitRoaming && DistanceAfter > LimitSquared && DistanceAfter >= DistanceBefore - 0.01f)
            { ScopedMove.RevertMove(); bBoundaryClipped = true; break; }
        }
        const FVector Achieved = Character->GetActorLocation() - SegmentStart;
        RemainingDistance = FMath::Max(0.f, RemainingDistance - Achieved.Size2D());
        // Player contact stops this step. Sliding around Spyro made the Bull push
        // across his body and lose its attack facing; only walls permit sliding.
        if (bHitPlayer || bProjectedPlayerHit) break;
        FVector Normal = FVector::ZeroVector;
        if (WorldContact.bBlockingHit)
        { bTerrainBlocked = true; Normal = WorldContact.Normal.GetSafeNormal2D(); }
        else if (bHitPlayer || bProjectedPlayerHit) Normal = PlayerContact.Normal.GetSafeNormal2D();
        else break;
        if (Normal.IsNearlyZero() || RemainingDistance < 0.01f) break;
        FVector Remainder = Pending - FVector(Achieved.X, Achieved.Y, 0);
        Remainder -= Normal * FMath::Min(0.f, FVector::DotProduct(Remainder, Normal));
        Pending = Remainder.GetClampedToMaxSize(RemainingDistance);
        if (Pending.IsNearlyZero(0.01f)) break;
    }
    LastStepDelta = Character->GetActorLocation() - Before;
    Movement->Velocity = LastStepDelta / TownSquareStep;
}

void UTownSquareEnemyBehaviorComponent::DropGemRange(int32 First, int32 Count)
{
    if (TownSquare::Bool(Dropper, TEXT("Cannot_Drop_Items"))) return;
    TownSquare::Call(Dropper, TEXT("Remove Gems We Perma Collected"));
    TownSquare::Call(Dropper, TEXT("Find All Items I Have But Shouldn't Drop"));
    auto* ItemsProperty = CastField<FArrayProperty>(TownSquare::Property(Dropper, TEXT("Items_to_Drop")));
    auto* PendingProperty = CastField<FArrayProperty>(TownSquare::Property(Dropper, TEXT("Items_I_Have_But_Shouldnt_Drop")));
    auto* SpawnedProperty = CastField<FArrayProperty>(TownSquare::Property(Dropper, TEXT("Items_I_Dropped")));
    if (!ItemsProperty || !PendingProperty || !SpawnedProperty) return;
    auto* ItemClass = CastField<FObjectPropertyBase>(ItemsProperty->Inner);
    auto* PendingBool = CastField<FBoolProperty>(PendingProperty->Inner);
    auto* SpawnedObject = CastField<FObjectPropertyBase>(SpawnedProperty->Inner);
    if (!ItemClass || !PendingBool || !SpawnedObject) return;
    FScriptArrayHelper Items(ItemsProperty, ItemsProperty->ContainerPtrToValuePtr<void>(Dropper));
    FScriptArrayHelper Pending(PendingProperty, PendingProperty->ContainerPtrToValuePtr<void>(Dropper));
    for (int32 Index = First; Index < First + Count; ++Index)
    {
        if (!Items.IsValidIndex(Index) || ReleasedGemIndices.Contains(Index)) continue;
        if (Pending.IsValidIndex(Index) && PendingBool->GetPropertyValue(Pending.GetRawPtr(Index))) continue;
        UClass* GemClass = Cast<UClass>(ItemClass->GetObjectPropertyValue(Items.GetRawPtr(Index)));
        if (!GemClass) continue; // Permanently collected entries retain their indices and become null.
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector Location = GetOwner()->GetActorLocation() + FVector(0, 0, TownSquare::Number(Dropper, TEXT("Item_Spawn_Height_Offset")));
        AActor* Gem = GetWorld()->SpawnActor<AActor>(GemClass, Location, GetOwner()->GetActorRotation(), Spawn);
        if (!Gem) continue;
        UFunction* Initialize = Gem->FindFunction(TEXT("Gem_Spawn_Process"));
        if (!Initialize) { Gem->Destroy(); continue; }
        FStructOnScope Params(Initialize);
        for (TFieldIterator<FProperty> It(Initialize); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            const FString Name = TownSquare::Key(It->GetName());
            void* Value = It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
            if (auto* P = CastField<FObjectPropertyBase>(*It))
            {
                if (Name == TEXT("playerwhospawnedme")) P->SetObjectPropertyValue(Value, TownSquare::ObjectValue(Damageable, TEXT("Deal Damage - Person Who Dealt the Damage")));
                if (Name == TEXT("objectispawnedfrom")) P->SetObjectPropertyValue(Value, GetOwner());
            }
            else if (auto* StringParam = CastField<FStrProperty>(*It))
            {
                if (Name == TEXT("nameofobjectwhospawnedme")) StringParam->SetPropertyValue(Value, GetOwner()->GetName());
            }
            else if (auto* IndexParam = CastField<FIntProperty>(*It))
            {
                if (Name == TEXT("spawnindex")) IndexParam->SetPropertyValue(Value, Index);
            }
        }
        // Register with the original dropper so checkpoint resets clean up emitted gems.
        FScriptArrayHelper Spawned(SpawnedProperty, SpawnedProperty->ContainerPtrToValuePtr<void>(Dropper));
        SpawnedObject->SetObjectPropertyValue(Spawned.GetRawPtr(Spawned.AddValue()), Gem);
        ReleasedGemIndices.Add(Index);
        ++GemsSpawned;
        if (UStaticMeshComponent* GemMesh = Gem->FindComponentByClass<UStaticMeshComponent>())
            if (GemMesh->IsSimulatingPhysics()) GemMesh->AddImpulse(FVector((Index % 3 - 1) * 100.f, 0, 550), NAME_None, true);
        Gem->ProcessEvent(Initialize, Params.GetStructMemory());
    }
}

void UTownSquareEnemyBehaviorComponent::OnRegister()
{
    Super::OnRegister();
    UpdateRoamPreview();
}
void UTownSquareEnemyBehaviorComponent::OnUnregister()
{
#if WITH_EDITORONLY_DATA
    if (RoamPreview) { RoamPreview->DestroyComponent(); RoamPreview = nullptr; }
    if (RoutePreview) { RoutePreview->DestroyComponent(); RoutePreview = nullptr; }
#endif
    Super::OnUnregister();
}
#if WITH_EDITOR
void UTownSquareEnemyBehaviorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    UpdateRoamPreview();
    Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
void UTownSquareEnemyBehaviorComponent::UpdateRoamPreview()
{
#if WITH_EDITORONLY_DATA
    // The Bull has no spline/waypoint authoring or duplicate Toreador preview.
    const auto* Toreador=Cast<UToreadorBehaviorComponent>(this);
    if (!Toreador) return;
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->GetRootComponent() || !GetWorld() ||
        (GetWorld()->WorldType != EWorldType::Editor && GetWorld()->WorldType != EWorldType::EditorPreview)) return;
    if (!RoamPreview)
    {
        RoamPreview = NewObject<USphereComponent>(Owner, NAME_None, RF_Transient);
        RoamPreview->SetupAttachment(Owner->GetRootComponent());
        RoamPreview->SetAbsolute(false, false, true);
        RoamPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        RoamPreview->SetGenerateOverlapEvents(false);
        RoamPreview->SetCanEverAffectNavigation(false);
        RoamPreview->SetHiddenInGame(true);
        RoamPreview->bIsEditorOnly = true;
        RoamPreview->bDrawOnlyIfSelected = true;
        RoamPreview->ShapeColor = FColor(50, 210, 255);
        RoamPreview->RegisterComponent();
    }
    const bool bSpline=Toreador->bUseRunPath && Toreador->GetRunPath();
    RoamPreview->SetWorldLocation(Owner->GetActorLocation());
    RoamPreview->SetSphereRadius(FMath::Max(300.f, Toreador->RoamRadius), false);
    RoamPreview->SetVisibility(bLimitRoaming && !bSpline);
    if (!RoutePreview)
    {
        RoutePreview=NewObject<USplineComponent>(Owner,NAME_None,RF_Transient);
        RoutePreview->SetupAttachment(Owner->GetRootComponent());
        RoutePreview->bIsEditorOnly=true; RoutePreview->SetHiddenInGame(true);
        RoutePreview->SetCollisionEnabled(ECollisionEnabled::NoCollision); RoutePreview->RegisterComponent();
    }
    RoutePreview->SetVisibility(!bSpline);
    if (bSpline) return;
    float Extent=1; for (const FVector& P:Toreador->RoutePoints) Extent=FMath::Max(Extent,P.Size2D()*WorldUnitsPerOriginalUnit);
    // Same conservative clearance as the configured paired collision boxes.
    const float Margin=(Toreador->LinkedBull ? 82.f : 72.f)*FMath::Sqrt(2.f)+2.f;
    const float Fit=FMath::Min(1.f,FMath::Max(1.f,FMath::Max(300.f,Toreador->RoamRadius)-Margin)*.8f/Extent);
    TArray<FVector> Preview;
    for (FVector P:Toreador->RoutePoints) { P*=WorldUnitsPerOriginalUnit; P.X*=Fit; P.Y*=Fit; Preview.Add(Owner->GetActorLocation()+FRotator(0,Owner->GetActorRotation().Yaw+Toreador->RouteYaw,0).RotateVector(P)); }
    RoutePreview->SetSplinePoints(Preview,ESplineCoordinateSpace::World,false);
    for (int32 I=0;I<Preview.Num();++I) RoutePreview->SetSplinePointType(I,ESplinePointType::Linear,false);
    RoutePreview->SetClosedLoop(true); RoutePreview->UpdateSpline();
    if (const auto* T=Cast<UToreadorBehaviorComponent>(this))
        RoamPreview->ShapeColor=T->LinkedBull && (!T->LinkedBull->FindComponentByClass<UBullBehaviorComponent>() || FVector::Dist2D(T->LinkedBull->GetActorLocation(),Owner->GetActorLocation())>T->RoamRadius-120.f) ? FColor::Red : FColor(50,210,255);
#endif
}

float UTownSquareEnemyBehaviorComponent::RoamCenterLimit() const
{
    float Margin = 0.f;
    // Both members fit the same route using the larger body's clearance.
    const UTownSquareEnemyBehaviorComponent* Members[] = {this, IsValid(Partner) ? Partner : nullptr};
    for (const auto* Member : Members)
    {
        if (!Member || !Member->Character) continue;
        Margin=FMath::Max(Margin,Member->Character->GetCapsuleComponent()->GetScaledCapsuleRadius());
        if (!Member->BodyCollision) continue;
        const FVector Extent = Member->BodyCollision->GetScaledBoxExtent();
        const FVector Offset = Member->BodyCollision->GetComponentLocation() - Member->Character->GetActorLocation();
        for (int32 X : {-1, 1}) for (int32 Y : {-1, 1}) for (int32 Z : {-1, 1})
            Margin = FMath::Max(Margin, (Offset + Member->BodyCollision->GetComponentQuat().RotateVector(
                Extent * FVector(X, Y, Z))).Size2D());
    }
    return FMath::Max(1.f, FMath::Max(300.f, TerritoryOwner()->ActiveRoamRadius) - Margin - 2.f);
}
FVector UTownSquareEnemyBehaviorComponent::ConstrainRoamMove(const FVector& Start, const FVector& Delta) const
{
    if (!bLimitRoaming) return Delta;
    const FVector Offset(Start.X - GetRoamCenter().X, Start.Y - GetRoamCenter().Y, 0);
    const FVector Travel(Delta.X, Delta.Y, 0);
    const float Radius = RoamCenterLimit();
    if ((Offset + Travel).SizeSquared() <= FMath::Square(Radius)) return Delta;
    const float A = Travel.SizeSquared(), C = Offset.SizeSquared() - FMath::Square(Radius);
    if (A < SMALL_NUMBER) return FVector::ZeroVector;
    const float B = FVector::DotProduct(Offset, Travel);
    const float Discriminant = B * B - A * C;
    // An external teleport/reset can put the actor outside its territory. Permit
    // only inward recovery, rather than trapping it there with every step zeroed.
    if (C > 0.f && B >= 0.f) return FVector::ZeroVector;
    if (C > 0.f && Discriminant < 0.f) return Delta * FMath::Clamp(-B / A, 0.f, 1.f);
    const float ExitTime = (-B + FMath::Sqrt(FMath::Max(0.f, Discriminant))) / A;
    return Delta * FMath::Clamp(ExitTime, 0.f, 1.f);
}


