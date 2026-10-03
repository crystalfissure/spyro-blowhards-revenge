#include "ToastyBehaviorComponent.h"
#include "SpyroMeleeContact.h"
#include "ToastyEncounterCollision.h"
#include "ToastyAnimInstance.h"
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

namespace Toasty
{
void IgnorePlayer(FCollisionQueryParams& Query, AActor* Player, bool IncludePlayer = true)
{
    if (!IsValid(Player)) return;
    if (IncludePlayer) Query.AddIgnoredActor(Player);
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



#include "SleepingDogBehaviorComponent.h"

namespace { constexpr float ToastyStep=1.f/30.f;
const int32 ToastyFrames[]={33,11,15,19,35,15,1,25,10,25,20,33};
int32 ToastyMesh(int32 C) { return C>=7?3:(C==5?2:(C==4?1:0)); }
float ToastyDistance(const FVector& V) { return FMath::Max(FMath::Abs(V.X),FMath::Abs(V.Y))+.375f*FMath::Min(FMath::Abs(V.X),FMath::Abs(V.Y)); }
}
void UToastyBehaviorComponent::TakeMovementControl()
{
    Character->SetActorTickEnabled(false); WalkingAI->SetComponentTickEnabled(false);
    if (auto* C=Character->GetController()) C->StopMovement();
    auto* M=Character->GetCharacterMovement(); M->DisableMovement(); M->SetComponentTickEnabled(false); M->bEnablePhysicsInteraction=false;
    Character->ConsumeMovementInputVector();
    if (Mesh->GetAnimClass()!=UToastyAnimInstance::StaticClass()) Mesh->SetAnimInstanceClass(UToastyAnimInstance::StaticClass());
}
void UToastyBehaviorComponent::SelectClip(int32 Clip,bool Blend)
{
    Clip=FMath::Clamp(Clip,0,11);
    const int32 NewMesh=ToastyMesh(Clip);
    if (NewMesh!=ActiveMesh)
    {
        // Clear the old proxy and publish the new clip before synchronous pose
        // initialization; costume/sheep rigs use different bone containers.
        Mesh->SetAnimInstanceClass(nullptr);
        CurrentClip=NextClip=Clip; CurrentFrame=Progress=0; NextFrame=ToastyFrames[Clip]>1?1:0;
        ActiveMesh=NewMesh; Mesh->SetSkeletalMesh(Meshes[NewMesh]);
        const float Scale=NewMesh==3?.292208f:.584416f; Mesh->SetRelativeScale3D(FVector(Scale));
        // MeshOffset is local; Home.TransformVector applies actor scale later.
        MeshOffset.Z=-Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-(Meshes[NewMesh]->GetBounds().Origin.Z-Meshes[NewMesh]->GetBounds().BoxExtent.Z)*Scale; Mesh->EmptyOverrideMaterials();
        Mesh->SetAnimInstanceClass(UToastyAnimInstance::StaticClass()); Blend=false;
    }
    if (Blend && NextClip==Clip) return;
    if (Blend) { CurrentClip=NextClip; CurrentFrame=NextFrame; NextClip=Clip; NextFrame=0; Progress=ProgressPerStep=16; }
    else { CurrentClip=NextClip=Clip; CurrentFrame=0; NextFrame=ToastyFrames[Clip]>1?1:0; Progress=0; ProgressPerStep=ToastyFrames[Clip]==1?0:32; EmitFrameSound(); }
}
bool UToastyBehaviorComponent::AdvanceAnimation()
{
    Progress+=ProgressPerStep; if (Progress<64) return false; Progress&=63; bool Complete=false;
    if (CurrentClip!=NextClip) { CurrentClip=NextClip; CurrentFrame=NextFrame; NextFrame=ToastyFrames[CurrentClip]>1?1:0; Progress=0; ProgressPerStep=ToastyFrames[CurrentClip]==1?0:32; }
    else { CurrentFrame=NextFrame; if (++NextFrame>=ToastyFrames[CurrentClip]) { NextFrame=0; Complete=true; } }
    EmitFrameSound(); return Complete;
}
void UToastyBehaviorComponent::GetPoseInputs(UAnimSequence*& A,UAnimSequence*& B,float& TimeA,float& TimeB,float& Alpha) const
{
    A=Animations.IsValidIndex(CurrentClip)?Animations[CurrentClip]:nullptr; B=Animations.IsValidIndex(NextClip)?Animations[NextClip]:nullptr;
    TimeA=A?A->GetPlayLength()*CurrentFrame/FMath::Max(1,ToastyFrames[CurrentClip]-1):0;
    TimeB=B?B->GetPlayLength()*NextFrame/FMath::Max(1,ToastyFrames[NextClip]-1):0;
    Alpha=FMath::Clamp((Progress+Accumulator/ToastyStep*ProgressPerStep)/64.f,0.f,1.f);
}
void UToastyBehaviorComponent::PlaySound(int32 Slot)
{
    if (SoundCueHistory.Num()>=256) SoundCueHistory.RemoveAt(0); SoundCueHistory.Add(Slot);
    PlayingSounds.RemoveAll([](UAudioComponent* A){return !IsValid(A)||!A->IsPlaying();});
    if (OriginalSounds.IsValidIndex(Slot) && OriginalSounds[Slot])
        if (auto* A=UGameplayStatics::SpawnSoundAttached(OriginalSounds[Slot],Character->GetRootComponent(),NAME_None,FVector::ZeroVector,EAttachLocation::KeepRelativeOffset,true,1,1,0,SoundAttenuation)) PlayingSounds.Add(A);
}
void UToastyBehaviorComponent::StopSounds() { for (auto* A:PlayingSounds) if (IsValid(A)) A->Stop(); PlayingSounds.Reset(); }
FVector UToastyBehaviorComponent::Feet(AActor* Actor) const
{
    FVector P=Actor->GetActorLocation(); if (auto* C=Cast<ACharacter>(Actor)) P.Z-=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); return P;
}
float UToastyBehaviorComponent::Distance(const FVector& Point) const { return ToastyDistance(Point-Character->GetActorLocation())/WorldUnitsPerOriginalUnit; }
bool UToastyBehaviorComponent::Face(const FVector& Point,float TurnUnits,float ToleranceUnits)
{
    const float Desired=FMath::RoundToFloat((Point-Character->GetActorLocation()).Rotation().Yaw*256.f/360.f)*360.f/256.f;
    const float Error=FMath::FindDeltaAngleDegrees(Heading,Desired);
    Heading=FMath::UnwindDegrees(Heading+FMath::Clamp(Error,-TurnUnits*360.f/256.f,TurnUnits*360.f/256.f));
    return FMath::Abs(Error)<=ToleranceUnits*360.f/256.f;
}
bool UToastyBehaviorComponent::GroundAt(const FVector& Point,FVector& Ground) const
{
    return ToastyEncounterCollision::Floor(Character,Pursuer,Point,WorldUnitsPerOriginalUnit,Ground);
}
void UToastyBehaviorComponent::UpdateBodyCollision()
{
    ToastyEncounterCollision::FitBody(Mesh,BodyCollision,ChargeSensor,ActiveMesh!=3);
}
bool UToastyBehaviorComponent::FindTerrainObstacle(const FVector& Start,const FVector& Delta,FHitResult& Obstacle) const
{
    const auto* Capsule=Character->GetCapsuleComponent();
    const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-1);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ToastyTerrain),false,Character); Toasty::IgnorePlayer(Query,Pursuer);
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByChannel(Hits,Start,Start+Delta,FQuat::Identity,ECC_WorldStatic,Shape,Query);
    bool Blocked=false;
    for (const auto& Hit:Hits)
    {
        if (!Hit.bBlockingHit) continue;
        // A capsule already touching/inside a wall must be allowed to move out.
        // Never discard an initial contact that the requested move deepens.
        if (Hit.bStartPenetrating && Hit.GetComponent())
        {
            FMTDResult Before,After;
            const bool OverlapBefore=Hit.GetComponent()->ComputePenetration(Before,Shape,Start,FQuat::Identity);
            const bool OverlapAfter=Hit.GetComponent()->ComputePenetration(After,Shape,Start+Delta,FQuat::Identity);
            if (FVector::DotProduct(Delta,Hit.Normal)>=-KINDA_SMALL_NUMBER &&
                (!OverlapAfter || (OverlapBefore && After.Distance<Before.Distance-KINDA_SMALL_NUMBER))) continue;
        }
        if (!Blocked || Hit.Time<Obstacle.Time) Obstacle=Hit;
        Blocked=true;
    }
    return Blocked;
}
void UToastyBehaviorComponent::RecoverTerrainPenetration()
{
    // A saved placement or moving obstacle may leave the capsule inside a
    // wall. Resolve only a shallow lateral capsule overlap.
    // Commit only a supported, completely clear endpoint; no route teleport.
    const auto* Capsule=Character->GetCapsuleComponent();
    const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-1);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ToastyRecoverTerrain),false,Character); Toasty::IgnorePlayer(Query,Pursuer);
    const FVector Start=Character->GetActorLocation();
    FVector Candidate=Start;
    for (int32 Iteration=0;Iteration<4;++Iteration)
    {
        TArray<FOverlapResult> Hits;
        GetWorld()->OverlapMultiByChannel(Hits,Candidate,FQuat::Identity,ECC_WorldStatic,Shape,Query);
        FVector Correction=FVector::ZeroVector;
        for (const auto& Hit:Hits)
        {
            if (!Hit.bBlockingHit || !Hit.GetComponent()) continue;
            FMTDResult Contact;
            if (!Hit.GetComponent()->ComputePenetration(Contact,Shape,Candidate,FQuat::Identity) || FMath::Abs(Contact.Direction.Z)>.5f) continue;
            const FVector Horizontal=Contact.Direction.GetSafeNormal2D();
            Correction+=Horizontal*(Contact.Distance+2.f)/FMath::Max(.5f,FVector::DotProduct(Horizontal,Contact.Direction));
        }
        if (Correction.IsNearlyZero()) break;
        Candidate+=Correction;
        if (FVector::Dist2D(Start,Candidate)>Capsule->GetScaledCapsuleRadius()*.5f) return;
    }
    if (Candidate.Equals(Start)) return;
    FVector Ground;
    if (!GroundAt(Candidate,Ground) || FMath::Abs(Ground.Z-Start.Z)>600*WorldUnitsPerOriginalUnit ||
        GetWorld()->OverlapBlockingTestByChannel(Ground,FQuat::Identity,ECC_WorldStatic,Shape,Query)) return;
    FHitResult Obstacle;
    if (FindTerrainObstacle(Start,Ground-Start,Obstacle)) return;
    Character->SetActorLocation(Ground,false,nullptr,ETeleportType::TeleportPhysics);
    LastSafeGround=Ground;
}
FVector UToastyBehaviorComponent::SweepMove(const FVector& Delta,bool PlayerBlocks)
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ToastyMove),false,Character); Toasty::IgnorePlayer(Q,Pursuer,!PlayerBlocks);
    FHitResult Wall; const FVector Start=Character->GetActorLocation();
    float Fraction=FindTerrainObstacle(Start,Delta,Wall)?FMath::Max(0.f,Wall.Time-.002f):1.f;
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
        }
    }
    bBlocked=Fraction<.99f; Character->SetActorLocation(Start+Delta*Fraction,false,nullptr,ETeleportType::TeleportPhysics); return Delta*Fraction;
}
bool UToastyBehaviorComponent::GroundMove(const FVector& Delta)
{
    RecoverTerrainPenetration();
    ToastyEncounterCollision::LiftFromFloor(Character,Pursuer,WorldUnitsPerOriginalUnit);
    FVector Move;
    if (!ProjectGroundMove(Delta,Move)) { bBlocked=true; return false; }
    const FVector Actual=SweepMove(Move,true);
    if (Actual.SizeSquared()>1) LastSafeGround=Character->GetActorLocation(); return !bBlocked;
}
bool UToastyBehaviorComponent::ProjectGroundMove(const FVector& Delta,FVector& Move) const
{
    FVector Ground; const FVector Start=Character->GetActorLocation();
    if (!GroundAt(Start+Delta,Ground) || FMath::Abs(Ground.Z-Start.Z)>600*WorldUnitsPerOriginalUnit) return false;
    // GroundAt already settles the entire capsule. These additional ledge
    // checks are rays: fitting another full capsule at each edge doubled up
    // the footprint and prevented even movement away from nearby walls.
    const auto* Capsule=Character->GetCapsuleComponent();
    const float R=Capsule->GetScaledCapsuleRadius()*.7f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ToastyFootprint),false,Character); Toasty::IgnorePlayer(Query,Pursuer);
    FCollisionObjectQueryParams Types; Types.AddObjectTypesToQuery(ECC_WorldStatic); Types.AddObjectTypesToQuery(ECC_WorldDynamic);
    const float MaxStep=600*WorldUnitsPerOriginalUnit;
    for (const FVector& Offset:{FVector(R,0,0),FVector(-R,0,0),FVector(0,R,0),FVector(0,-R,0)})
    {
        const FVector Edge=Ground+Offset-FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());
        TArray<FHitResult> Hits;
        GetWorld()->LineTraceMultiByObjectType(Hits,Edge+FVector(0,0,MaxStep),Edge-FVector(0,0,MaxStep),Types,Query);
        bool Supported=false;
        for (const auto& Hit:Hits)
        {
            const auto* Surface=Hit.GetComponent();
            if (Surface && !Cast<APawn>(Hit.GetActor()) && Hit.ImpactNormal.Z>=.65f &&
                Surface->GetCollisionResponseToChannel(Capsule->GetCollisionObjectType())==ECR_Block)
            { Supported=true; break; }
        }
        if (!Supported) return false;
    }
    Move=Delta; Move.Z=Ground.Z-Start.Z;
    return true;
}
FVector UToastyBehaviorComponent::FindGroundDirection(const FVector& Preferred,float Step,const FVector* Center) const
{
    const FVector Start=Character->GetActorLocation();
    // Probe short supported paths before turning. The caller keeps a detour
    // briefly so each step does not turn straight back into the same wall.
    for (float Angle:{0.f,45.f,-45.f,90.f,-90.f,135.f,-135.f,180.f})
    {
        const FVector Direction=Preferred.RotateAngleAxis(Angle*OrbitDirection,FVector::UpVector);
        if (Center && FVector::Dist2D(Start+Direction*Step*2,*Center)>GuardAreaRadius) continue;
        FVector Move;
        if (!ProjectGroundMove(Direction*Step*2,Move)) continue;
        FHitResult Wall;
        if (!FindTerrainObstacle(Start,Move,Wall)) return Direction;
    }
    return FVector::ZeroVector;
}
void UToastyBehaviorComponent::OnChargeSensorOverlap(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool bSweep,const FHitResult& Hit)
{
    if (!bDefeated && Other && Other!=Character && OtherComponent && OtherComponent->GetCollisionObjectType()==ECC_GameTraceChannel4)
    { auto* Capsule=Character->GetCapsuleComponent(); Capsule->OnComponentBeginOverlap.Broadcast(Capsule,Other,OtherComponent,BodyIndex,bSweep,Hit); }
}
void UToastyBehaviorComponent::FinishCorpse()
{
    if (bCorpseFinished) return; bCorpseFinished=true; State=EToastyState::Dead; StopSounds();
    Character->SetActorHiddenInGame(true); Character->SetActorEnableCollision(false);
    if (auto* P=CastField<FMulticastDelegateProperty>(Toasty::Property(WalkingAI,TEXT("Corpse Poofed"))))
        if (auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(WalkingAI))) D->ProcessMulticastDelegate<UObject>(nullptr);
}
void UToastyBehaviorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    PendingEnemyEvents.Reset();
    StopSounds();
    if (Mesh) Mesh->UnregisterOnBoneTransformsFinalizedDelegate(BoneTransformsHandle);
    if (ChargeSensor) ChargeSensor->OnComponentBeginOverlap.RemoveDynamic(this,&UToastyBehaviorComponent::OnChargeSensorOverlap);
    Toasty::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(UToastyBehaviorComponent,OnAcceptedDamage),true);
    Toasty::Bind(Dropper,TEXT("Item Dropper Successfully Reset"),this,GET_FUNCTION_NAME_CHECKED(UToastyBehaviorComponent,OnDropperReset),true);
    Super::EndPlay(Reason);
}
void UToastyBehaviorComponent::DropGemRange(int32 First, int32 Count)
{
    if (Toasty::Bool(Dropper, TEXT("Cannot_Drop_Items"))) return;
    Toasty::Call(Dropper, TEXT("Remove Gems We Perma Collected"));
    Toasty::Call(Dropper, TEXT("Find All Items I Have But Shouldn't Drop"));
    auto* ItemsProperty = CastField<FArrayProperty>(Toasty::Property(Dropper, TEXT("Items_to_Drop")));
    auto* PendingProperty = CastField<FArrayProperty>(Toasty::Property(Dropper, TEXT("Items_I_Have_But_Shouldnt_Drop")));
    auto* SpawnedProperty = CastField<FArrayProperty>(Toasty::Property(Dropper, TEXT("Items_I_Dropped")));
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
        const FVector Location = GetOwner()->GetActorLocation() + FVector(0, 0, Toasty::Number(Dropper, TEXT("Item_Spawn_Height_Offset")));
        AActor* Gem = GetWorld()->SpawnActor<AActor>(GemClass, Location, GetOwner()->GetActorRotation(), Spawn);
        if (!Gem) continue;
        UFunction* Initialize = Gem->FindFunction(TEXT("Gem_Spawn_Process"));
        if (!Initialize) { Gem->Destroy(); continue; }
        FStructOnScope Params(Initialize);
        for (TFieldIterator<FProperty> It(Initialize); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            const FString Name = Toasty::Key(It->GetName());
            void* Value = It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
            if (auto* P = CastField<FObjectPropertyBase>(*It))
            {
                if (Name == TEXT("playerwhospawnedme")) P->SetObjectPropertyValue(Value, Toasty::ObjectValue(Damageable, TEXT("Deal Damage - Person Who Dealt the Damage")));
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


UToastyBehaviorComponent::UToastyBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PrePhysics;
    StageLocations={FVector::ZeroVector,FVector(900,0,0),FVector(1800,0,0)};
}
FVector UToastyBehaviorComponent::StagePosition(int32 Index) const
{ return Home.TransformPosition(StageLocations.IsValidIndex(Index)?StageLocations[Index]:FVector::ZeroVector); }
bool UToastyBehaviorComponent::HasLivingGuards() const
{
    for (const auto& G:Guards) if (G.Stage==Stage && IsValid(G.Dog))
        if (auto* D=G.Dog->FindComponentByClass<USleepingDogBehaviorComponent>()) if (!D->bDefeated) return true;
    return false;
}
FString UToastyBehaviorComponent::ValidateEncounter() const
{
    if (StageLocations.Num()!=3) return TEXT("Provide three local stage positions.");
    TSet<AActor*> Assigned;
    for (const auto& G:Guards)
    {
        if (!IsValid(G.Dog) || !G.Dog->FindComponentByClass<USleepingDogBehaviorComponent>()) return TEXT("Every guard must reference a Sleeping Dog actor.");
        if (G.Stage<0 || G.Stage>2 || Assigned.Contains(G.Dog)) return TEXT("Assign each dog once, to stage 0, 1 or 2.");
        Assigned.Add(G.Dog);
    }
    return FString();
}
void UToastyBehaviorComponent::BeginPlay()
{
    Super::BeginPlay(); Character=Cast<ACharacter>(GetOwner());
    if (!Character) { SetComponentTickEnabled(false); return; }
    Mesh=Character->GetMesh();
    for (auto* C:Character->GetComponents())
    {
        if (C->GetClass()->GetName()==TEXT("Damageable_Com_C")) Damageable=C;
        if (C->GetClass()->GetName()==TEXT("Walking_AI_Character_C")) WalkingAI=C;
        if (C->GetClass()->GetName()==TEXT("Drops_Items_C")) Dropper=C;
        if (C->GetFName()==TEXT("ToastyBodyCollision")) BodyCollision=Cast<UBoxComponent>(C);
        if (C->GetFName()==TEXT("ToastyChargeSensor")) ChargeSensor=Cast<UBoxComponent>(C);
        if (C->GetFName()==TEXT("Attack_Radius")) if (auto* A=Cast<UPrimitiveComponent>(C))
        { A->SetGenerateOverlapEvents(false); A->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    }
    bool ClipsOK=Animations.Num()==12;
    for (int32 I=0;I<Animations.Num();++I) if (I!=6 && !Animations[I]) ClipsOK=false;
    if (!Mesh || !Damageable || !WalkingAI || !Dropper || !BodyCollision || !ChargeSensor || !ClipsOK || Meshes.Num()!=4 || Meshes.Contains(nullptr) || !BurntCostumeMesh)
    { UE_LOG(LogTemp,Error,TEXT("Toasty %s has incomplete contracts/assets."),*Character->GetName()); SetComponentTickEnabled(false); return; }
    Home=Character->GetActorTransform(); Heading=PreviousHeading=Home.Rotator().Yaw;
    PreviousLocation=LastSafeGround=Home.GetLocation(); MeshOffset=Mesh->GetRelativeLocation(); Random.Initialize(GetTypeHash(Character->GetFName()));
    Mesh->AddTickPrerequisiteComponent(this);
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->bEnableUpdateRateOptimizations=false;
    ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
    BoneTransformsHandle=Mesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this,&UToastyBehaviorComponent::UpdateBodyCollision));
    Toasty::Bind(Damageable,TEXT("Call Deal_Damage"),WalkingAI,TEXT("On Damaged"),true);
    Toasty::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(UToastyBehaviorComponent,OnAcceptedDamage));
    Toasty::Bind(Dropper,TEXT("Item Dropper Successfully Reset"),this,GET_FUNCTION_NAME_CHECKED(UToastyBehaviorComponent,OnDropperReset));
    ChargeSensor->OnComponentBeginOverlap.AddUniqueDynamic(this,&UToastyBehaviorComponent::OnChargeSensorOverlap);
    Toasty::SetNumber(Damageable,TEXT("Hit Points"),3); Toasty::SetBool(WalkingAI,TEXT("Poofs_On_Death"),false);
    Toasty::SetNumber(WalkingAI,TEXT("Death Launch Upwards Force"),0); Toasty::SetNumber(WalkingAI,TEXT("Death Launch Forwards Force"),0);
    Toasty::SetNumber(Character,TEXT("Corpse Poof Delay"),100000.f);
    CostumeRemains=NewObject<UStaticMeshComponent>(Character,TEXT("BurntCostume")); CostumeRemains->SetStaticMesh(BurntCostumeMesh);
    CostumeRemains->SetCollisionEnabled(ECollisionEnabled::NoCollision); CostumeRemains->SetVisibility(false); CostumeRemains->RegisterComponent();
    TakeMovementControl(); SelectClip(0,false);
    const FString Warning=ValidateEncounter(); if (!Warning.IsEmpty()) UE_LOG(LogTemp,Warning,TEXT("%s: %s"),*Character->GetName(),*Warning);
}
void UToastyBehaviorComponent::EmitFrameSound()
{
    struct FCue { int32 Clip, Frame, Sound; };
    static const FCue Cues[]={{0,20,0},{0,26,0},{1,7,1},{1,9,2},{2,0,3},{3,8,4},{4,6,5},{4,19,6},{5,1,7},{7,16,5},{8,2,1},{8,7,2},{9,0,8},{9,11,9},{10,4,0}};
    for (const auto& C:Cues) if (C.Clip==CurrentClip && C.Frame==CurrentFrame) PlaySound(C.Sound);
}
void UToastyBehaviorComponent::LeaveCostume()
{
    if (!CostumeRemains) return;
    CostumeRemains->SetWorldLocation(Character->GetActorLocation()+FVector(0,0,-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    CostumeRemains->SetWorldRotation(FRotator(0,Heading-90,0)); CostumeRemains->SetWorldScale3D(FVector(.584416f)); CostumeRemains->SetVisibility(true);
}
void UToastyBehaviorComponent::BeginRetreat()
{
    State=EToastyState::Retreat; StateTicks=0; AvoidTicks=0; AvoidDirection=FVector::ZeroVector; SelectClip(Health==1?8:1);
}
void UToastyBehaviorComponent::StepOriginal()
{
    const bool LivingGuards = HasLivingGuards();
    if (ObservedGuardStage == Stage && bObservedLivingGuards && !LivingGuards)
        PendingEnemyEvents.Add(ESpyroEnemySignal::GuardsReleased, Stage);
    ObservedGuardStage = Stage;
    bObservedLivingGuards = LivingGuards;
    PreviousLocation=Character->GetActorLocation(); PreviousHeading=Heading; ++SimulationTicks; ++StateTicks;
    if (bFirstTick)
    {
        ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
        Toasty::Bind(Damageable,TEXT("Call Deal_Damage"),WalkingAI,TEXT("On Damaged"),true);
        Toasty::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(UToastyBehaviorComponent,OnAcceptedDamage)); bFirstTick=false;
    }
    TakeMovementControl(); if (!IsValid(Pursuer)) Pursuer=UGameplayStatics::GetPlayerCharacter(this,0);
    if (State==EToastyState::Dead) return;
    const bool Complete=AdvanceAnimation(); if (Cooldown>0) --Cooldown;
    const int32 Base=Health==1?7:0;
    if (State==EToastyState::Defeating)
    {
        if (Complete) { DropGemRange(2,1); PublishDefeat(); FinishCorpse(); } return;
    }
    if (State==EToastyState::HitReact)
    {
        if (Complete) BeginRetreat(); return;
    }
    if (State==EToastyState::Transforming)
    {
        if (Complete)
        {
            if (CurrentClip==5) { LeaveCostume(); SelectClip(7,false); }
            else BeginRetreat();
        }
        return;
    }
    if (State==EToastyState::Retreat)
    {
        const FVector Destination=StagePosition(Stage+1);
        if (Distance(Destination)<600)
        { Stage=FMath::Min(Stage+1,2); PendingEnemyEvents.Add(ESpyroEnemySignal::StageChanged, Stage); bEngaged=false; AvoidTicks=0; State=HasLivingGuards()?EToastyState::Guarded:EToastyState::Idle; StateTicks=0; SelectClip(Base); return; }
        RecoverTerrainPenetration();
        ToastyEncounterCollision::LiftFromFloor(Character,Pursuer,WorldUnitsPerOriginalUnit);
        const float Step=200*WorldUnitsPerOriginalUnit;
        const FVector Preferred=(Destination-Character->GetActorLocation()).GetSafeNormal2D();
        FVector Direction=Preferred;
        if (AvoidTicks>0) { --AvoidTicks; Direction=AvoidDirection; }
        else
        {
            Direction=FindGroundDirection(Preferred,Step);
            if (!Direction.IsNearlyZero() && !Direction.Equals(Preferred,.01f)) { AvoidDirection=Direction; AvoidTicks=20; PendingEnemyEvents.Add(ESpyroEnemySignal::RecoveryStarted, Stage); }
        }
        if (!Direction.IsNearlyZero() && Face(Character->GetActorLocation()+Direction*1000,9,20))
            if (!GroundMove(FRotator(0,Heading,0).Vector()*Step))
            { AvoidDirection=FindGroundDirection(Preferred,Step); AvoidTicks=AvoidDirection.IsNearlyZero()?0:20; }
        return; // Unsupported or enclosed routes still stop rather than teleporting.
    }
    if (State==EToastyState::Attack)
    {
        if ((CurrentClip==3 && CurrentFrame>=9) || (CurrentClip==10 && CurrentFrame>=5)) HitPlayer();
        if (Complete) { State=HasLivingGuards()?EToastyState::Guarded:EToastyState::Idle; SelectClip(Base); StateTicks=0; }
        return;
    }
    if (HasLivingGuards())
    {
        State=EToastyState::Guarded;
        if (!bEngaged)
        {
            SelectClip(Base);
            if (!IsValid(Pursuer) || FVector::Dist2D(Character->GetActorLocation(),Pursuer->GetActorLocation())>ApproachDistance ||
                FMath::Abs(Feet(Character).Z-Feet(Pursuer).Z)>800*WorldUnitsPerOriginalUnit) return;
            FHitResult Wall; FCollisionQueryParams Q(SCENE_QUERY_STAT(ToastyApproach),false,Character); Toasty::IgnorePlayer(Q,Pursuer);
            if (GetWorld()->LineTraceSingleByChannel(Wall,Character->GetActorLocation(),Pursuer->GetActorLocation(),ECC_Visibility,Q)) return;
            bEngaged=true;
        }
        if (IsValid(Pursuer))
        {
            const FVector Center=StagePosition(Stage);
            const FVector Radial=(Character->GetActorLocation()-Center).GetSafeNormal2D();
            FVector Direction=(Character->GetActorLocation()-Pursuer->GetActorLocation()).GetSafeNormal2D();
            const float Radius=FVector::Dist2D(Character->GetActorLocation(),Center);
            // Continue running around the guard area instead of stopping at an
            // escape point. The outer band turns fleeing into an inward orbit.
            if (Radius>GuardAreaRadius*.65f)
                Direction=(FVector(-Radial.Y,Radial.X,0)*OrbitDirection-Radial*.65f).GetSafeNormal2D();
            RecoverTerrainPenetration();
            ToastyEncounterCollision::LiftFromFloor(Character,Pursuer,WorldUnitsPerOriginalUnit);
            if (AvoidTicks>0) { --AvoidTicks; Direction=AvoidDirection; }
            else
            {
                const FVector Safe=FindGroundDirection(Direction,(180+Stage*20)*WorldUnitsPerOriginalUnit,&Center);
                if (!Safe.IsNearlyZero() && !Safe.Equals(Direction,.01f)) { AvoidDirection=Safe; AvoidTicks=20; Direction=Safe; PendingEnemyEvents.Add(ESpyroEnemySignal::RecoveryStarted, Stage); }
            }
            SelectClip(Base+1);
            if (Face(Character->GetActorLocation()+Direction*1000,10,24))
            {
                const float Step=(180+Stage*20)*WorldUnitsPerOriginalUnit;
                const FVector Move=FRotator(0,Heading,0).Vector()*Step;
                if (FVector::Dist2D(Character->GetActorLocation()+Move,Center)<=GuardAreaRadius)
                {
                    if (!GroundMove(Move)) { AvoidDirection=FindGroundDirection(Direction,(180+Stage*20)*WorldUnitsPerOriginalUnit,&Center); AvoidTicks=20; }
                }
                else { AvoidDirection=FindGroundDirection(-Radial,(180+Stage*20)*WorldUnitsPerOriginalUnit,&Center); AvoidTicks=20; }
            }
        }
        return;
    }
    if (State==EToastyState::Guarded) { SelectClip(Base); StateTicks=0; }
    State=EToastyState::Idle;
    const bool FacingPlayer=IsValid(Pursuer) && Face(Pursuer->GetActorLocation(),6,20);
    if (Complete && Health>1)
    {
        if (CurrentClip==4) SelectClip(0,false);
        else if (--TauntLoops<=0) { TauntLoops=Random.RandRange(2,3); SelectClip(4,false); }
    }
    if (Cooldown==0 && FacingPlayer && Distance(Pursuer->GetActorLocation())<(Health==1?2400:3000) && FMath::Abs(Feet(Character).Z-Feet(Pursuer).Z)<800*WorldUnitsPerOriginalUnit)
    {
        FHitResult Wall; FCollisionQueryParams Q(SCENE_QUERY_STAT(ToastySight),false,Character); Toasty::IgnorePlayer(Q,Pursuer);
        if (!GetWorld()->LineTraceSingleByChannel(Wall,Character->GetActorLocation(),Pursuer->GetActorLocation(),ECC_Visibility,Q))
        { State=EToastyState::Attack; StateTicks=0; Cooldown=90; bHitThisAttack=false; SelectClip(Base+3); PendingEnemyEvents.Add(ESpyroEnemySignal::AttackCommitted, Base+3); }
    }
}
void UToastyBehaviorComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction); if (!Character || !Character->HasAuthority()) return;
    if (Toasty::Bool(Damageable,TEXT("Frozen")) || Toasty::Bool(Damageable,TEXT("Paralyzed_by_Fear")) || Toasty::Bool(Dropper,TEXT("Reset_in_Progress"))) { PendingEnemyEvents.Flush(this, OnEnemySignal); return; }
    Accumulator+=FMath::Max(0.f,DeltaTime); int32 Steps=0;
    while (Accumulator+KINDA_SMALL_NUMBER>=ToastyStep && Steps++<30) { Accumulator=FMath::Max(0.f,Accumulator-ToastyStep); StepOriginal(); }
    Accumulator=FMath::Min(Accumulator,ToastyStep); const float Alpha=Accumulator/ToastyStep;
    Mesh->SetWorldLocation(FMath::Lerp(PreviousLocation,Character->GetActorLocation(),Alpha)+Home.TransformVector(MeshOffset));
    Mesh->SetWorldRotation(FRotator(0,PreviousHeading+FMath::FindDeltaAngleDegrees(PreviousHeading,Heading)*Alpha-90,0));
    PendingEnemyEvents.Flush(this, OnEnemySignal);
}
void UToastyBehaviorComponent::HitPlayer()
{
    if (bHitThisAttack || !IsValid(Pursuer)) return;
    const FVector Offset=Feet(Pursuer)-Feet(Character);
    if (ToastyDistance(Offset)>(Health==1?2400:3000)*WorldUnitsPerOriginalUnit || FMath::Abs(Offset.Z)>120 || FMath::Abs(FMath::FindDeltaAngleDegrees(Heading,Offset.Rotation().Yaw))>65) return;
    if (!SpyroMeleeContact::HasClearContact(Character, Pursuer)) return;
    UFunction* F=Pursuer->FindFunction(TEXT("Deal Damage to Player")); if (!F) return;
    FStructOnScope Params(F); UObject* D=Toasty::ObjectValue(Pursuer,TEXT("Damageable")); const int32 Before=Toasty::Number(D,TEXT("Hit Points"));
    for (TFieldIterator<FProperty> It(F);It && It->HasAnyPropertyFlags(CPF_Parm);++It)
    {
        void* V=It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
        if (auto* P=CastField<FByteProperty>(*It)) P->SetPropertyValue(V,1);
        if (auto* P=CastField<FStructProperty>(*It)) if (P->Struct==TBaseStructure<FVector>::Get()) *static_cast<FVector*>(V)=Offset.GetSafeNormal2D();
        if (auto* P=CastField<FObjectPropertyBase>(*It)) P->SetObjectPropertyValue(V,Character);
    }
    bHitThisAttack=true; Pursuer->ProcessEvent(F,Params.GetStructMemory());
    if (Toasty::Number(D,TEXT("Hit Points"))<Before) ++AcceptedPlayerHits;
}
void UToastyBehaviorComponent::OnAcceptedDamage()
{
    if (!Character->HasAuthority() || bDefeated || State==EToastyState::HitReact || State==EToastyState::Transforming || State==EToastyState::Retreat || State==EToastyState::Defeating ||
        Toasty::Bool(Damageable,TEXT("Invincible")) || Toasty::Bool(Damageable,TEXT("Frozen")) || Toasty::Bool(Dropper,TEXT("Reset_in_Progress"))) return;
    if (Toasty::Number(Damageable,TEXT("Deal Damage - Damage Type"))!=2) return;
    --Health; Toasty::SetNumber(Damageable,TEXT("Hit Points"),Health); StateTicks=0; bHitThisAttack=true;
    if (Health==2) { DropGemRange(0,1); State=EToastyState::HitReact; SelectClip(2,false); }
    else if (Health==1) { DropGemRange(1,1); State=EToastyState::Transforming; SelectClip(5,false); }
    else { bDefeated=true; State=EToastyState::Defeating; SelectClip(9,false); BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision); ChargeSensor->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
}
void UToastyBehaviorComponent::PublishDefeat()
{
    const bool Suppressed=Toasty::Bool(Dropper,TEXT("Cannot_Drop_Items")); Toasty::SetBool(Dropper,TEXT("Cannot_Drop_Items"),true);
    if (auto* P=CastField<FMulticastDelegateProperty>(Toasty::Property(Damageable,TEXT("Damage Was Successfully Dealt"))))
        if (auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(Damageable))) D->ProcessMulticastDelegate<UObject>(nullptr);
    Toasty::SetBool(Dropper,TEXT("Cannot_Drop_Items"),Suppressed); TakeMovementControl();
}
void UToastyBehaviorComponent::OnDropperReset()
{
    PendingEnemyEvents.Reset();
    ObservedGuardStage=INDEX_NONE; bObservedLivingGuards=false;
    // The shared Blueprint removes array elements while iterating forwards.
    // With staged rewards this can leave a live middle gem behind. Finish its
    // own reset contract before allowing the same reward indices to drop again.
    if (auto* P=CastField<FArrayProperty>(Toasty::Property(Dropper,TEXT("Items_I_Dropped"))))
    {
        if (auto* Inner=CastField<FObjectPropertyBase>(P->Inner))
        {
            FScriptArrayHelper Items(P,P->ContainerPtrToValuePtr<void>(Dropper));
            TArray<TWeakObjectPtr<AActor>> Remaining;
            for (int32 I=0;I<Items.Num();++I)
                Remaining.Add(Cast<AActor>(Inner->GetObjectPropertyValue(Items.GetRawPtr(I))));
            Items.EmptyValues();
            for (const auto& Gem:Remaining)
                if (Gem.IsValid()) Toasty::Call(Gem.Get(),TEXT("Spawned Gem Reset"));
        }
    }
    StopSounds(); SoundCueHistory.Reset(); ReleasedGemIndices.Reset(); GemsSpawned=0; Health=3; Stage=0;
    State=EToastyState::Guarded; StateTicks=Cooldown=0; bDefeated=bHitThisAttack=bCorpseFinished=bBlocked=bEngaged=false;
    Accumulator=0; AcceptedPlayerHits=0; TauntLoops=2; AvoidTicks=0; AvoidDirection=FVector::ZeroVector; OrbitDirection=1;
    Character->SetActorTransform(Home,false,nullptr,ETeleportType::TeleportPhysics); PreviousLocation=LastSafeGround=Home.GetLocation(); Heading=PreviousHeading=Home.Rotator().Yaw;
    Character->SetActorHiddenInGame(false); Character->SetActorEnableCollision(true);
    if (CostumeRemains) CostumeRemains->SetVisibility(false);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); ChargeSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
    Toasty::SetNumber(Damageable,TEXT("Hit Points"),3); TakeMovementControl(); SelectClip(0,false);
    PendingEnemyEvents.Add(ESpyroEnemySignal::ResetCompleted, Stage);
}
