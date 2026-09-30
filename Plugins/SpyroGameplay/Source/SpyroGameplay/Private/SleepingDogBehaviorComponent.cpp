#include "SleepingDogBehaviorComponent.h"
#include "SleepingDogAnimInstance.h"
#include "ToastyEncounterCollision.h"
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

namespace SleepingDog
{
void IgnorePlayer(FCollisionQueryParams& Query, AActor* Player, bool IncludePlayer = true)
{
    if (!IsValid(Player)) return;
    if (IncludePlayer) Query.AddIgnoredActor(Player);
    // Sparx is a child actor with a blocking capsule, not a component of Spyro.
    TArray<AActor*> Children;
    Player->GetAllChildActors(Children, true);
    Query.AddIgnoredActors(Children);
}
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



namespace
{
constexpr float DogStep=1.f/30.f;
const int32 DogFrames[]={20,10,25,1,20,40,28,20,10,25,1,20,40,10,10,10};
float DogDistance(const FVector& V) { return FMath::Max(FMath::Abs(V.X),FMath::Abs(V.Y))+.375f*FMath::Min(FMath::Abs(V.X),FMath::Abs(V.Y)); }
int32 DogMesh(int32 Clip) { return Clip>=13?2:(Clip>=7?1:0); }
}
USleepingDogBehaviorComponent::USleepingDogBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PrePhysics;
}
void USleepingDogBehaviorComponent::BeginPlay()
{
    Super::BeginPlay(); Character=Cast<ACharacter>(GetOwner());
    if (!Character) { SetComponentTickEnabled(false); return; }
    Mesh=Character->GetMesh();
    for (auto* C:Character->GetComponents())
    {
        if (C->GetClass()->GetName()==TEXT("Damageable_Com_C")) Damageable=C;
        if (C->GetClass()->GetName()==TEXT("Walking_AI_Character_C")) WalkingAI=C;
        if (C->GetClass()->GetName()==TEXT("Drops_Items_C")) Dropper=C;
        if (C->GetFName()==TEXT("SleepingDogBodyCollision")) BodyCollision=Cast<UBoxComponent>(C);
        if (C->GetFName()==TEXT("SleepingDogChargeSensor")) ChargeSensor=Cast<UBoxComponent>(C);
        if (C->GetFName()==TEXT("Attack_Radius")) if (auto* A=Cast<UPrimitiveComponent>(C))
        { A->SetGenerateOverlapEvents(false); A->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    }
    if (!Mesh || !Damageable || !WalkingAI || !Dropper || !BodyCollision || !ChargeSensor ||
        Animations.Num()!=16 || Animations.Contains(nullptr) || Meshes.Num()!=3 || Meshes.Contains(nullptr))
    { UE_LOG(LogTemp,Error,TEXT("Sleeping Dog %s has incomplete contracts/assets."),*Character->GetName()); SetComponentTickEnabled(false); return; }
    Home=Character->GetActorTransform(); Heading=PreviousHeading=Home.Rotator().Yaw;
    PreviousLocation=LastSafeGround=Home.GetLocation(); MeshOffset=Mesh->GetRelativeLocation();
    Mesh->AddTickPrerequisiteComponent(this);
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->bEnableUpdateRateOptimizations=false;
    ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
    BoneTransformsHandle=Mesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this,&USleepingDogBehaviorComponent::UpdateBodyCollision));
    SleepingDog::Bind(Damageable,TEXT("Call Deal_Damage"),WalkingAI,TEXT("On Damaged"),true);
    SleepingDog::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(USleepingDogBehaviorComponent,OnAcceptedDamage));
    SleepingDog::Bind(Dropper,TEXT("Item Dropper Successfully Reset"),this,GET_FUNCTION_NAME_CHECKED(USleepingDogBehaviorComponent,OnDropperReset));
    ChargeSensor->OnComponentBeginOverlap.AddUniqueDynamic(this,&USleepingDogBehaviorComponent::OnChargeSensorOverlap);
    SleepingDog::SetNumber(Damageable,TEXT("Hit Points"),2);
    SleepingDog::SetBool(WalkingAI,TEXT("Poofs_On_Death"),false);
    SleepingDog::SetNumber(WalkingAI,TEXT("Death Launch Upwards Force"),0);
    SleepingDog::SetNumber(WalkingAI,TEXT("Death Launch Forwards Force"),0);
    SleepingDog::SetNumber(Character,TEXT("Corpse Poof Delay"),100000.f);
    TakeMovementControl(); SelectClip(0,false);
}
void USleepingDogBehaviorComponent::TakeMovementControl()
{
    Character->SetActorTickEnabled(false); WalkingAI->SetComponentTickEnabled(false);
    if (auto* C=Character->GetController()) C->StopMovement();
    auto* M=Character->GetCharacterMovement(); M->DisableMovement(); M->SetComponentTickEnabled(false); M->bEnablePhysicsInteraction=false;
    Character->ConsumeMovementInputVector();
    if (Mesh->GetAnimClass()!=USleepingDogAnimInstance::StaticClass()) Mesh->SetAnimInstanceClass(USleepingDogAnimInstance::StaticClass());
}
void USleepingDogBehaviorComponent::SelectClip(int32 Clip,bool Blend)
{
    Clip=FMath::Clamp(Clip,0,15);
    const int32 NewMesh=DogMesh(Clip);
    if (NewMesh!=ActiveMesh)
    {
        ActiveMesh=NewMesh; Mesh->SetSkeletalMesh(Meshes[NewMesh]); Mesh->EmptyOverrideMaterials();
        Mesh->SetAnimInstanceClass(USleepingDogAnimInstance::StaticClass()); Blend=false;
    }
    if (Blend && NextClip==Clip) return;
    if (Blend) { CurrentClip=NextClip; CurrentFrame=NextFrame; NextClip=Clip; NextFrame=0; Progress=ProgressPerStep=16; }
    else { CurrentClip=NextClip=Clip; CurrentFrame=0; NextFrame=DogFrames[Clip]>1?1:0; Progress=0; ProgressPerStep=DogFrames[Clip]==1?0:(Clip==6?64:32); EmitFrameSound(); }
}
bool USleepingDogBehaviorComponent::AdvanceAnimation()
{
    Progress+=ProgressPerStep; if (Progress<64) return false; Progress&=63; bool Complete=false;
    if (CurrentClip!=NextClip) { CurrentClip=NextClip; CurrentFrame=NextFrame; NextFrame=DogFrames[CurrentClip]>1?1:0; Progress=0; ProgressPerStep=DogFrames[CurrentClip]==1?0:(CurrentClip==6?64:32); }
    else { CurrentFrame=NextFrame; if (++NextFrame>=DogFrames[CurrentClip]) { NextFrame=0; Complete=true; } }
    EmitFrameSound(); return Complete;
}
void USleepingDogBehaviorComponent::GetPoseInputs(UAnimSequence*& A,UAnimSequence*& B,float& TimeA,float& TimeB,float& Alpha) const
{
    A=Animations.IsValidIndex(CurrentClip)?Animations[CurrentClip]:nullptr; B=Animations.IsValidIndex(NextClip)?Animations[NextClip]:nullptr;
    TimeA=A?A->GetPlayLength()*CurrentFrame/FMath::Max(1,DogFrames[CurrentClip]-1):0;
    TimeB=B?B->GetPlayLength()*NextFrame/FMath::Max(1,DogFrames[NextClip]-1):0;
    Alpha=FMath::Clamp((Progress+Accumulator/DogStep*ProgressPerStep)/64.f,0.f,1.f);
}
void USleepingDogBehaviorComponent::PlaySound(int32 Slot)
{
    if (SoundCueHistory.Num()>=256) SoundCueHistory.RemoveAt(0); SoundCueHistory.Add(Slot);
    PlayingSounds.RemoveAll([](UAudioComponent* A){return !IsValid(A)||!A->IsPlaying();});
    if (OriginalSounds.IsValidIndex(Slot) && OriginalSounds[Slot])
        if (auto* A=UGameplayStatics::SpawnSoundAttached(OriginalSounds[Slot],Character->GetRootComponent(),NAME_None,FVector::ZeroVector,EAttachLocation::KeepRelativeOffset,true,1,1,0,SoundAttenuation)) PlayingSounds.Add(A);
}
void USleepingDogBehaviorComponent::EmitFrameSound()
{
    if (CurrentClip==1 || CurrentClip==8) { if (CurrentFrame==0) PlaySound(2); if (CurrentFrame==5) PlaySound(3); }
    if (CurrentClip==4 || CurrentClip==11) { if (CurrentFrame==0) PlaySound(4); if (CurrentFrame==7) PlaySound(5); if (CurrentFrame==14) PlaySound(6); }
    if (CurrentClip==6 && CurrentFrame==4) PlaySound(7);
    if (CurrentClip==13 && CurrentFrame==3) PlaySound(1);
}
void USleepingDogBehaviorComponent::StopSounds() { for (auto* A:PlayingSounds) if (IsValid(A)) A->Stop(); PlayingSounds.Reset(); }
FVector USleepingDogBehaviorComponent::Feet(AActor* Actor) const
{
    FVector P=Actor->GetActorLocation(); if (auto* C=Cast<ACharacter>(Actor)) P.Z-=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); return P;
}
float USleepingDogBehaviorComponent::Distance(const FVector& Point) const { return DogDistance(Point-Character->GetActorLocation())/WorldUnitsPerOriginalUnit; }
bool USleepingDogBehaviorComponent::Face(const FVector& Point,float TurnUnits,float ToleranceUnits)
{
    const float Desired=FMath::RoundToFloat((Point-Character->GetActorLocation()).Rotation().Yaw*256.f/360.f)*360.f/256.f;
    const float Error=FMath::FindDeltaAngleDegrees(Heading,Desired);
    Heading=FMath::UnwindDegrees(Heading+FMath::Clamp(Error,-TurnUnits*360.f/256.f,TurnUnits*360.f/256.f));
    return FMath::Abs(Error)<=ToleranceUnits*360.f/256.f;
}
bool USleepingDogBehaviorComponent::GroundAt(const FVector& Point,FVector& Ground) const
{
    return ToastyEncounterCollision::Floor(Character,Pursuer,Point,WorldUnitsPerOriginalUnit,Ground);
}
void USleepingDogBehaviorComponent::UpdateBodyCollision()
{
    ToastyEncounterCollision::FitBody(Mesh,BodyCollision,ChargeSensor,false);
    HitPlayer();
}
FVector USleepingDogBehaviorComponent::SweepMove(const FVector& Delta,bool PlayerBlocks)
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SleepingDogMove),false,Character); SleepingDog::IgnorePlayer(Q,Pursuer,!PlayerBlocks);
    FCollisionQueryParams WorldQ=Q; SleepingDog::IgnorePlayer(WorldQ,Pursuer);
    FHitResult Wall; const FVector Start=Character->GetActorLocation();
    auto* Capsule=Character->GetCapsuleComponent();
    GetWorld()->SweepSingleByChannel(Wall,Start,Start+Delta,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-1),WorldQ);
    if (Wall.bBlockingHit && State==ESleepingDogState::Pouncing)
        UE_LOG(LogTemp,Verbose,TEXT("Sleeping Dog airborne sweep: %s / %s"),*GetNameSafe(Wall.GetActor()),*GetNameSafe(Wall.GetComponent()));
    float Fraction=Wall.bBlockingHit?FMath::Max(0.f,Wall.Time-.002f):1.f;
    float PlayerContactTime=MAX_flt;
    if (PlayerBlocks)
    {
        FCollisionObjectQueryParams O; O.AddObjectTypesToQuery(ECC_Pawn); O.AddObjectTypesToQuery(ECC_GameTraceChannel4);
        TArray<FHitResult> Hits;
        GetWorld()->SweepMultiByObjectType(Hits,BodyCollision->GetComponentLocation(),BodyCollision->GetComponentLocation()+Delta,BodyCollision->GetComponentQuat(),O,FCollisionShape::MakeBox(BodyCollision->GetScaledBoxExtent()),Q);
        for (const auto& H:Hits)
        {
            const auto* C=H.GetComponent(); if (!C || C->GetCollisionResponseToChannel(ECC_WorldDynamic)!=ECR_Block) continue;
            if (ToastyEncounterCollision::SeparatingContact(H,BodyCollision,Delta)) continue;
            Fraction=FMath::Min(Fraction,FMath::Max(0.f,H.Time-.002f));
            if (H.GetActor()==Pursuer) PlayerContactTime=FMath::Min(PlayerContactTime,H.Time);
        }
    }
    bBlocked=Fraction<.99f; Character->SetActorLocation(Start+Delta*Fraction,false,nullptr,ETeleportType::TeleportPhysics);
    if (PlayerContactTime<=Fraction+.002f) HitPlayer(true);
    return Delta*Fraction;
}
bool USleepingDogBehaviorComponent::GroundMove(const FVector& Delta)
{
    ToastyEncounterCollision::LiftFromFloor(Character,Pursuer,WorldUnitsPerOriginalUnit);
    FVector Ground; const FVector Start=Character->GetActorLocation();
    if (!GroundAt(Start+Delta,Ground) || FMath::Abs(Ground.Z-Start.Z)>600*WorldUnitsPerOriginalUnit) { bBlocked=true; return false; }
    // Footprint probes reject unsupported landings/ledges, rather than balancing the root over a void.
    const float R=Character->GetCapsuleComponent()->GetScaledCapsuleRadius()*.7f;
    for (const FVector& Offset:{FVector(R,0,0),FVector(-R,0,0),FVector(0,R,0),FVector(0,-R,0)})
    { FVector Edge; if (!GroundAt(Start+Delta+Offset,Edge) || FMath::Abs(Edge.Z-Ground.Z)>600*WorldUnitsPerOriginalUnit) { bBlocked=true; return false; } }
    FVector Move=Delta; Move.Z=Ground.Z-Start.Z; const FVector Actual=SweepMove(Move,true);
    if (Actual.SizeSquared()>1) LastSafeGround=Character->GetActorLocation(); return !bBlocked;
}
void USleepingDogBehaviorComponent::BeginPounce()
{
    ToastyEncounterCollision::LiftFromFloor(Character,Pursuer,WorldUnitsPerOriginalUnit);
    State=ESleepingDogState::Aiming; StateTicks=0; bHitThisAttack=bSlidingOffPlayer=false;
    if (IsValid(Pursuer)) PounceTarget=Pursuer->GetActorLocation();
    SelectClip((Health==1?7:0)+2);
}
void USleepingDogBehaviorComponent::StepOriginal()
{
    PreviousLocation=Character->GetActorLocation(); PreviousHeading=Heading; ++SimulationTicks; ++StateTicks;
    if (bFirstTick)
    {
        ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
        // Base_Enemy_BP binds WalkingAI during the actor's BeginPlay, after component BeginPlay.
        SleepingDog::Bind(Damageable,TEXT("Call Deal_Damage"),WalkingAI,TEXT("On Damaged"),true);
        SleepingDog::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(USleepingDogBehaviorComponent,OnAcceptedDamage));
        bFirstTick=false;
    }
    TakeMovementControl();
    if (!IsValid(Pursuer)) Pursuer=UGameplayStatics::GetPlayerCharacter(this,0);
    if (State==ESleepingDogState::Dead) return;
    const bool Complete=AdvanceAnimation(); if (Cooldown>0) --Cooldown;
    const int32 Base=Health==1?7:0;
    if (State==ESleepingDogState::Dying)
    {
        GroundMove(FRotator(0,Heading+180,0).Vector()*DeathSpeed*WorldUnitsPerOriginalUnit); DeathSpeed=FMath::Max(0.f,DeathSpeed-12);
        if (Complete || StateTicks>60) FinishCorpse(); return;
    }
    if (State==ESleepingDogState::Singed)
    {
        if (CurrentFrame<2 && IsValid(Pursuer)) PounceTarget=Pursuer->GetActorLocation();
        Face(PounceTarget,7,0);
        if (NextFrame>=9) { State=ESleepingDogState::Aiming; StateTicks=0; SelectClip(8,false); }
        return;
    }
    if (State==ESleepingDogState::Sleeping)
    {
        if (Cooldown==0 && IsValid(Pursuer) && Distance(Pursuer->GetActorLocation())<2200 && FMath::Abs(Feet(Pursuer).Z-Feet(Character).Z)<800*WorldUnitsPerOriginalUnit)
        {
            FHitResult Wall; FCollisionQueryParams Q(SCENE_QUERY_STAT(SleepingDogSight),false,Character); SleepingDog::IgnorePlayer(Q,Pursuer);
            if (!GetWorld()->LineTraceSingleByChannel(Wall,Character->GetActorLocation(),Pursuer->GetActorLocation(),ECC_Visibility,Q)) BeginPounce();
        }
        return;
    }
    if (State==ESleepingDogState::Aiming)
    {
        // 80080B68: vertical speed = 300 + floor(distance * 35 / 1024).
        VerticalSpeed=300+FMath::FloorToFloat(Distance(IsValid(Pursuer)?Pursuer->GetActorLocation():PounceTarget)*35.f/1024.f);
        if (NextClip!=Base+2) SelectClip(Base+2);
        if (Face(PounceTarget,10,4)) { State=ESleepingDogState::Pouncing; StateTicks=0; bHitThisAttack=false; ++PounceCount; PlaySound(1); }
        return;
    }
    if (State==ESleepingDogState::Pouncing)
    {
        FVector Ground;
        if (!GroundAt(Character->GetActorLocation(),Ground))
        { Character->SetActorLocation(LastSafeGround,false,nullptr,ETeleportType::TeleportPhysics); State=ESleepingDogState::Landing; StateTicks=0; return; }
        FVector Horizontal=FVector::ZeroVector;
        if (!bSlidingOffPlayer && Distance(PounceTarget)>150) Horizontal=FRotator(0,Heading,0).Vector()*230*WorldUnitsPerOriginalUnit;
        // Late descent can correct toward Spyro by 80 original units; facing remains committed.
        if (!bSlidingOffPlayer && IsValid(Pursuer))
        {
            const float D=Distance(Pursuer->GetActorLocation());
            if (D>80 && D<800 && VerticalSpeed<100 && Character->GetActorLocation().Z-Ground.Z>500*WorldUnitsPerOriginalUnit)
                Horizontal=(Pursuer->GetActorLocation()-Character->GetActorLocation()).GetSafeNormal2D()*80*WorldUnitsPerOriginalUnit;
        }
        // Don't launch horizontally beyond supported ground. A blocked jump still completes vertically.
        FVector DestinationFloor=FVector::ZeroVector;
        const FVector LandingPoint=Character->GetActorLocation()+Horizontal;
        bool Supported=GroundAt(LandingPoint,DestinationFloor);
        const float Margin=Character->GetCapsuleComponent()->GetScaledCapsuleRadius()*.7f;
        for (const FVector& Offset:{FVector(Margin,0,0),FVector(-Margin,0,0),FVector(0,Margin,0),FVector(0,-Margin,0)})
        { FVector Edge; if (!GroundAt(LandingPoint+Offset,Edge) || FMath::Abs(Edge.Z-DestinationFloor.Z)>600*WorldUnitsPerOriginalUnit) Supported=false; }
        if (!Supported) Horizontal=FVector::ZeroVector;
        SweepMove(Horizontal,true);
        FVector P=Character->GetActorLocation(); P.Z+=VerticalSpeed*WorldUnitsPerOriginalUnit; VerticalSpeed=FMath::Max(-600.f,VerticalSpeed-50);
        GroundAt(P,Ground);
        const float VerticalDelta=FMath::Max(P.Z,Ground.Z)-Character->GetActorLocation().Z;
        const FVector VerticalMove=SweepMove(FVector(0,0,VerticalDelta),true);
        if (Character->GetActorLocation().Z<=Ground.Z+1.f && VerticalSpeed<=0)
        {
            LastSafeGround=Character->GetActorLocation();
            State=ESleepingDogState::Landing; StateTicks=0; PlaySound(0);
            if (NextFrame<13) { CurrentFrame=13; NextFrame=14; Progress=0; }
        }
        else
        {
            // A stationary or invincible player can hold the dog above the
            // floor. Slide off the contact instead of tunnelling down or hanging.
            if (VerticalDelta<0 && VerticalMove.Z>VerticalDelta+1.f && IsValid(Pursuer))
            {
                // Commit to the escape: resuming the forward pounce on the next
                // step would undo this slide and trap the dog over Spyro.
                bSlidingOffPlayer=true;
                FVector Away=(Character->GetActorLocation()-Pursuer->GetActorLocation()).GetSafeNormal2D();
                if (Away.IsNearlyZero()) Away=FRotator(0,Heading,0).Vector();
                SweepMove(Away*100*WorldUnitsPerOriginalUnit,true);
            }
            if (NextFrame==13) { CurrentFrame=8; NextFrame=9; Progress=0; }
            HitPlayer();
        }
        return;
    }
    if (State==ESleepingDogState::Landing)
    {
        if (NextFrame>=24 || Complete || StateTicks>50) { State=ESleepingDogState::Returning; StateTicks=0; SelectClip(Base+1); }
        return;
    }
    if (State==ESleepingDogState::Returning)
    {
        if (Distance(Home.GetLocation())<180)
        {
            if (Face(Home.GetLocation()+Home.Rotator().Vector()*1000,6,5)) { State=ESleepingDogState::Sleeping; Cooldown=60; StateTicks=0; SelectClip(Base); }
        }
        else if (Face(Home.GetLocation(),6,20)) GroundMove(FRotator(0,Heading,0).Vector()*130*WorldUnitsPerOriginalUnit);
        // Terrain can change during play. Recover in place rather than chase through walls forever.
        if (StateTicks>180) { State=ESleepingDogState::Sleeping; Cooldown=60; StateTicks=0; SelectClip(Base); }
    }
}
void USleepingDogBehaviorComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction); if (!Character || !Character->HasAuthority()) return;
    if (SleepingDog::Bool(Damageable,TEXT("Frozen")) || SleepingDog::Bool(Damageable,TEXT("Paralyzed_by_Fear")) || SleepingDog::Bool(Dropper,TEXT("Reset_in_Progress"))) return;
    Accumulator+=FMath::Max(0.f,DeltaTime); int32 Steps=0;
    while (Accumulator+KINDA_SMALL_NUMBER>=DogStep && Steps++<30) { Accumulator=FMath::Max(0.f,Accumulator-DogStep); StepOriginal(); }
    Accumulator=FMath::Min(Accumulator,DogStep); const float Alpha=Accumulator/DogStep;
    Mesh->SetWorldLocation(FMath::Lerp(PreviousLocation,Character->GetActorLocation(),Alpha)+Home.TransformVector(MeshOffset));
    Mesh->SetWorldRotation(FRotator(0,PreviousHeading+FMath::FindDeltaAngleDegrees(PreviousHeading,Heading)*Alpha-90,0));
}
void USleepingDogBehaviorComponent::HitPlayer(bool SweptContact)
{
    if (bHitThisAttack || !IsValid(Pursuer) || State!=ESleepingDogState::Pouncing) return;
    if (!SweptContact && !ToastyEncounterCollision::TouchesPlayer(BodyCollision,Pursuer)) return;
    FHitResult Wall; FCollisionQueryParams Q(SCENE_QUERY_STAT(SleepingDogAttack),false,Character); SleepingDog::IgnorePlayer(Q,Pursuer);
    if (GetWorld()->LineTraceSingleByChannel(Wall,Character->GetActorLocation(),Pursuer->GetActorLocation(),ECC_Visibility,Q)) return;
    UFunction* F=Pursuer->FindFunction(TEXT("Deal Damage to Player")); if (!F) return;
    FStructOnScope Params(F); UObject* D=SleepingDog::ObjectValue(Pursuer,TEXT("Damageable")); const int32 Before=SleepingDog::Number(D,TEXT("Hit Points"));
    for (TFieldIterator<FProperty> It(F);It && It->HasAnyPropertyFlags(CPF_Parm);++It)
    {
        void* V=It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
        if (auto* P=CastField<FByteProperty>(*It)) P->SetPropertyValue(V,1);
        if (auto* P=CastField<FStructProperty>(*It)) if (P->Struct==TBaseStructure<FVector>::Get()) *static_cast<FVector*>(V)=(Pursuer->GetActorLocation()-Character->GetActorLocation()).GetSafeNormal2D();
        if (auto* P=CastField<FObjectPropertyBase>(*It)) P->SetObjectPropertyValue(V,Character);
    }
    bHitThisAttack=true; Pursuer->ProcessEvent(F,Params.GetStructMemory());
    if (SleepingDog::Number(D,TEXT("Hit Points"))<Before) ++AcceptedPlayerHits;
}
void USleepingDogBehaviorComponent::OnChargeSensorOverlap(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool bSweep,const FHitResult& Hit)
{
    if (!bDefeated && Other && Other!=Character && OtherComponent && OtherComponent->GetCollisionObjectType()==ECC_GameTraceChannel4)
    { auto* Capsule=Character->GetCapsuleComponent(); Capsule->OnComponentBeginOverlap.Broadcast(Capsule,Other,OtherComponent,BodyIndex,bSweep,Hit); }
}
void USleepingDogBehaviorComponent::OnAcceptedDamage()
{
    if (!Character->HasAuthority() || bDefeated || State==ESleepingDogState::Singed || SleepingDog::Bool(Damageable,TEXT("Invincible")) || SleepingDog::Bool(Damageable,TEXT("Frozen")) || SleepingDog::Bool(Dropper,TEXT("Reset_in_Progress"))) return;
    // Inspected Damage_Types: Burn=2, Ram=5. Original class335 only accepts flame flags.
    if (SleepingDog::Number(Damageable,TEXT("Deal Damage - Damage Type"))!=2) return;
    --Health; SleepingDog::SetNumber(Damageable,TEXT("Hit Points"),Health);
    if (Health<=0) { Defeat(); return; }
    State=ESleepingDogState::Singed; StateTicks=0; VerticalSpeed=0; bHitThisAttack=true;
    if (IsValid(Pursuer)) PounceTarget=Pursuer->GetActorLocation(); SelectClip(13,false);
}
void USleepingDogBehaviorComponent::Defeat()
{
    bDefeated=true; State=ESleepingDogState::Dying; StateTicks=0; DeathSpeed=230; bHitThisAttack=true;
    DropGemRange(0,1); const bool Suppressed=SleepingDog::Bool(Dropper,TEXT("Cannot_Drop_Items")); SleepingDog::SetBool(Dropper,TEXT("Cannot_Drop_Items"),true);
    if (auto* P=CastField<FMulticastDelegateProperty>(SleepingDog::Property(Damageable,TEXT("Damage Was Successfully Dealt"))))
        if (auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(Damageable))) D->ProcessMulticastDelegate<UObject>(nullptr);
    SleepingDog::SetBool(Dropper,TEXT("Cannot_Drop_Items"),Suppressed); TakeMovementControl(); SelectClip(11);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision); ChargeSensor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void USleepingDogBehaviorComponent::FinishCorpse()
{
    if (bCorpseFinished) return; bCorpseFinished=true; State=ESleepingDogState::Dead; StopSounds();
    Character->SetActorHiddenInGame(true); Character->SetActorEnableCollision(false);
    if (auto* P=CastField<FMulticastDelegateProperty>(SleepingDog::Property(WalkingAI,TEXT("Corpse Poofed"))))
        if (auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(WalkingAI))) D->ProcessMulticastDelegate<UObject>(nullptr);
}
void USleepingDogBehaviorComponent::OnDropperReset()
{
    StopSounds(); SoundCueHistory.Reset(); ReleasedGemIndices.Reset(); GemsSpawned=0; Health=2;
    State=ESleepingDogState::Sleeping; StateTicks=0; Cooldown=70; bDefeated=bHitThisAttack=bCorpseFinished=bBlocked=bSlidingOffPlayer=false;
    Accumulator=VerticalSpeed=DeathSpeed=0; AcceptedPlayerHits=PounceCount=0;
    Character->SetActorTransform(Home,false,nullptr,ETeleportType::TeleportPhysics); PreviousLocation=LastSafeGround=Home.GetLocation(); Heading=PreviousHeading=Home.Rotator().Yaw;
    Character->SetActorHiddenInGame(false); Character->SetActorEnableCollision(true);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); ChargeSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
    SleepingDog::SetNumber(Damageable,TEXT("Hit Points"),2); TakeMovementControl(); SelectClip(0,false);
}
void USleepingDogBehaviorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopSounds();
    if (Mesh) Mesh->UnregisterOnBoneTransformsFinalizedDelegate(BoneTransformsHandle);
    if (ChargeSensor) ChargeSensor->OnComponentBeginOverlap.RemoveDynamic(this,&USleepingDogBehaviorComponent::OnChargeSensorOverlap);
    SleepingDog::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(USleepingDogBehaviorComponent,OnAcceptedDamage),true);
    SleepingDog::Bind(Dropper,TEXT("Item Dropper Successfully Reset"),this,GET_FUNCTION_NAME_CHECKED(USleepingDogBehaviorComponent,OnDropperReset),true);
    Super::EndPlay(Reason);
}
void USleepingDogBehaviorComponent::DropGemRange(int32 First, int32 Count)
{
    if (SleepingDog::Bool(Dropper, TEXT("Cannot_Drop_Items"))) return;
    SleepingDog::Call(Dropper, TEXT("Remove Gems We Perma Collected"));
    SleepingDog::Call(Dropper, TEXT("Find All Items I Have But Shouldn't Drop"));
    auto* ItemsProperty = CastField<FArrayProperty>(SleepingDog::Property(Dropper, TEXT("Items_to_Drop")));
    auto* PendingProperty = CastField<FArrayProperty>(SleepingDog::Property(Dropper, TEXT("Items_I_Have_But_Shouldnt_Drop")));
    auto* SpawnedProperty = CastField<FArrayProperty>(SleepingDog::Property(Dropper, TEXT("Items_I_Dropped")));
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
        const FVector Location = GetOwner()->GetActorLocation() + FVector(0, 0, SleepingDog::Number(Dropper, TEXT("Item_Spawn_Height_Offset")));
        AActor* Gem = GetWorld()->SpawnActor<AActor>(GemClass, Location, GetOwner()->GetActorRotation(), Spawn);
        if (!Gem) continue;
        UFunction* Initialize = Gem->FindFunction(TEXT("Gem_Spawn_Process"));
        if (!Initialize) { Gem->Destroy(); continue; }
        FStructOnScope Params(Initialize);
        for (TFieldIterator<FProperty> It(Initialize); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            const FString Name = SleepingDog::Key(It->GetName());
            void* Value = It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
            if (auto* P = CastField<FObjectPropertyBase>(*It))
            {
                if (Name == TEXT("playerwhospawnedme")) P->SetObjectPropertyValue(Value, SleepingDog::ObjectValue(Damageable, TEXT("Deal Damage - Person Who Dealt the Damage")));
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
