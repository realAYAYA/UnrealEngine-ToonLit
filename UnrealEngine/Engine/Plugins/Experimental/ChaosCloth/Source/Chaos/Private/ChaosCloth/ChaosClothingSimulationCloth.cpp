// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosClothingSimulationMesh.h"
#include "ChaosCloth/ChaosClothingSimulationCollider.h"
#include "ChaosCloth/ChaosWeightMapTarget.h"
#include "ChaosCloth/ChaosClothPrivate.h"
#include "Containers/ArrayView.h"
#include "HAL/IConsoleManager.h"
#include "ClothingSimulation.h"

namespace Chaos
{

namespace ClothingSimulationClothConsoleVariables
{
	TAutoConsoleVariable<bool> CVarLegacyDisablesAccurateWind(TEXT("p.ChaosCloth.LegacyDisablesAccurateWind"), true, TEXT("Whether using the Legacy wind model switches off the accurate wind model, or adds up to it"));
}

FClothingSimulationCloth::FLODData::FLODData(
	int32 InNumParticles,
	const TConstArrayView<uint32>& InIndices,
	const TArray<TConstArrayView<FRealSingle>>& InWeightMaps,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& InTethers)
	: NumParticles(InNumParticles)
	, Indices(InIndices)
	, WeightMaps(InWeightMaps)
	, Tethers(InTethers)
{
}

void FClothingSimulationCloth::FLODData::Add(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth, int32 InLODIndex)
{
	check(Solver);
	check(Cloth);
	check(Cloth->Mesh);

	if (!NumParticles)
	{
		return;
	}

	// Add a new solver data chunk
	check(!SolverData.Find(Solver));
	FSolverData& SolverDatum = SolverData.Add(Solver);
	int32& Offset = SolverDatum.Offset;
	FTriangleMesh& TriangleMesh = SolverDatum.TriangleMesh;

	// Add particles
	Offset = Solver->AddParticles(NumParticles, Cloth->GroupId);  // TODO: Have a per solver map of offset

	// Update source mesh for this LOD, this is required prior to reset the start pose
	Cloth->Mesh->Update(Solver, INDEX_NONE, InLODIndex, 0, Offset);

	// Reset the particles start pose before setting up mass and constraints
	ResetStartPose(Solver);

	// Build a sim friendly triangle mesh including the solver particle's Offset
	const int32 NumElements = Indices.Num() / 3;
	TArray<TVec3<int32>> Elements;
	Elements.Reserve(NumElements);

	for (int32 i = 0; i < NumElements; ++i)
	{
		const int32 Index = 3 * i;
		Elements.Add(
			{ static_cast<int32>(Offset + Indices[Index]),
			 static_cast<int32>(Offset + Indices[Index + 1]),
			 static_cast<int32>(Offset + Indices[Index + 2]) });
	}

	TriangleMesh.Init(MoveTemp(Elements));
	TriangleMesh.GetPointToTriangleMap(); // Builds map for later use by GetPointNormals(), and the velocity fields

	// Initialize the normals, in case the sim data is queried before the simulation steps
	UpdateNormals(Solver);

	// Set the particle masses
	const TConstArrayView<FRealSingle>& MaxDistances = WeightMaps[(int32)EChaosWeightMapTarget::MaxDistance];
	static const FRealSingle KinematicDistanceThreshold = 0.1f;  // TODO: This is not the same value as set in the painting UI but we might want to expose this value as parameter
	auto KinematicPredicate =
		[MaxDistances](int32 Index)
		{
			return MaxDistances.IsValidIndex(Index) && MaxDistances[Index] < KinematicDistanceThreshold;
		};

	switch (Cloth->MassMode)
	{
	default:
		check(false);
	case EMassMode::UniformMass:
		Solver->SetParticleMassUniform(Offset, Cloth->MassValue, Cloth->MinPerParticleMass, TriangleMesh, KinematicPredicate);
		break;
	case EMassMode::TotalMass:
		Solver->SetParticleMassFromTotalMass(Offset, Cloth->MassValue, Cloth->MinPerParticleMass, TriangleMesh, KinematicPredicate);
		break;
	case EMassMode::Density:
		Solver->SetParticleMassFromDensity(Offset, Cloth->MassValue, Cloth->MinPerParticleMass, TriangleMesh, KinematicPredicate);
		break;
	}
	const TConstArrayView<Softs::FSolverReal> InvMasses(Solver->GetParticleInvMasses(Offset), NumParticles);

	// Setup solver constraints
	FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);
	const TArray<TVec3<int32>>& SurfaceElements = TriangleMesh.GetSurfaceElements();

	// Self collisions
	if (Cloth->bUseSelfCollisions)
	{
		static const int32 DisabledCollisionElementsN = 5;  // TODO: Make this a parameter?
		TSet<TVec2<int32>> DisabledCollisionElements;  // TODO: Is this needed? Turn this into a bit array?
	
		const int32 Range = Offset + NumParticles;
		for (int32 Index = Offset; Index < Range; ++Index)
		{
			const TSet<int32> Neighbors = TriangleMesh.GetNRing(Index, DisabledCollisionElementsN);
			for (int32 Element : Neighbors)
			{
				check(Index != Element);
				DisabledCollisionElements.Emplace(TVec2<int32>(Index, Element));
				DisabledCollisionElements.Emplace(TVec2<int32>(Element, Index));
			}
		}
		ClothConstraints.SetSelfCollisionConstraints(TriangleMesh, MoveTemp(DisabledCollisionElements), (Softs::FSolverReal)Cloth->SelfCollisionThickness, (Softs::FSolverReal)Cloth->SelfCollisionFrictionCoefficient, Cloth->bUseSelfIntersections, Cloth->bUseSelfIntersections);
	}

