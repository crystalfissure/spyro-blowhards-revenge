#include "SM64EditorReferenceLibrary.h"

#include "AssetToolsModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "StaticMeshResources.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
class FSM64CountObjectReferences final : public FArchiveUObject
{
public:
    explicit FSM64CountObjectReferences(UObject* InTarget)
        : Target(InTarget)
    {
        ArIsObjectReferenceCollector = true;
        ArIgnoreArchetypeRef = false;
        ArIgnoreOuterRef = true;
    }

    virtual FArchive& operator<<(UObject*& Object) override
    {
        if (Object == Target)
        {
            ++Count;
        }
        return *this;
    }

    UObject* Target = nullptr;
    int64 Count = 0;
};
}

int64 USM64EditorReferenceLibrary::ReplaceHardReferencesInPackage(
    const FString& PackageName,
    const FString& OldObjectPath,
    const FString& NewObjectPath)
{
    UPackage* Package = FindPackage(nullptr, *PackageName);
    if (!Package)
    {
        Package = LoadPackage(nullptr, *PackageName, LOAD_None);
    }

    UObject* OldObject = LoadObject<UObject>(nullptr, *OldObjectPath);
    UObject* NewObject = LoadObject<UObject>(nullptr, *NewObjectPath);
    if (!Package || !OldObject || !NewObject || OldObject == NewObject)
    {
        UE_LOG(LogTemp, Error,
            TEXT("SM64 reference replacement rejected: package=%s old=%s new=%s"),
            *PackageName, *OldObjectPath, *NewObjectPath);
        return -1;
    }

    TMap<UObject*, UObject*> Replacements;
    Replacements.Add(OldObject, NewObject);
    // UPackage does not serialize ownership links to every contained export. Visit each
    // export explicitly so LevelScriptBlueprint graphs, pins, and generated classes are
    // included in the exact-object replacement pass.
    TArray<UObject*> ObjectsInPackage;
    GetObjectsWithPackage(Package, ObjectsInPackage, true);
    int64 Count = 0;
    for (UObject* Object : ObjectsInPackage)
    {
        if (!Object || Object->IsPendingKill())
        {
            continue;
        }
        FArchiveReplaceObjectRef<UObject> ReplaceArchive(
            Object,
            Replacements,
            false,
            true,
            true,
            false,
            false);
        Count += ReplaceArchive.GetCount();
    }
    if (Count > 0)
    {
        Package->MarkPackageDirty();
    }

    UE_LOG(LogTemp, Display,
        TEXT("SM64 hard reference replacement: package=%s old=%s new=%s count=%lld"),
        *PackageName, *OldObjectPath, *NewObjectPath, Count);
    return Count;
}

int32 USM64EditorReferenceLibrary::ReplaceSoftReferencesInPackage(
    const FString& PackageName,
    const FString& OldObjectPath,
    const FString& NewObjectPath)
{
    UPackage* Package = FindPackage(nullptr, *PackageName);
    if (!Package)
    {
        Package = LoadPackage(nullptr, *PackageName, LOAD_None);
    }
    if (!Package || OldObjectPath.IsEmpty() || NewObjectPath.IsEmpty() || OldObjectPath == NewObjectPath)
    {
        UE_LOG(LogTemp, Error,
            TEXT("SM64 soft reference replacement rejected: package=%s old=%s new=%s"),
            *PackageName, *OldObjectPath, *NewObjectPath);
        return -1;
    }

    const FSoftObjectPath OldPath(OldObjectPath);
    const FSoftObjectPath NewPath(NewObjectPath);
    TMap<FSoftObjectPath, FSoftObjectPath> RedirectMap;
    RedirectMap.Add(OldPath, NewPath);

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(
        TEXT("AssetTools")).Get();
    const bool bWasDirty = Package->IsDirty();
    TArray<UPackage*> Packages;
    Packages.Add(Package);
    AssetTools.RenameReferencingSoftObjectPaths(Packages, RedirectMap);

    // IAssetTools exposes the rewrite but not its private per-package match count.
    // A clean package is marked dirty only when the serializer changes a path.
    const int32 Count = (!bWasDirty && Package->IsDirty()) ? 1 : 0;
    UE_LOG(LogTemp, Display,
        TEXT("SM64 soft reference replacement: package=%s old=%s new=%s objects=%d"),
        *PackageName, *OldObjectPath, *NewObjectPath, Count);
    return Count;
}

int64 USM64EditorReferenceLibrary::CountHardReferencesInPackage(
    const FString& PackageName,
    const FString& ObjectPath)
{
    UPackage* Package = FindPackage(nullptr, *PackageName);
    if (!Package)
    {
        Package = LoadPackage(nullptr, *PackageName, LOAD_None);
    }
    UObject* Target = LoadObject<UObject>(nullptr, *ObjectPath);
    if (!Package || !Target)
    {
        return -1;
    }

    TArray<UObject*> ObjectsInPackage;
    GetObjectsWithPackage(Package, ObjectsInPackage, true);
    FSM64CountObjectReferences Counter(Target);
    for (UObject* Object : ObjectsInPackage)
    {
        if (Object && !Object->IsPendingKill())
        {
            Object->Serialize(Counter);
        }
    }
    return Counter.Count;
}

