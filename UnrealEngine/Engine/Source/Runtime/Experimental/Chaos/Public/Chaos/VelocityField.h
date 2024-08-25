// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/SoftsSolverParticlesRange.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Chaos/PBDFlatWeightMap.h"

namespace Chaos::Softs
{

// Velocity field used solely for aerodynamics effects, use Chaos Fields for other types of fields.
class FVelocityAndPressureField final
{
public:
	static constexpr FSolverReal DefaultDragCoefficient = (FSolverReal)0.5;
	static constexpr FSolverReal DefaultLiftCoefficient = (FSolverReal)0.1;
	static constexpr FSolverReal DefaultFluidDensity = (FSolverReal)1.225;
	static constexpr FSolverReal MinCoefficient = (FSolverReal)0.;   // Applies to both drag and lift
	static constexpr FSolverReal MaxCoefficient = (FSolverReal)10.;  //

	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return
			IsDragEnabled(PropertyCollection, false) ||
			IsLiftEnabled(PropertyCollection, false) ||
			IsPressureEnabled(PropertyCollection, false);
	}

	explicit FVelocityAndPressureField(const FCollectionPropertyConstFacade& PropertyCollection)
		: Offset(INDEX_NONE)
		, NumParticles(0)
		, Lift(GetWeightedFloatLift(PropertyCollection, (FSolverReal)0.).ClampAxes(MinCoefficient, MaxCoefficient))
		, Drag(GetWeightedFloatDrag(PropertyCollection, (FSolverReal)0.).ClampAxes(MinCoefficient, MaxCoefficient))
		, Pressure(GetWeightedFloatPressure(PropertyCollection, (FSolverReal)0.))
		, Rho(FMath::Max(GetFluidDensity(PropertyCollection, (FSolverReal)0.), (FSolverReal)0.))
		, QuarterRho(Rho * (FSolverReal)0.25f)
		, DragIndex(PropertyCollection)
		, LiftIndex(PropertyCollection)
		, FluidDensityIndex(PropertyCollection)
		, PressureIndex(PropertyCollection)
		, WindVelocityIndex(PropertyCollection)
	{
	}

	FVelocityAndPressureField(
		const FSolverParticlesRange& Particles,
		const FTriangleMesh* TriangleMesh,
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& Weightmaps,
		FSolverReal WorldScale)
		: Lift(GetWeightedFloatLift(PropertyCollection, (FSolverReal)0.).ClampAxes(MinCoefficient, MaxCoefficient))
		, Drag(GetWeightedFloatDrag(PropertyCollection, (FSolverReal)0.).ClampAxes(MinCoefficient, MaxCoefficient))
		, Pressure(GetWeightedFloatPressure(PropertyCollection, (FSolverReal)0.)/WorldScale)
		, DragIndex(PropertyCollection)
		, LiftIndex(PropertyCollection)
		, FluidDensityIndex(PropertyCollection)
		, PressureIndex(PropertyCollection)
		, WindVelocityIndex(PropertyCollection)
	{
		SetGeometry(Particles, TriangleMesh);
		SetProperties(
			FSolverVec2(GetWeightedFloatDrag(PropertyCollection, 0.f)),  // If these properties don't exist, set their values to 0, not to DefaultCoefficients!
			FSolverVec2(GetWeightedFloatLift(PropertyCollection, 0.f)),
			(FSolverReal)GetFluidDensity(PropertyCollection, 0.f),
			FSolverVec2(GetWeightedFloatPressure(PropertyCollection, 0.f)),  // These getters also initialize the property indices, so keep before SetMultipliers
			WorldScale);
		SetMultipliers(PropertyCollection, Weightmaps);
		InitColor(Particles);
	}

	// Construct an uninitialized field. Mesh, properties, and velocity will have to be set for this field to be valid.
	FVelocityAndPressureField()
		: Offset(INDEX_NONE)
		, NumParticles(0)
		, Lift(FSolverVec2(0.))
		, Drag(FSolverVec2(0.))
		, Pressure(FSolverVec2(0.))
		, DragIndex(ForceInit)
		, LiftIndex(ForceInit)
		, FluidDensityIndex(ForceInit)
		, PressureIndex(ForceInit)
		, WindVelocityIndex(ForceInit)
	{
		SetProperties(FSolverVec2(0.), FSolverVec2(0.), (FSolverReal)0., FSolverVec2(0.));
	}

	~FVelocityAndPressureField() {}

	CHAOS_API void UpdateForces(const FSolverParticles& InParticles, const FSolverReal /*Dt*/);