	// Edge constraints
	const TConstArrayView<FRealSingle>& EdgeStiffnessMultipliers = WeightMaps[(int32)EChaosWeightMapTarget::EdgeStiffness];
	if (Cloth->EdgeStiffness[0] > (FRealSingle)0. || (Cloth->EdgeStiffness[1] > (FRealSingle)0. && EdgeStiffnessMultipliers.Num() == NumParticles))
	{
		ClothConstraints.SetEdgeConstraints(SurfaceElements, EdgeStiffnessMultipliers, Cloth->bUseXPBDEdgeConstraints);
	}

	// Bending constraints
	const TConstArrayView<FRealSingle>& BendingStiffnessMultipliers = WeightMaps[(int32)EChaosWeightMapTarget::BendingStiffness];
	const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers = WeightMaps[(int32)EChaosWeightMapTarget::BucklingStiffness];
	if (Cloth->BendingStiffness[0] > (FRealSingle)0. || (Cloth->BendingStiffness[1] > (FRealSingle)0. && BendingStiffnessMultipliers.Num() == NumParticles) ||
		(Cloth->bUseBendingElements && (Cloth->BucklingStiffness[0] > (FRealSingle)0. || (Cloth->BucklingStiffness[1] > (FRealSingle)0. && BucklingStiffnessMultipliers.Num() == NumParticles))))
	{
		if (Cloth->bUseBendingElements)
		{
			TArray<Chaos::TVec4<int32>> BendingElements = TriangleMesh.GetUniqueAdjacentElements();
			ClothConstraints.SetBendingConstraints(MoveTemp(BendingElements), BendingStiffnessMultipliers, BucklingStiffnessMultipliers, Cloth->bUseXPBDBendingConstraints);
		}
		else
		{
			const TArray<Chaos::TVec2<int32>> Edges = TriangleMesh.GetUniqueAdjacentPoints();
			ClothConstraints.SetBendingConstraints(Edges, BendingStiffnessMultipliers, Cloth->bUseXPBDBendingConstraints);
		}
	}

	// Area constraints
	const TConstArrayView<FRealSingle>& AreaStiffnessMultipliers = WeightMaps[(int32)EChaosWeightMapTarget::AreaStiffness];
	if (Cloth->AreaStiffness[0] > (FRealSingle)0. || (Cloth->AreaStiffness[1] > (FRealSingle)0. && AreaStiffnessMultipliers.Num() == NumParticles))
	{
		ClothConstraints.SetAreaConstraints(SurfaceElements, AreaStiffnessMultipliers, Cloth->bUseXPBDAreaConstraints);
	}

	// Volume constraints
	if (Cloth->VolumeStiffness > (FRealSingle)0.)
	{
		if (Cloth->bUseThinShellVolumeConstraints)
		{
			TArray<Chaos::TVec2<int32>> BendingConstraints = TriangleMesh.GetUniqueAdjacentPoints();
			TArray<Chaos::TVec2<int32>> DoubleBendingConstraints;
			{
				TMap<int32, TArray<int32>> BendingHash;
				for (int32 i = 0; i < BendingConstraints.Num(); ++i)
				{
					BendingHash.FindOrAdd(BendingConstraints[i][0]).Add(BendingConstraints[i][1]);
					BendingHash.FindOrAdd(BendingConstraints[i][1]).Add(BendingConstraints[i][0]);
				}
				TSet<Chaos::TVec2<int32>> Visited;
				for (auto Elem : BendingHash)
				{
					for (int32 i = 0; i < Elem.Value.Num(); ++i)
					{
						for (int32 j = i + 1; j < Elem.Value.Num(); ++j)
						{
							if (Elem.Value[i] == Elem.Value[j])
								continue;
							auto NewElem = Chaos::TVec2<int32>(Elem.Value[i], Elem.Value[j]);
							if (!Visited.Contains(NewElem))
							{
								DoubleBendingConstraints.Add(NewElem);
								Visited.Add(NewElem);
								Visited.Add(Chaos::TVec2<int32>(Elem.Value[j], Elem.Value[i]));
							}
						}
					}
				}
			}

			ClothConstraints.SetVolumeConstraints(DoubleBendingConstraints, (Softs::FSolverReal)Cloth->VolumeStiffness);
		}
		else
		{
			TArray<Chaos::TVec3<int32>> SurfaceConstraints = SurfaceElements;
			ClothConstraints.SetVolumeConstraints(MoveTemp(SurfaceConstraints), (Softs::FSolverReal)Cloth->VolumeStiffness);
		}
	}

