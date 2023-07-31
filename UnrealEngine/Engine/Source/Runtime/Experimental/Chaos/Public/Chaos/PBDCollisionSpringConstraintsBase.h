// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Chaos/Core.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Containers/Set.h"


namespace Chaos
{
	class FTriangleMesh;
}

namespace Chaos::Softs
{

// This is an invertible spring class, typical springs are not invertible aware
class CHAOS_API FPBDCollisionSpringConstraintsBase
{
public:
	static constexpr FSolverReal BackCompatThickness = (FSolverReal)1.f;
	static constexpr FSolverReal BackCompatStiffness = (FSolverReal)0.5f;
	static constexpr FSolverReal BackCompatFrictionCoefficient = (FSolverReal)0.f;

	FPBDCollisionSpringConstraintsBase(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		const TArray<FSolverVec3>* InReferencePositions,
		TSet<TVec2<int32>>&& InDisabledCollisionElements,
		const FSolverReal InThickness = BackCompatThickness,
		const FSolverReal InStiffness = BackCompatStiffness,
		const FSolverReal InFrictionCoefficient = BackCompatFrictionCoefficient);

	virtual ~FPBDCollisionSpringConstraintsBase() {}

	UE_DEPRECATED(5.0, "Use Init(Particles, Spatial, GIAColors) instead.")
	void Init(const FSolverParticles& Particles);

	template<typename SpatialAccelerator>
	void Init(const FSolverParticles& Particles, const SpatialAccelerator& Spatial, const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);

	FSolverVec3 GetDelta(const FSolverParticles& InParticles, const int32 i) const;

	const TArray<TVec4<int32>>& GetConstraints() const { return Constraints;  }
	const TArray<FSolverVec3>& GetBarys() const { return Barys; }
	FSolverReal GetThickness() const { return Thickness; }
	bool GetGlobalIntersectionAnalysis() const { return bGlobalIntersectionAnalysis; }
	const TArray<bool>& GetFlipNormals() const { return FlipNormal; }

	void SetThickness(FSolverReal InThickness) { Thickness = FMath::Max(InThickness, (FSolverReal)0.);  }
	void SetFrictionCoefficient(FSolverReal InFrictionCoefficient) { FrictionCoefficient = InFrictionCoefficient; }

protected:
	TArray<TVec4<int32>> Constraints;
	TArray<FSolverVec3> Barys;
	TArray<bool> FlipNormal;

private:
	const FTriangleMesh& TriangleMesh;
	const TArray<TVec3<int32>>& Elements;
	const TArray<FSolverVec3>* ReferencePositions;
	const TSet<TVec2<int32>> DisabledCollisionElements;  // TODO: Make this a bitarray

	int32 Offset;
	int32 NumParticles;
	FSolverReal Thickness;
	FSolverReal Stiffness;
	FSolverReal FrictionCoefficient;
	bool bGlobalIntersectionAnalysis; // This is set based on which Init is called.
};

}  // End namespace Chaos::Softs

#endif
