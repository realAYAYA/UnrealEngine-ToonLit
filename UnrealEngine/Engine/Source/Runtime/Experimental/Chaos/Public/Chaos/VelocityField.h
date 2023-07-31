// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/TriangleMesh.h"

namespace Chaos::Softs
{

// Velocity field used solely for aerodynamics effects, use Chaos Fields for other types of fields.
class CHAOS_API FVelocityAndPressureField final
{
public:
	static constexpr FSolverReal DefaultDragCoefficient = (FSolverReal)0.5;
	static constexpr FSolverReal DefaultLiftCoefficient = (FSolverReal)0.1;
	static constexpr FSolverReal DefaultFluidDensity = (FSolverReal)1.225e-6;

	// Construct an uninitialized field. Mesh, properties, and velocity will have to be set for this field to be valid.
	FVelocityAndPressureField()
		: Offset(INDEX_NONE)
		, NumParticles(0)
	{
		SetProperties(FSolverVec2(0.), FSolverVec2(0.), (FSolverReal)0.);
	}

	~FVelocityAndPressureField() {}

	void UpdateForces(const FSolverParticles& InParticles, const FSolverReal /*Dt*/);

	inline void Apply(FSolverParticles& InParticles, const FSolverReal Dt, const int32 Index) const
	{
		checkSlow(Index >= Offset && Index < Offset + NumParticles);  // The index should always match the original triangle mesh range

		const TArray<int32>& ElementIndices = PointToTriangleMap[Index];
		for (const int32 ElementIndex : ElementIndices)
		{
			InParticles.Acceleration(Index) += InParticles.InvM(Index) * Forces[ElementIndex];
		}
	}

	void SetProperties(const FSolverVec2& Drag, const FSolverVec2& Lift, const FSolverReal FluidDensity, const FSolverVec2& Pressure = FSolverVec2::ZeroVector)
	{
		constexpr FSolverReal OneQuarter = (FSolverReal)0.25;
		QuarterRho = FluidDensity * OneQuarter;

		constexpr FSolverReal MinCoefficient = (FSolverReal)0.;
		constexpr FSolverReal MaxCoefficient = (FSolverReal)10.;
		DragBase = FMath::Clamp(Drag[0], MinCoefficient, MaxCoefficient);
		DragRange = FMath::Clamp(Drag[1], MinCoefficient, MaxCoefficient) - DragBase;
		LiftBase = FMath::Clamp(Lift[0], MinCoefficient, MaxCoefficient);
		LiftRange = FMath::Clamp(Lift[1], MinCoefficient, MaxCoefficient) - LiftBase;
		PressureBase = Pressure[0];
		PressureRange = Pressure[1] - PressureBase;
	}

	bool IsActive() const 
	{ 
		// Note: range can be a negative value (although not when Lift or Drag base is zero)
		return (DragBase > (FSolverReal)0. || DragRange != (FSolverReal)0.) || (LiftBase > (FSolverReal)0. || LiftRange != (FSolverReal)0.) || PressureBase != (FSolverReal)0. || PressureRange != (FSolverReal)0.; 
	}

	void SetGeometry(const FTriangleMesh* TriangleMesh, const TConstArrayView<FRealSingle>& DragMultipliers, const TConstArrayView<FRealSingle>& LiftMultipliers, const TConstArrayView<FRealSingle>& PressureMultipliers = TConstArrayView<FRealSingle>());

	void SetVelocity(const FSolverVec3& InVelocity) { Velocity = InVelocity; }

	const TConstArrayView<TVector<int32, 3>>& GetElements() const { return Elements; }
	TConstArrayView<FSolverVec3> GetForces() const { return TConstArrayView<FSolverVec3>(Forces); }

private:
	void UpdateField(const FSolverParticles& InParticles, int32 ElementIndex, const FSolverVec3& InVelocity, const FSolverReal Cd, const FSolverReal Cl, const FSolverReal Cp)
	{
		const TVec3<int32>& Element = Elements[ElementIndex];

		// Calculate the normal and the area of the surface exposed to the flow
		FSolverVec3 N = FSolverVec3::CrossProduct(
			InParticles.X(Element[1]) - InParticles.X(Element[0]),
			InParticles.X(Element[2]) - InParticles.X(Element[0]));
		const FSolverReal DoubleArea = N.SafeNormalize();

		// Calculate the direction and the relative velocity of the triangle to the flow
		const FSolverVec3& SurfaceVelocity = (FSolverReal)(1. / 3.) * (
			InParticles.V(Element[0]) +
			InParticles.V(Element[1]) +
			InParticles.V(Element[2]));
		const FSolverVec3 V = InVelocity - SurfaceVelocity;

		// Set the aerodynamic forces
		const FSolverReal VDotN = FSolverVec3::DotProduct(V, N);
		const FSolverReal VSquare = FSolverVec3::DotProduct(V, V);

		Forces[ElementIndex] = QuarterRho * DoubleArea * (VDotN >= (FSolverReal)0. ?  // The flow can hit either side of the triangle, so the normal might need to be reversed
			(Cd - Cl) * VDotN * V + Cl * VSquare * N :
			(Cl - Cd) * VDotN * V - Cl * VSquare * N) - DoubleArea * (FSolverReal)0.5 * Cp * N; // N points in the opposite direction of the actual mesh normals
	}

private:
	TConstArrayView<TArray<int32>> PointToTriangleMap;
	TConstArrayView<TVec3<int32>> Elements;
	TArray<FSolverVec3> Forces;
	TArray<FSolverVec3> Multipliers;
	FSolverVec3 Velocity;
	FSolverReal DragBase;
	FSolverReal DragRange;
	FSolverReal LiftBase;
	FSolverReal LiftRange;
	FSolverReal PressureBase; 
	FSolverReal PressureRange;
	FSolverReal QuarterRho;
	int32 Offset;
	int32 NumParticles;
};

using FVelocityField UE_DEPRECATED(5.1, "Chaos::Softs::FVelocityField has been renamed FVelocityAndPressureField to match its new behavior.") = FVelocityAndPressureField;

}  // End namespace Chaos::Softs

#if !defined(CHAOS_VELOCITY_FIELD_ISPC_ENABLED_DEFAULT)
#define CHAOS_VELOCITY_FIELD_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bChaos_VelocityField_ISPC_Enabled = INTEL_ISPC && CHAOS_VELOCITY_FIELD_ISPC_ENABLED_DEFAULT;
#else
extern CHAOS_API bool bChaos_VelocityField_ISPC_Enabled;
#endif