	// Long range constraints
	const TConstArrayView<FRealSingle>& TetherStiffnessMultipliers = WeightMaps[(int32)EChaosWeightMapTarget::TetherStiffness];
	if (Cloth->TetherStiffness[0] > (FRealSingle)0. || (Cloth->TetherStiffness[1] > (FRealSingle)0. && TetherStiffnessMultipliers.Num() == NumParticles))
	{
		const TConstArrayView<FRealSingle>& TetherScaleMultipliers = WeightMaps[(int32)EChaosWeightMapTarget::TetherScale];

		const Softs::FSolverReal MeshScale = Cloth->Mesh->GetScale();

		ClothConstraints.SetLongRangeConstraints(
			Tethers,
			TetherStiffnessMultipliers,
			TetherScaleMultipliers,
			Softs::FSolverVec2((Softs::FSolverReal)Cloth->TetherScale[0], (Softs::FSolverReal)Cloth->TetherScale[1]) * MeshScale);
	}

	// Max distances
	if (MaxDistances.Num() == NumParticles)
	{
		ClothConstraints.SetMaximumDistanceConstraints(MaxDistances);
	}

	// Backstop Constraints
	const TConstArrayView<FRealSingle>& BackstopDistances = WeightMaps[(int32)EChaosWeightMapTarget::BackstopDistance];
	const TConstArrayView<FRealSingle>& BackstopRadiuses = WeightMaps[(int32)EChaosWeightMapTarget::BackstopRadius];
	if (BackstopRadiuses.Num() == NumParticles && BackstopDistances.Num() == NumParticles)
	{
		ClothConstraints.SetBackstopConstraints(BackstopDistances, BackstopRadiuses, Cloth->bUseLegacyBackstop);
	}

	// Animation Drive Constraints
	const TConstArrayView<FRealSingle>& AnimDriveStiffnessMultipliers = WeightMaps[(int32)EChaosWeightMapTarget::AnimDriveStiffness];
	if (Cloth->AnimDriveStiffness[0] > (FRealSingle)0. || (Cloth->AnimDriveStiffness[1] > (FRealSingle)0. && AnimDriveStiffnessMultipliers.Num() == NumParticles))
	{
		const TConstArrayView<FRealSingle>& AnimDriveDampingMultipliers = WeightMaps[(int32)EChaosWeightMapTarget::AnimDriveDamping];
		ClothConstraints.SetAnimDriveConstraints(AnimDriveStiffnessMultipliers, AnimDriveDampingMultipliers);
	}

	// Shape target constraint
	if (Cloth->ShapeTargetStiffness > (FRealSingle)0.)
	{
		ClothConstraints.SetShapeTargetConstraints((Softs::FSolverReal)Cloth->ShapeTargetStiffness);
	}

	// Commit rules to solver
	ClothConstraints.CreateRules();

	// Disable constraints by default
	ClothConstraints.Enable(false);

	// Update LOD stats
	NumKinenamicParticles = 0;
	NumDynammicParticles = 0;
	for (int32 Index = 0; Index < NumParticles; ++Index)
	{
		if (InvMasses[Index] == (Softs::FSolverReal)0.)
		{
			++NumKinenamicParticles;
		}
		else
		{
			++NumDynammicParticles;
		}
	}
}

void FClothingSimulationCloth::FLODData::Remove(FClothingSimulationSolver* Solver)
{
	SolverData.Remove(Solver);
}

void FClothingSimulationCloth::FLODData::Update(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth)
{
	check(Solver);
	const int32 Offset = SolverData.FindChecked(Solver).Offset;
	check(Offset != INDEX_NONE);

	const Softs::FSolverReal MeshScale = Cloth->Mesh->GetScale();

	// Update the animatable constraint parameters
	FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);
	ClothConstraints.SetMaximumDistanceProperties((Softs::FSolverReal)Cloth->MaxDistancesMultiplier * MeshScale);
	ClothConstraints.SetEdgeProperties(Softs::FSolverVec2((Softs::FSolverReal)Cloth->EdgeStiffness[0], (Softs::FSolverReal)Cloth->EdgeStiffness[1]));
	ClothConstraints.SetBendingProperties(Softs::FSolverVec2((Softs::FSolverReal)Cloth->BendingStiffness[0], (Softs::FSolverReal)Cloth->BendingStiffness[1]), Cloth->BucklingRatio, Softs::FSolverVec2((Softs::FSolverReal)Cloth->BucklingStiffness[0], (Softs::FSolverReal)Cloth->BucklingStiffness[1]));
	ClothConstraints.SetAreaProperties(Softs::FSolverVec2((FSolverReal)Cloth->AreaStiffness[0], (Softs::FSolverReal)Cloth->AreaStiffness[1]));
	ClothConstraints.SetLongRangeAttachmentProperties(
		Softs::FSolverVec2((Softs::FSolverReal)Cloth->TetherStiffness[0], (Softs::FSolverReal)Cloth->TetherStiffness[1]),
		Softs::FSolverVec2((Softs::FSolverReal)Cloth->TetherScale[0], (Softs::FSolverReal)Cloth->TetherScale[1]) * MeshScale);
	ClothConstraints.SetSelfCollisionProperties((Softs::FSolverReal)Cloth->SelfCollisionThickness, (Softs::FSolverReal)Cloth->SelfCollisionFrictionCoefficient, Cloth->bUseSelfIntersections, Cloth->bUseSelfIntersections);
	ClothConstraints.SetAnimDriveProperties(
		Softs::FSolverVec2((Softs::FSolverReal)Cloth->AnimDriveStiffness[0], (Softs::FSolverReal)Cloth->AnimDriveStiffness[1]),
		Softs::FSolverVec2((Softs::FSolverReal)Cloth->AnimDriveDamping[0], (Softs::FSolverReal)Cloth->AnimDriveDamping[1]));
	ClothConstraints.SetThinShellVolumeProperties((Softs::FSolverReal)Cloth->VolumeStiffness);
	ClothConstraints.SetVolumeProperties((Softs::FSolverReal)Cloth->VolumeStiffness);
	ClothConstraints.SetBackstopProperties(Cloth->bEnableBackstop, MeshScale);
}

