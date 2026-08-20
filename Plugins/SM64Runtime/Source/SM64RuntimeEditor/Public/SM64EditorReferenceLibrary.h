#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SM64EditorReferenceLibrary.generated.h"

class UStaticMesh;
class AActor;
class UBlueprint;

/** Exact, editor-only package reference operations used by the idempotent course pipeline. */
UCLASS()
class SM64RUNTIMEEDITOR_API USM64EditorReferenceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Replaces hard UObject references beneath a loaded package. The package is marked dirty
     * when a replacement occurs; the caller remains responsible for saving it.
     */
    UFUNCTION(BlueprintCallable, Category = "SM64|Editor")
    static int64 ReplaceHardReferencesInPackage(
        const FString& PackageName,
        const FString& OldObjectPath,
        const FString& NewObjectPath);

    /** Rewrites serialized FSoftObjectPath/FSoftClassPath values beneath a package. */
    UFUNCTION(BlueprintCallable, Category = "SM64|Editor")
    static int32 ReplaceSoftReferencesInPackage(
        const FString& PackageName,
        const FString& OldObjectPath,
        const FString& NewObjectPath);

    /** Counts exact hard UObject references without changing the package. */
    UFUNCTION(BlueprintPure, Category = "SM64|Editor")
    static int64 CountHardReferencesInPackage(
        const FString& PackageName,
        const FString& ObjectPath);

    /**
     * Builds one convex hull per source mesh section from the collision mesh's
     * own render vertices. Coplanar SM64 collision sheets receive a very small
     * thickness so PhysX can cook them as movable simple collision.
     * Returns the number of generated hulls.
     */
    UFUNCTION(BlueprintCallable, Category = "SM64|Editor|Collision")
    static int32 BuildSimpleConvexCollisionFromSource(
        UStaticMesh* StaticMesh,
        float CoplanarThickness = 2.0f);

    /** Reads the serialized convex-hull count directly from the mesh BodySetup. */
    UFUNCTION(BlueprintPure, Category = "SM64|Editor|Collision")
    static int32 GetConvexCollisionHullCount(UStaticMesh* StaticMesh);

    /** Enables every LOD0 section and cooks the source triangles as static complex collision. */
    UFUNCTION(BlueprintCallable, Category = "SM64|Editor|Collision")
    static int32 ConfigureComplexAsSimpleCollision(UStaticMesh* StaticMesh);

    /** Stable commandlet-safe alternative to EditorLevelLibrary's actor spawner. */
    UFUNCTION(BlueprintCallable, Category = "SM64|Editor|Level")
    static AActor* SpawnActorInEditor(
        TSubclassOf<AActor> ActorClass,
        const FTransform& Transform);

    /**
     * Idempotently adds a Blueprint SaveGame variable with type TMap<FName, int32>.
     * Existing variables are accepted only when their reflected type matches exactly.
     */
    UFUNCTION(BlueprintCallable, Category = "SM64|Editor|Save")
    static bool EnsureNameIntMapVariable(UBlueprint* Blueprint, FName VariableName);
};
