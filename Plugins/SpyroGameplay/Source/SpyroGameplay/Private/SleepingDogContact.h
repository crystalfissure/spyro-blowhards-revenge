#pragma once

#include "CoreMinimal.h"
#include <cfloat>

// Toasty class335 model data, verified in both supplied NTSC save states.
// This geometry is independent of the broad rendered-body/flame bounds.
namespace SleepingDogContact
{
inline int32 Group(int32 Clip, int32 NextClip, int32 Frame, int32 NextFrame, int32 Progress)
{
    // func_800529E4 selects the next frame at ANY positive interpolation progress.
    const int32 SelectedClip = Progress > 0 ? NextClip : Clip;
    const int32 SelectedFrame = Progress > 0 ? NextFrame : Frame;
    if (SelectedClip != 2 && SelectedClip != 9) return 0;
    return SelectedFrame >= 10 && SelectedFrame <= 18 ? 1 : 2;
}

struct FInterval
{
    double Low = 0.0;
    double High = 1.0;
};

inline bool SphereInterval(const FVector& Start, const FVector& End,
    const FVector& Center, double Radius, bool Planar, FInterval& Interval)
{
    const FVector Offset = Start - Center;
    const FVector Delta = End - Start;
    const double A = double(Delta.X) * Delta.X + double(Delta.Y) * Delta.Y + (Planar ? 0.0 : double(Delta.Z) * Delta.Z);
    const double B = 2.0 * (double(Offset.X) * Delta.X + double(Offset.Y) * Delta.Y + (Planar ? 0.0 : double(Offset.Z) * Delta.Z));
    const double C = double(Offset.X) * Offset.X + double(Offset.Y) * Offset.Y + (Planar ? 0.0 : double(Offset.Z) * Offset.Z) - Radius * Radius;
    if (A < 1.e-12) return C < 0.0;
    const double Discriminant = B * B - 4.0 * A * C;
    if (Discriminant <= 0.0) return false; // Original contact excludes tangency.
    const double Root = FMath::Sqrt(Discriminant);
    Interval.Low = FMath::Max(Interval.Low, (-B - Root) / (2.0 * A));
    Interval.High = FMath::Min(Interval.High, (-B + Root) / (2.0 * A));
    return Interval.Low < Interval.High;
}

inline bool ZInterval(double Start, double End, double Min, double Max, FInterval& Interval)
{
    const double Delta = End - Start;
    if (FMath::Abs(Delta) < 1.e-12) return Start >= Min && Start <= Max;
    double Low = (Min - Start) / Delta;
    double High = (Max - Start) / Delta;
    if (Low > High) Swap(Low, High);
    Interval.Low = FMath::Max(Interval.Low, Low);
    Interval.High = FMath::Min(Interval.High, High);
    return Interval.Low < Interval.High;
}

inline bool CrushSweep(const FVector& RelativeStart, const FVector& RelativeEnd,
    float DogUnits, float PlayerRadius)
{
    if (DogUnits <= SMALL_NUMBER || PlayerRadius <= SMALL_NUMBER) return false;
    FInterval Bounds;
    if (!SphereInterval(RelativeStart, RelativeEnd, FVector::ZeroVector,
            962.0 * DogUnits + PlayerRadius, false, Bounds) ||
        !SphereInterval(RelativeStart, RelativeEnd, FVector::ZeroVector,
            550.0 * DogUnits + PlayerRadius, true, Bounds)) return false;
    FInterval Core = Bounds;
    if (ZInterval(RelativeStart.Z, RelativeEnd.Z, 0.0, 700.0 * DogUnits, Core)) return true;
    FInterval Lower = Bounds;
    if (ZInterval(RelativeStart.Z, RelativeEnd.Z, -DBL_MAX, 0.0, Lower) &&
        SphereInterval(RelativeStart, RelativeEnd, FVector(0, 0, 8173.f * DogUnits),
            8192.0 * DogUnits + PlayerRadius, false, Lower)) return true;
    FInterval Upper = Bounds;
    return ZInterval(RelativeStart.Z, RelativeEnd.Z, 700.0 * DogUnits, DBL_MAX, Upper) &&
        SphereInterval(RelativeStart, RelativeEnd, FVector(0, 0, -7473.f * DogUnits),
            8192.0 * DogUnits + PlayerRadius, false, Upper);
}

inline bool CrushTouches(const FVector& RelativePosition, float DogUnits, float PlayerRadius)
{
    return CrushSweep(RelativePosition, RelativePosition, DogUnits, PlayerRadius);
}
}