void FClothingSimulationCloth::FLODData::Enable(FClothingSimulationSolver* Solver, bool bEnable) const
{
	check(Solver);
	const int32 Offset = SolverData.FindChecked(Solver).Offset;
	check(Offset != INDEX_NONE);
	
	// Enable particles (and related constraints)
	Solver->EnableParticles(Offset, bEnable);
}

void FClothingSimulationCloth::FLODData::ResetStartPose(FClothingSimulationSolver* Solver) const
{
	check(Solver);
	const int32 Offset = SolverData.FindChecked(Solver).Offset;
	check(Offset != INDEX_NONE);

	Solver->ResetStartPose(Offset, NumParticles);
}

void FClothingSimulationCloth::FLODData::UpdateNormals(FClothingSimulationSolver* Solver) const
{
	check(Solver);

	const FSolverData& SolverDatum = SolverData.FindChecked(Solver);
	const int32 Offset = SolverDatum.Offset;
	const FTriangleMesh& TriangleMesh = SolverDatum.TriangleMesh;

	check(Offset != INDEX_NONE);

	TConstArrayView<Softs::FSolverVec3> Points(Solver->GetParticleXs(Offset) - Offset, Offset + NumParticles);  // TODO: TriangleMesh still uses global array
	TArray<Softs::FSolverVec3> FaceNormals;
	TriangleMesh.GetFaceNormals(FaceNormals, Points, /*ReturnEmptyOnError =*/ false);

	TArrayView<Softs::FSolverVec3> Normals(Solver->GetNormals(Offset), NumParticles);
	TriangleMesh.GetPointNormals(Normals, TConstArrayView<Softs::FSolverVec3>(FaceNormals), /*bUseGlobalArray =*/ false);
}

