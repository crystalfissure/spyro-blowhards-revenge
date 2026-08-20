#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "SM64ActActor.h"
#include "SM64StaticCourseActor.generated.h"

/**
 * A reusable course-art wrapper that deliberately separates visible SM64 art
 * from its authoritative decomp collision mesh.  It also participates in the
 * normal course act-mask lifecycle through ASM64ActActor.
 */
UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64StaticCourseActor : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64StaticCourseActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* VisualMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* CollisionMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* DefaultCollisionMesh = nullptr;
};
