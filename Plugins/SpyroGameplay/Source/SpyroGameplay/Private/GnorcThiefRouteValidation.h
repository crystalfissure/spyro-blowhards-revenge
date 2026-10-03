#pragma once
#include "CoreMinimal.h"

namespace GnorcThiefRouteValidation
{
inline FString Error(const TArray<FVector>& Points, float Units)
{
    if (!FMath::IsFinite(Units) || Units <= 0.f || !FMath::IsFinite(1.f / Units))
        return TEXT("World Units Per Original Unit must be finite and greater than zero.");
    if (Points.Num() < 2) return TEXT("Route Points must contain at least two points.");
    for (int32 I = 0; I < Points.Num(); ++I)
    {
        const FVector Scaled = Points[I] * Units;
        if (Points[I].ContainsNaN() || Scaled.ContainsNaN() ||
            !FMath::IsFinite(Points[I].SizeSquared()) || !FMath::IsFinite(Scaled.SizeSquared()))
            return FString::Printf(TEXT("Route point %d must have finite, usable coordinates."), I);
    }
    return FString();
}
}