FClothingSimulationCloth::FClothingSimulationCloth(
	FClothingSimulationMesh* InMesh,
	TArray<FClothingSimulationCollider*>&& InColliders,
	uint32 InGroupId,
	EMassMode InMassMode,
	FRealSingle InMassValue,
	FRealSingle InMinPerParticleMass,
	const TVec2<FRealSingle>& InEdgeStiffness,
	const TVec2<FRealSingle>& InBendingStiffness,
	FRealSingle InBucklingRatio,
	const TVec2<FRealSingle>& InBucklingStiffness,
	bool bInUseBendingElements,
	const TVec2<FRealSingle>& InAreaStiffness,
	FRealSingle InVolumeStiffness,
	bool bInUseThinShellVolumeConstraints,
	const TVec2<FRealSingle>& InTetherStiffness,
	const TVec2<FRealSingle>& InTetherScale,
	ETetherMode InTetherMode,
	FRealSingle InMaxDistancesMultiplier,
	const TVec2<FRealSingle>& InAnimDriveStiffness,
	const TVec2<FRealSingle>& InAnimDriveDamping,
	FRealSingle InShapeTargetStiffness,
	bool bInUseXPBDEdgeConstraints,
	bool bInUseXPBDBendingConstraints,
	bool bInUseXPBDAreaConstraints,
	FRealSingle InGravityScale,
	bool bInIsGravityOverridden,
	const TVec3<FRealSingle>& InGravityOverride,
	const TVec3<FRealSingle>& InLinearVelocityScale,
	FRealSingle InAngularVelocityScale,
	FRealSingle InFictitiousAngularScale,
	const TVec2<FRealSingle>& InDrag,
	const TVec2<FRealSingle>& InLift,
	bool bInUseLegacyWind,
	const TVec2<FRealSingle>& InPressure,
	FRealSingle InDampingCoefficient,
	FRealSingle InLocalDampingCoefficient,
	FRealSingle InCollisionThickness,
	FRealSingle InFrictionCoefficient,
	bool bInUseCCD,
	bool bInUseSelfCollisions,
	FRealSingle InSelfCollisionThickness,
	FRealSingle InSelfCollisionFrictionCoefficient,
	bool bInUseSelfIntersections,
	bool bInUseLegacyBackstop,
	bool bInUseLODIndexOverride, 
	int32 InLODIndexOverride)
	: Mesh(nullptr)
	, Colliders()
	, GroupId(InGroupId)
	, MassMode(InMassMode)
	, MassValue(InMassValue)
	, MinPerParticleMass(InMinPerParticleMass)
	, EdgeStiffness(InEdgeStiffness)
	, BendingStiffness(InBendingStiffness)
	, BucklingRatio(InBucklingRatio)
	, BucklingStiffness(InBucklingStiffness)
	, bUseBendingElements(bInUseBendingElements)
	, AreaStiffness(InAreaStiffness)
	, VolumeStiffness(InVolumeStiffness)
	, bUseThinShellVolumeConstraints(bInUseThinShellVolumeConstraints)
	, TetherStiffness(InTetherStiffness)
	, TetherScale(InTetherScale)
	, TetherMode(InTetherMode)
	, MaxDistancesMultiplier(InMaxDistancesMultiplier)
	, AnimDriveStiffness(InAnimDriveStiffness)
	, AnimDriveDamping(InAnimDriveDamping)
	, ShapeTargetStiffness(InShapeTargetStiffness)
	, bUseXPBDEdgeConstraints(bInUseXPBDEdgeConstraints)
	, bUseXPBDBendingConstraints(bInUseXPBDBendingConstraints)
	, bUseXPBDAreaConstraints(bInUseXPBDAreaConstraints)
	, GravityScale(InGravityScale)
	, bIsGravityOverridden(bInIsGravityOverridden)
	, GravityOverride(InGravityOverride)
	, LinearVelocityScale(InLinearVelocityScale)
	, AngularVelocityScale(InAngularVelocityScale)
	, FictitiousAngularScale(InFictitiousAngularScale)
	, Drag(InDrag)
	, Lift(InLift)
	, WindVelocity((FReal)0., (FReal)0., (FReal)0.)  // Set by clothing interactor
	, AirDensity(1.225e-6f)  // Set by clothing interactor
	, bUseLegacyWind(bInUseLegacyWind)
	, Pressure(InPressure)
	, DampingCoefficient(InDampingCoefficient)
	, LocalDampingCoefficient(InLocalDampingCoefficient)
	, CollisionThickness(InCollisionThickness)
	, FrictionCoefficient(InFrictionCoefficient)
	, bUseCCD(bInUseCCD)
	, bUseSelfCollisions(bInUseSelfCollisions)
	, SelfCollisionThickness(InSelfCollisionThickness)
	, SelfCollisionFrictionCoefficient(InSelfCollisionFrictionCoefficient)
	, bUseSelfIntersections(bInUseSelfIntersections)
	, bEnableBackstop(true)  // Set by clothing interactor
	, bUseLegacyBackstop(bInUseLegacyBackstop)
	, bUseLODIndexOverride(bInUseLODIndexOverride)
	, LODIndexOverride(InLODIndexOverride)
	, bNeedsReset(false)
	, bNeedsTeleport(false)
	, NumActiveKinematicParticles(0)
	, NumActiveDynamicParticles(0)
{
	SetMesh(InMesh);
	SetColliders(MoveTemp(InColliders));
}

FClothingSimulationCloth::~FClothingSimulationCloth()
{
}

void FClothingSimulationCloth::SetMesh(FClothingSimulationMesh* InMesh)
{
	Mesh = InMesh;

	// Reset LODs
	const int32 NumLODs = Mesh ? Mesh->GetNumLODs() : 0;
	LODData.Reset(NumLODs);
	const bool bUseGeodesicTethers = (TetherMode == ETetherMode::Geodesic);
	for (int32 Index = 0; Index < NumLODs; ++Index)
	{
		LODData.Emplace(
			Mesh->GetNumPoints(Index),
			Mesh->GetIndices(Index),
			Mesh->GetWeightMaps(Index),
			Mesh->GetTethers(Index, bUseGeodesicTethers));
	}

	// Iterate all known solvers
	TArray<FClothingSimulationSolver*> Solvers;
	LODIndices.GetKeys(Solvers);
	for (FClothingSimulationSolver* const Solver : Solvers)
	{
		// Refresh this cloth to recreate particles
		Solver->RefreshCloth(this);
	}
}

void FClothingSimulationCloth::SetColliders(TArray<FClothingSimulationCollider*>&& InColliders)
{
	// Empty the collider list, but keep the pointers around for the removal operation below
	const TArray<FClothingSimulationCollider*> TempColliders = MoveTemp(Colliders);

	// Replace with the new colliders
	Colliders = InColliders;

	// Iterate all known solvers
	TArray<FClothingSimulationSolver*> Solvers;
	LODIndices.GetKeys(Solvers);
	for (FClothingSimulationSolver* const Solver : Solvers)
	{
		// Remove any held collider data related to this cloth simulation
		for (FClothingSimulationCollider* const Collider : TempColliders)
		{
			Collider->Remove(Solver, this);
		}

		// Refresh this cloth to recreate collision particles
		Solver->RefreshCloth(this);
	}
}

void FClothingSimulationCloth::AddCollider(FClothingSimulationCollider* InCollider)
{
	check(InCollider);

	if (Colliders.Find(InCollider) != INDEX_NONE)
	{
		return;
	}

	// Add the collider to the solver update array
	Colliders.Emplace(InCollider);

	// Iterate all known solvers
	TArray<FClothingSimulationSolver*> Solvers;
	LODIndices.GetKeys(Solvers);
	for (FClothingSimulationSolver* const Solver : Solvers)
	{
		// Refresh this cloth to recreate collision particles
		Solver->RefreshCloth(this);
	}
}