int32 USM64EditorReferenceLibrary::BuildSimpleConvexCollisionFromSource(
    UStaticMesh* StaticMesh,
    float CoplanarThickness)
{
    FStaticMeshRenderData* RenderData = StaticMesh ? StaticMesh->GetRenderData() : nullptr;
    if (!StaticMesh || !RenderData || RenderData->LODResources.Num() == 0)
    {
        return -1;
    }

    StaticMesh->Modify();
    StaticMesh->CreateBodySetup();
    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    if (!BodySetup)
    {
        return -1;
    }
    BodySetup->Modify();
    BodySetup->RemoveSimpleCollision();
    BodySetup->CollisionTraceFlag = CTF_UseDefault;

    const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
    const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
    const FRawStaticIndexBuffer& Indices = LOD.IndexBuffer;
    const float HalfThickness = FMath::Max(0.1f, CoplanarThickness * 0.5f);

    for (const FStaticMeshSection& Section : LOD.Sections)
    {
        TSet<uint32> UniqueIndices;
        const uint32 EndIndex = Section.FirstIndex + Section.NumTriangles * 3;
        for (uint32 Index = Section.FirstIndex; Index < EndIndex; ++Index)
        {
            UniqueIndices.Add(Indices.GetIndex(Index));
        }
        if (UniqueIndices.Num() < 3)
        {
            continue;
        }

        TArray<FVector> Vertices;
        Vertices.Reserve(UniqueIndices.Num() * 2);
        FBox Bounds(ForceInit);
        for (uint32 VertexIndex : UniqueIndices)
        {
            if (VertexIndex < Positions.GetNumVertices())
            {
                const FVector Position = Positions.VertexPosition(VertexIndex);
                Vertices.Add(Position);
                Bounds += Position;
            }
        }
        if (Vertices.Num() < 3)
        {
            continue;
        }

        const FVector Size = Bounds.GetSize();
        int32 ThinAxis = 0;
        if (Size.Y < Size.X)
        {
            ThinAxis = 1;
        }
        if (Size.Z < Size[ThinAxis])
        {
            ThinAxis = 2;
        }
        if (Size[ThinAxis] < CoplanarThickness)
        {
            const int32 OriginalCount = Vertices.Num();
            for (int32 VertexIndex = 0; VertexIndex < OriginalCount; ++VertexIndex)
            {
                FVector Negative = Vertices[VertexIndex];
                FVector Positive = Vertices[VertexIndex];
                Negative[ThinAxis] -= HalfThickness;
                Positive[ThinAxis] += HalfThickness;
                Vertices[VertexIndex] = Negative;
                Vertices.Add(Positive);
            }
        }

        FKConvexElem Convex;
        Convex.VertexData = MoveTemp(Vertices);
        Convex.UpdateElemBox();
        BodySetup->AggGeom.ConvexElems.Add(MoveTemp(Convex));
    }

    BodySetup->InvalidatePhysicsData();
    BodySetup->CreatePhysicsMeshes();
    StaticMesh->MarkPackageDirty();
    return BodySetup->AggGeom.ConvexElems.Num();
}

int32 USM64EditorReferenceLibrary::GetConvexCollisionHullCount(UStaticMesh* StaticMesh)
{
    const UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
    return BodySetup ? BodySetup->AggGeom.ConvexElems.Num() : -1;
}

int32 USM64EditorReferenceLibrary::ConfigureComplexAsSimpleCollision(UStaticMesh* StaticMesh)
{
    FStaticMeshRenderData* RenderData = StaticMesh ? StaticMesh->GetRenderData() : nullptr;
    if (!StaticMesh || !RenderData || RenderData->LODResources.Num() == 0)
    {
        return -1;
    }
    StaticMesh->Modify();
    StaticMesh->CreateBodySetup();
    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    if (!BodySetup)
    {
        return -1;
    }
    BodySetup->Modify();
    BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
    const int32 SectionCount = RenderData->LODResources[0].Sections.Num();
    FMeshSectionInfoMap& SectionInfoMap = StaticMesh->GetSectionInfoMap();
    FMeshSectionInfoMap& OriginalSectionInfoMap = StaticMesh->GetOriginalSectionInfoMap();
    for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
    {
        FMeshSectionInfo Info = SectionInfoMap.Get(0, SectionIndex);
        Info.bEnableCollision = true;
        SectionInfoMap.Set(0, SectionIndex, Info);
        OriginalSectionInfoMap.Set(0, SectionIndex, Info);
    }
    BodySetup->InvalidatePhysicsData();
    BodySetup->CreatePhysicsMeshes();
    StaticMesh->MarkPackageDirty();
    return SectionCount;
}

AActor* USM64EditorReferenceLibrary::SpawnActorInEditor(
    TSubclassOf<AActor> ActorClass,
    const FTransform& Transform)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || !*ActorClass || !World->GetCurrentLevel())
    {
        return nullptr;
    }

    FActorSpawnParameters Parameters;
    Parameters.OverrideLevel = World->GetCurrentLevel();
    Parameters.ObjectFlags |= RF_Transactional;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor = World->SpawnActor<AActor>(*ActorClass, Transform, Parameters);
    if (Actor)
    {
        Actor->Modify();
        World->GetCurrentLevel()->MarkPackageDirty();
    }
    return Actor;
}

bool USM64EditorReferenceLibrary::EnsureNameIntMapVariable(
    UBlueprint* Blueprint,
    FName VariableName)
{
    if (!Blueprint || VariableName.IsNone())
    {
        return false;
    }

    if (const FProperty* Existing = FindFProperty<FProperty>(
        Blueprint->GeneratedClass, VariableName))
    {
        const FMapProperty* Map = CastField<FMapProperty>(Existing);
        return Map && CastField<FNameProperty>(Map->KeyProp)
            && CastField<FIntProperty>(Map->ValueProp);
    }

    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    PinType.ContainerType = EPinContainerType::Map;
    PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Int;
    Blueprint->Modify();
    if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableName, PinType))
    {
        return false;
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Blueprint->MarkPackageDirty();

    const FMapProperty* Added = FindFProperty<FMapProperty>(
        Blueprint->GeneratedClass, VariableName);
    return Added && CastField<FNameProperty>(Added->KeyProp)
        && CastField<FIntProperty>(Added->ValueProp);
}