	inline void Apply(FSolverParticles& InParticles, const FSolverReal Dt, const int32 Index) const
	{
		checkSlow(Index >= Offset && Index < Offset + NumParticles);  // The index should always match the original triangle mesh range

		const TArray<int32>& ElementIndices = PointToTriangleMap[Index];
		for (const int32 ElementIndex : ElementIndices)
		{
			InParticles.Acceleration(Index) += InParticles.InvM(Index) * Forces[ElementIndex];
		}
	}

	CHAOS_API void Apply(FSolverParticlesRange& InParticles, const FSolverReal Dt) const;

	// This version will not load WindVelocity from the config. Call SetVelocity to set it explicitly.
	CHAOS_API void SetProperties(
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& Weightmaps,
		FSolverReal WorldScale,
		bool bEnableAerodynamics);

	// This version will load WindVelocity from the config
	CHAOS_API void SetPropertiesAndWind(
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& Weightmaps,
		FSolverReal WorldScale,
		bool bEnableAerodynamics,
		const FSolverVec3& SolverWind);

	UE_DEPRECATED(5.3, "Use SetProperties(const FCollectionPropertyConstFacade&, const TMap<FString, TConstArrayView<FRealSingle>>&, FSolverReal, bool) instead.")
	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection, FSolverReal WorldScale)
	{
		constexpr bool bEnableAerodynamics = true;
		SetProperties(PropertyCollection, TMap<FString, TConstArrayView<FRealSingle>>(), WorldScale, bEnableAerodynamics);
	}

	CHAOS_API void SetProperties(
		const FSolverVec2& Drag,
		const FSolverVec2& Lift,
		const FSolverReal FluidDensity,
		const FSolverVec2& Pressure = FSolverVec2::ZeroVector,
		FSolverReal WorldScale = 1.f);

	bool IsActive() const 
	{ 
		return Pressure.GetLow() != (FSolverReal)0. || Pressure.GetHigh() != (FSolverReal)0. ||
			(AreAerodynamicsEnabled() && (
				Drag.GetLow() > (FSolverReal)0. || Drag.GetOffsetRange()[1] != (FSolverReal)0. ||  // Note: range can be a negative value (although not when Lift or Drag base is zero)
				Lift.GetLow() > (FSolverReal)0. || Lift.GetOffsetRange()[1] != (FSolverReal)0.));
	}

	CHAOS_API void SetGeometry(
		const FTriangleMesh* TriangleMesh,
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& Weightmaps,
		FSolverReal WorldScale);

	CHAOS_API void SetGeometry(
		const FTriangleMesh* TriangleMesh,
		const TConstArrayView<FRealSingle>& DragMultipliers,
		const TConstArrayView<FRealSingle>& LiftMultipliers,
		const TConstArrayView<FRealSingle>& PressureMultipliers = TConstArrayView<FRealSingle>());

	void SetVelocity(const FSolverVec3& InVelocity) { Velocity = InVelocity; }

	TConstArrayView<TVector<int32, 3>> GetElements() const { return TConstArrayView<TVector<int32, 3>>(Elements); }
	TConstArrayView<FSolverVec3> GetForces() const { return TConstArrayView<FSolverVec3>(Forces); }