void FClothingSimulationCloth::RemoveCollider(FClothingSimulationCollider* InCollider)
{
	if (Colliders.Find(InCollider) == INDEX_NONE)
	{
		return;
	}

	// Remove collider from array
	Colliders.RemoveSwap(InCollider);

	// Iterate all known solvers
	TArray<FClothingSimulationSolver*> Solvers;
	LODIndices.GetKeys(Solvers);
	for (FClothingSimulationSolver* const Solver : Solvers)
	{
		// Remove any held collider data related to this cloth simulation
		InCollider->Remove(Solver, this);

		// Refresh this cloth to recreate collision particles
		Solver->RefreshCloth(this);
	}
}

void FClothingSimulationCloth::RemoveColliders()
{
	// Empty the collider list, but keep the pointers around for the removal operation below
	const TArray<FClothingSimulationCollider*> TempColliders = MoveTemp(Colliders);

	// Iterate all known solvers
	TArray<FClothingSimulationSolver*> Solvers;
	LODIndices.GetKeys(Solvers);
	for (FClothingSimulationSolver* const Solver : Solvers)
	{
		// Remove any held collider data related to this cloth simulation
		for (FClothingSimulationCollider* const Collider : TempColliders)
		{
			Collider->Remove(Solver, this);
		}

		// Refresh this cloth to recreate collision particles
		Solver->RefreshCloth(this);
	}
}

void FClothingSimulationCloth::Add(FClothingSimulationSolver* Solver)
{
	check(Solver);

	// Can't add a cloth twice to the same solver
	check(!LODIndices.Find(Solver));

	// Initialize LODIndex
	int32& LODIndex = LODIndices.Add(Solver);
	LODIndex = INDEX_NONE;

	// Add LODs
	for (int32 Index = 0; Index < LODData.Num(); ++Index)
	{
		LODData[Index].Add(Solver, this, Index);
	}

	// Add Colliders
	for (FClothingSimulationCollider* Collider : Colliders)
	{
		Collider->Add(Solver, this);
	}
}

void FClothingSimulationCloth::Remove(FClothingSimulationSolver* Solver)
{
	// Remove Colliders
	for (FClothingSimulationCollider* Collider : Colliders)
	{
		Collider->Remove(Solver, this);
	}

	// Remove solver from maps
	LODIndices.Remove(Solver);
	for (FLODData& LODDatum: LODData)
	{
		LODDatum.Remove(Solver);
	}
}

int32 FClothingSimulationCloth::GetNumParticles(int32 InLODIndex) const
{
	return LODData.IsValidIndex(InLODIndex) ? LODData[InLODIndex].NumParticles : 0;
}

int32 FClothingSimulationCloth::GetOffset(const FClothingSimulationSolver* Solver, int32 InLODIndex) const
{
	return LODData.IsValidIndex(InLODIndex) ? LODData[InLODIndex].SolverData.FindChecked(Solver).Offset : 0;
}

TVec3<FRealSingle> FClothingSimulationCloth::GetGravity(const FClothingSimulationSolver* Solver) const
{
	check(Solver);
	return Solver->IsClothGravityOverrideEnabled() && bIsGravityOverridden ? GravityOverride : Solver->GetGravity() * GravityScale;
}

FAABB3 FClothingSimulationCloth::CalculateBoundingBox(const FClothingSimulationSolver* Solver) const
{
	check(Solver);

	// Calculate local space bounding box
	Softs::FSolverAABB3 BoundingBox = Softs::FSolverAABB3::EmptyAABB();

	const TConstArrayView<Softs::FSolverVec3> ParticlePositions = GetParticlePositions(Solver);
	for (const Softs::FSolverVec3& ParticlePosition : ParticlePositions)
	{
		BoundingBox.GrowToInclude(ParticlePosition);
	}

	// Return world space bounding box
	return FAABB3(BoundingBox).TransformedAABB(FRigidTransform3(Solver->GetLocalSpaceLocation(), FRotation3::Identity));
}

int32 FClothingSimulationCloth::GetOffset(const FClothingSimulationSolver* Solver) const
{
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	return LODData.IsValidIndex(LODIndex) ? GetOffset(Solver, LODIndex) : INDEX_NONE;
}

const FTriangleMesh& FClothingSimulationCloth::GetTriangleMesh(const FClothingSimulationSolver* Solver) const
{
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	static const FTriangleMesh EmptyTriangleMesh;
	return LODData.IsValidIndex(LODIndex) ? LODData[LODIndex].SolverData.FindChecked(Solver).TriangleMesh : EmptyTriangleMesh;
}

const TArray<TConstArrayView<FRealSingle>>& FClothingSimulationCloth::GetWeightMaps(const FClothingSimulationSolver* Solver) const
{
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	static const TArray<TConstArrayView<FRealSingle>> EmptyWeightMaps;
	return LODData.IsValidIndex(LODIndex) ? LODData[LODIndex].WeightMaps : EmptyWeightMaps;
}

const TArray<TConstArrayView<TTuple<int32, int32, float>>>& FClothingSimulationCloth::GetTethers(const FClothingSimulationSolver* Solver) const
{
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	static const TArray<TConstArrayView<TTuple<int32, int32, float>>> EmptyTethers;
	return LODData.IsValidIndex(LODIndex) ? LODData[LODIndex].Tethers : EmptyTethers;
}

int32 FClothingSimulationCloth::GetReferenceBoneIndex() const
{
	return Mesh ? Mesh->GetReferenceBoneIndex() : INDEX_NONE;
}

void FClothingSimulationCloth::PreUpdate(FClothingSimulationSolver* Solver)
{
	check(Solver);

	// Exit if the input mesh is missing
	if (!Mesh)
	{
		return;
	}

	// Update Cloth Colliders
	{
		SCOPE_CYCLE_COUNTER(STAT_ClothUpdateCollisions);

		for (FClothingSimulationCollider* Collider : Colliders)
		{
			Collider->PreUpdate(Solver, this);
		}
	}
}

void FClothingSimulationCloth::Update(FClothingSimulationSolver* Solver)
{
	check(Solver);

	// Exit if the input mesh is missing
	if (!Mesh)
	{
		return;
	}

	// Retrieve LOD Index, either from the override, or from the mesh input
	int32& LODIndex = LODIndices.FindChecked(Solver);  // Must be added to solver first

	const int32 PrevLODIndex = LODIndex;
	LODIndex = bUseLODIndexOverride && LODData.IsValidIndex(LODIndexOverride) ? LODIndexOverride : Mesh->GetLODIndex();

	// Update reference space transform from the mesh's reference bone transform  TODO: Add override in the style of LODIndexOverride
	const FRigidTransform3 OldReferenceSpaceTransform = ReferenceSpaceTransform;
	ReferenceSpaceTransform = Mesh->GetReferenceBoneTransform();
	ReferenceSpaceTransform.SetScale3D(FVec3(1.f));

	// Update Cloth Colliders
	{
		SCOPE_CYCLE_COUNTER(STAT_ClothUpdateCollisions);

		for (FClothingSimulationCollider* Collider : Colliders)
		{
			Collider->Update(Solver, this);
		}
	}

	// Update the source mesh skinned positions
	const int32 PrevOffset = GetOffset(Solver, PrevLODIndex);
	const int32 Offset = GetOffset(Solver, LODIndex);
	check(PrevOffset != INDEX_NONE && Offset != INDEX_NONE);

	Mesh->Update(Solver, PrevLODIndex, LODIndex, PrevOffset, Offset);

	// LOD Switching
	if (LODIndex != PrevLODIndex)
	{
		if (PrevLODIndex != INDEX_NONE)
		{
			// Disable previous LOD's particles
			LODData[PrevLODIndex].Enable(Solver, false);
		}
		if (LODIndex != INDEX_NONE)
		{
			// Enable new LOD's particles
			LODData[LODIndex].Enable(Solver, true);
			NumActiveKinematicParticles = LODData[LODIndex].NumKinenamicParticles;
			NumActiveDynamicParticles = LODData[LODIndex].NumDynammicParticles;

			// Wrap new LOD based on previous LOD if possible (can only do 1 level LOD at a time, and if previous LOD exists)
			bNeedsReset = bNeedsReset || !Mesh->WrapDeformLOD(
				PrevLODIndex,
				LODIndex,
				Solver->GetNormals(PrevOffset),
				Solver->GetParticlePandInvMs(PrevOffset),
				Solver->GetParticleVs(PrevOffset),
				Solver->GetParticlePandInvMs(Offset),
				Solver->GetParticleXs(Offset),
				Solver->GetParticleVs(Offset));

				// Update the wind velocity field for the new LOD mesh
				const TConstArrayView<FRealSingle>& DragMultipliers = LODData[LODIndex].WeightMaps[(int32)EChaosWeightMapTarget::Drag];
				const TConstArrayView<FRealSingle>& LiftMultipliers = LODData[LODIndex].WeightMaps[(int32)EChaosWeightMapTarget::Lift];
				const TConstArrayView<FRealSingle>& PressureMultipliers = LODData[LODIndex].WeightMaps[(int32)EChaosWeightMapTarget::Pressure];

				Solver->SetWindAndPressureGeometry(GroupId, GetTriangleMesh(Solver), DragMultipliers, LiftMultipliers, PressureMultipliers);
		}
		else
		{
			NumActiveKinematicParticles = 0;
			NumActiveDynamicParticles = 0;
		}
	}

	if (LODIndex != INDEX_NONE)
	{
		// Update Cloth group parameters  TODO: Cloth groups should exist as their own node object so that they can be used by several cloth objects
		LODData[LODIndex].Update(Solver, this);

		// TODO: Move all groupID updates out of the cloth update to allow to use of the same GroupId with different cloths

		// Set the reference input velocity and deal with teleport & reset; external forces depends on these values, so they must be initialized before then
		FVec3 OutLinearVelocityScale;
		FReal OutAngularVelocityScale;

		if (bNeedsReset)
		{
			// Make sure not to do any pre-sim transform just after a reset
			OutLinearVelocityScale = FVec3(1.f);
			OutAngularVelocityScale = 1.f;

			// Reset to start pose
			LODData[LODIndex].ResetStartPose(Solver);
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Cloth in group Id %d Needs reset."), GroupId);
		}
		else if (bNeedsTeleport)
		{
			// Remove all impulse velocity from the last frame
			OutLinearVelocityScale = FVec3(0.f);
			OutAngularVelocityScale = 0.f;
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Cloth in group Id %d Needs teleport."), GroupId);
		}
		else
		{
			// Use the cloth group's parameters
			OutLinearVelocityScale = LinearVelocityScale;
			OutAngularVelocityScale = AngularVelocityScale;
		}

		Solver->SetReferenceVelocityScale(
			GroupId,
			OldReferenceSpaceTransform,
			ReferenceSpaceTransform,
			OutLinearVelocityScale,
			OutAngularVelocityScale,
			FictitiousAngularScale);

		// Update gravity
		// This code relies on the solver gravity property being already set.
		// In order to use a cloth gravity override, it must first be enabled by the solver so that an override at solver level can still take precedence if needed.
		// In all cases apart from when the cloth override is used, the gravity scale must be combined to the solver gravity value.
		Solver->SetGravity(GroupId, GetGravity(Solver));

		// External forces (legacy wind+field)
		Solver->AddExternalForces(GroupId, bUseLegacyWind);

		if (bUseLegacyWind && ClothingSimulationClothConsoleVariables::CVarLegacyDisablesAccurateWind.GetValueOnAnyThread())
		{
			Solver->SetWindAndPressureProperties(GroupId, TVec2<FRealSingle>(0.), TVec2<FRealSingle>(0.), (FRealSingle)0., Pressure);  // Disable the wind velocity field
		}
		else
		{
			Solver->SetWindAndPressureProperties(GroupId, Drag, Lift, AirDensity, Pressure);
		}
		Solver->SetWindVelocity(GroupId, WindVelocity + Solver->GetWindVelocity());

		// Update general solver properties
		Solver->SetProperties(GroupId, DampingCoefficient, LocalDampingCoefficient, CollisionThickness, FrictionCoefficient);

		// Update use of continuous collision detection
		Solver->SetUseCCD(GroupId, bUseCCD);
	}

	// Reset trigger flags
	bNeedsTeleport = false;
	bNeedsReset = false;
}