private:
	bool AreAerodynamicsEnabled() const { return QuarterRho > (FSolverReal)0.; }

	CHAOS_API void InitColor(const FSolverParticlesRange& InParticles);
	CHAOS_API void ResetColor(); // Used when setting geometry without Particles

	CHAOS_API void SetGeometry(const FSolverParticlesRange& Particles, const FTriangleMesh* TriangleMesh);
	CHAOS_API void SetGeometry(const FTriangleMesh* TriangleMesh);

	CHAOS_API void SetMultipliers(const FCollectionPropertyConstFacade& PropertyCollection,const TMap<FString, TConstArrayView<FRealSingle>>& Weightmaps);

	CHAOS_API void SetMultipliers(
		const TConstArrayView<FRealSingle>& DragMultipliers,
		const TConstArrayView<FRealSingle>& LiftMultipliers,
		const TConstArrayView<FRealSingle>& PressureMultipliers);


	template<typename SolverParticlesOrRange>
	FSolverVec3 CalculateForce(const SolverParticlesOrRange& InParticles, int32 ElementIndex, const FSolverVec3& InVelocity, const FSolverReal Cd, const FSolverReal Cl, const FSolverReal Cp) const 
	{
		const TVec3<int32>& Element = Elements[ElementIndex];

		// Calculate the normal and the area of the surface exposed to the flow
		FSolverVec3 N = FSolverVec3::CrossProduct(
			InParticles.GetX(Element[1]) - InParticles.GetX(Element[0]),
			InParticles.GetX(Element[2]) - InParticles.GetX(Element[0]));
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

		return QuarterRho * DoubleArea * (VDotN >= (FSolverReal)0. ?  // The flow can hit either side of the triangle, so the normal might need to be reversed
			(Cd - Cl) * VDotN * V + Cl * VSquare * N :
			(Cl - Cd) * VDotN * V - Cl * VSquare * N) - DoubleArea * (FSolverReal)0.5 * Cp * N; // N points in the opposite direction of the actual mesh normals
	}

	void UpdateField(const FSolverParticles& InParticles, int32 ElementIndex, const FSolverVec3& InVelocity, const FSolverReal Cd, const FSolverReal Cl, const FSolverReal Cp)
	{
		Forces[ElementIndex] = CalculateForce(InParticles, ElementIndex, InVelocity, Cd, Cl, Cp);
	}

	template<typename SolverParticlesOrRange>
	FSolverVec3 CalculateForce(const SolverParticlesOrRange& InParticles, int32 ElementIndex, const FSolverVec3& InVelocity, const FSolverReal Cd, const FSolverReal Cl, const FSolverReal Cp, const FSolverReal MaxVelocitySquared) const
	{
		checkSlow(MaxVelocitySquared > (FSolverReal)0);

		const TVec3<int32>& Element = Elements[ElementIndex];

		// Calculate the normal and the area of the surface exposed to the flow
		FSolverVec3 N = FSolverVec3::CrossProduct(
			InParticles.GetX(Element[1]) - InParticles.GetX(Element[0]),
			InParticles.GetX(Element[2]) - InParticles.GetX(Element[0]));
		const FSolverReal DoubleArea = N.SafeNormalize();

		// Calculate the direction and the relative velocity of the triangle to the flow
		const FSolverVec3& SurfaceVelocity = (FSolverReal)(1. / 3.) * (
			InParticles.V(Element[0]) +
			InParticles.V(Element[1]) +
			InParticles.V(Element[2]));
		FSolverVec3 V = InVelocity - SurfaceVelocity;

		// Clamp the velocity
		const FSolverReal RelVelocitySquared = V.SquaredLength();
		if (RelVelocitySquared > MaxVelocitySquared)
		{
			V *= FMath::Sqrt(MaxVelocitySquared / RelVelocitySquared);
		}

		// Set the aerodynamic forces
		const FSolverReal VDotN = FSolverVec3::DotProduct(V, N);
		const FSolverReal VSquare = FSolverVec3::DotProduct(V, V);

		return QuarterRho * DoubleArea * (VDotN >= (FSolverReal)0. ?  // The flow can hit either side of the triangle, so the normal might need to be reversed
			(Cd - Cl) * VDotN * V + Cl * VSquare * N :
			(Cl - Cd) * VDotN * V - Cl * VSquare * N) - DoubleArea * (FSolverReal)0.5 * Cp * N; // N points in the opposite direction of the actual mesh normals
	}

	void UpdateField(const FSolverParticles& InParticles, int32 ElementIndex, const FSolverVec3& InVelocity, const FSolverReal Cd, const FSolverReal Cl, const FSolverReal Cp, const FSolverReal MaxVelocitySquared)
	{
		Forces[ElementIndex] = CalculateForce(InParticles, ElementIndex, InVelocity, Cd, Cl, Cp, MaxVelocitySquared);
	}

private:
	int32 Offset;
	int32 NumParticles;
	TConstArrayView<TArray<int32>> PointToTriangleMap; // Points use global indexing. May point to PointToTriangleMapLocal or data in the original FTriangleMesh
	TConstArrayView<TVec3<int32>> Elements; // May point to ElementsLocal or data in the original FTriangleMesh
	TArray<TArray<int32>> PointToTriangleMapLocal; // Points use local indexing. Only used with ElementsLocal.
	TArray<TVec3<int32>> ElementsLocal; // Local copy of the triangle mesh's elements. Kinematic faces have been removed, and may be reordered by coloring.
	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
	FPBDFlatWeightMap Lift;
	FPBDFlatWeightMap Drag;
	FPBDFlatWeightMap Pressure;

	TArray<FSolverVec3> Forces;
	FSolverVec3 Velocity;
	FSolverReal Rho;
	FSolverReal QuarterRho;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(Drag, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(Lift, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(FluidDensity, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(Pressure, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(WindVelocity, FVector3f);
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