void FClothingSimulationCloth::PostUpdate(FClothingSimulationSolver* Solver)
{
	check(Solver);

	const int32 LODIndex = LODIndices.FindChecked(Solver);
	if (LODIndex != INDEX_NONE)
	{
		// Update normals
		LODData[LODIndex].UpdateNormals(Solver);
	}
}

TConstArrayView<Softs::FSolverVec3> FClothingSimulationCloth::GetAnimationPositions(const FClothingSimulationSolver* Solver) const
{
	check(Solver);
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	check(GetOffset(Solver, LODIndex) != INDEX_NONE);
	return TConstArrayView<Softs::FSolverVec3>(Solver->GetAnimationPositions(GetOffset(Solver, LODIndex)), GetNumParticles(LODIndex));
}

TConstArrayView<Softs::FSolverVec3> FClothingSimulationCloth::GetAnimationNormals(const FClothingSimulationSolver* Solver) const
{
	check(Solver);
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	check(GetOffset(Solver, LODIndex) != INDEX_NONE);
	return TConstArrayView<Softs::FSolverVec3>(Solver->GetAnimationNormals(GetOffset(Solver, LODIndex)), GetNumParticles(LODIndex));
}

TConstArrayView<Softs::FSolverVec3> FClothingSimulationCloth::GetParticlePositions(const FClothingSimulationSolver* Solver) const
{
	check(Solver);
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	check(GetOffset(Solver, LODIndex) != INDEX_NONE);
	return TConstArrayView<Softs::FSolverVec3>(Solver->GetParticleXs(GetOffset(Solver, LODIndex)), GetNumParticles(LODIndex));
}

TConstArrayView<Softs::FSolverVec3> FClothingSimulationCloth::GetParticleNormals(const FClothingSimulationSolver* Solver) const
{
	check(Solver);
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	check(GetOffset(Solver, LODIndex) != INDEX_NONE);
	return TConstArrayView<Softs::FSolverVec3>(Solver->GetNormals(GetOffset(Solver, LODIndex)), GetNumParticles(LODIndex));
}

TConstArrayView<Softs::FSolverVec3> FClothingSimulationCloth::GetParticleVelocities(const FClothingSimulationSolver* Solver) const
{
	check(Solver);
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	check(GetOffset(Solver, LODIndex) != INDEX_NONE);
	return TConstArrayView<Softs::FSolverVec3>(Solver->GetParticleVs(GetOffset(Solver, LODIndex)), GetNumParticles(LODIndex));
}

TConstArrayView<Softs::FSolverReal> FClothingSimulationCloth::GetParticleInvMasses(const FClothingSimulationSolver* Solver) const
{
	check(Solver);
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	check(GetOffset(Solver, LODIndex) != INDEX_NONE);
	return TConstArrayView<Softs::FSolverReal>(Solver->GetParticleInvMasses(GetOffset(Solver, LODIndex)), GetNumParticles(LODIndex));
}

}  // End namespace Chaos
