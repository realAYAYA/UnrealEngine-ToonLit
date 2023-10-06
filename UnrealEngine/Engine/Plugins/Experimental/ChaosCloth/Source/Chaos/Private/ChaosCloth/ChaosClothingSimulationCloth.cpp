// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosClothingSimulationMesh.h"
#include "ChaosCloth/ChaosClothingSimulationCollider.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "ChaosCloth/ChaosClothingPatternData.h"
#include "ChaosCloth/ChaosWeightMapTarget.h"
#include "ChaosCloth/ChaosClothPrivate.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Containers/ArrayView.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "HAL/IConsoleManager.h"
#include "ClothingSimulation.h"

namespace Chaos
{

namespace ClothingSimulationClothDefault
{
	constexpr int32 MassMode = (int32)FClothingSimulationCloth::EMassMode::Density;
	constexpr bool bUseGeodesicTethers = true;
	constexpr float MassValue = 0.35f;
	constexpr float MinPerParticleMass = 0.0001f;
	constexpr float CollisionThickness = 1.0f;
	constexpr float FrictionCoefficient = 0.8f;
	constexpr float DampingCoefficient = 0.01f;
	constexpr float Drag = 0.035f;
	constexpr float Lift = 0.035f;
	constexpr float Pressure = 0.f;
	constexpr float AirDensity = 1.225f;  // Air density in kg/m^3
	constexpr float GravityScale = 1.f;
	constexpr float GravityZOverride = -980.665f;
	constexpr float VelocityScale = 0.75f;
	constexpr float FictitiousAngularScale = 1.f;
}

namespace ClothingSimulationClothConsoleVariables
{
	TAutoConsoleVariable<bool> CVarLegacyDisablesAccurateWind(TEXT("p.ChaosCloth.LegacyDisablesAccurateWind"), true, TEXT("Whether using the Legacy wind model switches off the accurate wind model, or adds up to it"));
}

struct FClothingSimulationCloth::FLODData
{
	// Input mesh
	const int32 NumParticles;
	const TConstArrayView<uint32> Indices;
	const TMap<FString, TConstArrayView<FRealSingle>> WeightMaps;
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>> Tethers;

	const FClothingPatternData PatternData;

	// Per Solver data
	struct FSolverData
	{
		int32 LODIndex;
		int32 Offset;
		FTriangleMesh TriangleMesh;  // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
	};
	TMap<FClothingSimulationSolver*, FSolverData> SolverData;

	// Stats
	int32 NumKinematicParticles;
	int32 NumDynamicParticles;

	FLODData(
		int32 InNumParticles,
		const TConstArrayView<uint32>& InIndices,
		const TConstArrayView<FVector2f>& InPatternPositions,
		const TConstArrayView<uint32>& InPatternIndices,
		const TConstArrayView<uint32>& InPatternToWeldedIndices,
		TMap<FString, TConstArrayView<FRealSingle>>&& InWeightMaps,
		const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& InTethers);

	void Add(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth, int32 LODIndex);
	void Remove(FClothingSimulationSolver* Solver);

	void Update(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);

	void Enable(FClothingSimulationSolver* Solver, bool bEnable) const;

	void ResetStartPose(FClothingSimulationSolver* Solver) const;

	void UpdateNormals(FClothingSimulationSolver* Solver) const;
};

FClothingSimulationCloth::FLODData::FLODData(
	int32 InNumParticles,
	const TConstArrayView<uint32>& InIndices,
	const TConstArrayView<FVector2f>& InPatternPositions,
	const TConstArrayView<uint32>& InPatternIndices,
	const TConstArrayView<uint32>& InPatternToWeldedIndices,
	TMap<FString, TConstArrayView<FRealSingle>>&& InWeightMaps,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& InTethers)
	: NumParticles(InNumParticles)
	, Indices(InIndices)
	, WeightMaps(MoveTemp(InWeightMaps))
	, Tethers(InTethers)
	, PatternData(NumParticles, Indices, InPatternPositions, InPatternIndices, InPatternToWeldedIndices)
{
}

void FClothingSimulationCloth::FLODData::Add(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth, int32 InLODIndex)
{
	check(Solver);
	check(Cloth);
	check(Cloth->Mesh);

	// Add a new solver data chunk
	check(!SolverData.Find(Solver));
	FSolverData& SolverDatum = SolverData.Add(Solver);
	SolverDatum.LODIndex = InLODIndex;
	int32& Offset = SolverDatum.Offset;
	FTriangleMesh& TriangleMesh = SolverDatum.TriangleMesh;

	// Add particles
	Offset = Solver->AddParticles(NumParticles, Cloth->GroupId);  // TODO: Have a per solver map of offset

	if (!NumParticles)
	{
		return;
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	// Update source mesh for this LOD, this is required prior to reset the start pose
	Cloth->Mesh->Update(Solver, INDEX_NONE, InLODIndex, 0, Offset);
	
	// Retrieve the component's scale
	const Softs::FSolverReal MeshScale = Cloth->Mesh->GetScale();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

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

	TriangleMesh.Init(MoveTemp(Elements), Offset, Offset + NumParticles - 1);  // Init with the Offset to avoid discrepancies since the Triangle Mesh only relies on the used indices
	TriangleMesh.GetPointToTriangleMap(); // Builds map for later use by GetPointNormals(), and the velocity fields

	// Initialize the normals, in case the sim data is queried before the simulation steps
	UpdateNormals(Solver);

	// Retrieve config properties
	check(Cloth->Config);
	const Softs::FCollectionPropertyFacade& ConfigProperties = Cloth->Config->GetProperties(InLODIndex);

	// Retrieve MaxDistance information (weight map and Low/High values)
	int32 MaxDistancePropertyKeyIndex;
	FString MaxDistanceString = TEXT("MaxDistance");
	MaxDistanceString = ConfigProperties.GetStringValue(MaxDistanceString, MaxDistanceString, &MaxDistancePropertyKeyIndex);

	FRealSingle MaxDistanceBase;
	FRealSingle MaxDistanceRange;
	if (MaxDistancePropertyKeyIndex != INDEX_NONE)
	{
		MaxDistanceBase = (FRealSingle)ConfigProperties.GetLowValue<float>(MaxDistancePropertyKeyIndex);
		MaxDistanceRange = (FRealSingle)ConfigProperties.GetHighValue<float>(MaxDistancePropertyKeyIndex) - MaxDistanceBase;
	}
	else
	{
		MaxDistanceBase = (FRealSingle)0.;
		MaxDistanceRange = (FRealSingle)1.;
	}

	const TConstArrayView<FRealSingle> MaxDistances = WeightMaps.FindRef(MaxDistanceString);

	// Set the particle masses
	static const FRealSingle KinematicDistanceThreshold = 0.1f;  // TODO: This is not the same value as set in the painting UI but we might want to expose this value as parameter
	auto KinematicPredicate =
		[MaxDistances, MaxDistanceBase, MaxDistanceRange](int32 Index)
		{
			return (MaxDistances.IsValidIndex(Index) ? MaxDistanceBase + MaxDistanceRange * MaxDistances[Index] : MaxDistanceBase) < KinematicDistanceThreshold;
		};

	const int32 MassMode = ConfigProperties.GetValue<int32>(TEXT("MassMode"), ClothingSimulationClothDefault::MassMode);
	const FRealSingle MassValue = (FRealSingle)ConfigProperties.GetValue<float>(TEXT("MassValue"), ClothingSimulationClothDefault::MassValue);
	const FRealSingle MinPerParticleMass = (FRealSingle)ConfigProperties.GetValue<float>(TEXT("MinPerParticleMass"), ClothingSimulationClothDefault::MinPerParticleMass);

	switch (MassMode)
	{
	default:
		check(false);
	case EMassMode::UniformMass:
		Solver->SetParticleMassUniform(Offset, MassValue, MinPerParticleMass, TriangleMesh, KinematicPredicate);
		break;
	case EMassMode::TotalMass:
		Solver->SetParticleMassFromTotalMass(Offset, MassValue, MinPerParticleMass, TriangleMesh, KinematicPredicate);
		break;
	case EMassMode::Density:
		Solver->SetParticleMassFromDensity(Offset, MassValue, MinPerParticleMass, TriangleMesh, KinematicPredicate);
		break;
	}

	// Setup solver constraints
	FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);

	// Create constraints
	const bool bEnabled = false;  // Set constraint disabled by default
	ClothConstraints.AddRules(ConfigProperties, TriangleMesh, &PatternData, WeightMaps, Tethers, MeshScale, bEnabled);

	// Update LOD stats
	const TConstArrayView<Softs::FSolverReal> InvMasses(Solver->GetParticleInvMasses(Offset), NumParticles);
	NumKinematicParticles = 0;
	NumDynamicParticles = 0;
	for (int32 Index = 0; Index < NumParticles; ++Index)
	{
		if (InvMasses[Index] == (Softs::FSolverReal)0.)
		{
			++NumKinematicParticles;
		}
		else
		{
			++NumDynamicParticles;
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
	check(Cloth); 
	const FSolverData& SolverDatum = SolverData.FindChecked(Solver);

	const int32 Offset = SolverDatum.Offset;
	check(Offset != INDEX_NONE);

	// Update the animatable constraint parameters
	FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);

	check(Cloth->Config);
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	const Softs::FSolverReal MeshScale = Cloth->Mesh->GetScale();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	const Softs::FSolverReal MaxDistancesScale = (Softs::FSolverReal)Cloth->MaxDistancesMultiplier;
	ClothConstraints.Update(Cloth->Config->GetProperties(SolverDatum.LODIndex), WeightMaps, MeshScale, MaxDistancesScale);
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

	if (Offset != INDEX_NONE)
	{
		TConstArrayView<Softs::FSolverVec3> Points(Solver->GetParticleXs(Offset) - Offset, Offset + NumParticles);  // TODO: TriangleMesh still uses global array
		TArray<Softs::FSolverVec3> FaceNormals;
		TriangleMesh.GetFaceNormals(FaceNormals, Points, /*ReturnEmptyOnError =*/ false);

		TArrayView<Softs::FSolverVec3> Normals(Solver->GetNormals(Offset), NumParticles);
		TriangleMesh.GetPointNormals(Normals, TConstArrayView<Softs::FSolverVec3>(FaceNormals), /*bUseGlobalArray =*/ false);
	}
}

FClothingSimulationCloth::FClothingSimulationCloth(
	FClothingSimulationConfig* InConfig,
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	FClothingSimulationMesh* InMesh,
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	TArray<FClothingSimulationCollider*>&& InColliders,
	uint32 InGroupId)
	: GroupId(InGroupId)
{
	SetConfig(InConfig);
	SetMesh(InMesh);
	SetColliders(MoveTemp(InColliders));
}

FClothingSimulationCloth::FClothingSimulationCloth(
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	FClothingSimulationMesh* InMesh,
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
	FRealSingle /*InVolumeStiffness*/,  // Deprecated
	bool /*bInUseThinShellVolumeConstraints*/,  // Deprecated
	const TVec2<FRealSingle>& InTetherStiffness,
	const TVec2<FRealSingle>& InTetherScale,
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ETetherMode InTetherMode,
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FRealSingle InMaxDistancesMultiplier,
	const TVec2<FRealSingle>& InAnimDriveStiffness,
	const TVec2<FRealSingle>& InAnimDriveDamping,
	FRealSingle /*InShapeTargetStiffness*/,  // Deprecated
	bool bInUseXPBDEdgeConstraints,
	bool bInUseXPBDBendingConstraints,
	bool /*bInUseXPBDAreaConstraints*/,  // Deprecated
	FRealSingle InGravityScale,
	bool bInUseGravityOverride,
	const TVec3<FRealSingle>& InGravityOverride,
	const TVec3<FRealSingle>& InLinearVelocityScale,
	FRealSingle InAngularVelocityScale,
	FRealSingle InFictitiousAngularScale,
	const TVec2<FRealSingle>& InDrag,
	const TVec2<FRealSingle>& InLift,
	bool bInUsePointBasedWindModel,
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
	int32 InLODIndexOverride,
	const TVec2<FRealSingle>& InEdgeDampingRatio,
	const TVec2<FRealSingle>& InBendingDampingRatio)
	: GroupId(InGroupId)
	, PropertyCollection(MakeShared<FManagedArrayCollection>())
	, bUseLODIndexOverride(bInUseLODIndexOverride)
	, LODIndexOverride(InLODIndexOverride)
{
	// Turn parameters into a config
	Softs::FCollectionPropertyMutableFacade Properties(PropertyCollection);
	Properties.DefineSchema();

	constexpr bool bEnable = true;
	constexpr bool bAnimatable = true;
	
	// Mass
	{
		Properties.AddValue(TEXT("MassMode"), (int32)InMassMode);
		Properties.AddValue(TEXT("MassValue"), InMassValue);
		Properties.AddValue(TEXT("MinPerParticleMass"), InMinPerParticleMass);
	}
	
	// Edge constraint
	if (InEdgeStiffness[0] > 0.f || InEdgeStiffness[1] > 0.f)
	{
		const int32 EdgeSpringStiffnessIndex = Properties.AddProperty(TEXT("EdgeSpringStiffness"), bEnable, bAnimatable);
		Properties.SetWeightedValue(EdgeSpringStiffnessIndex, InEdgeStiffness[0], InEdgeStiffness[1]);
		Properties.SetStringValue(EdgeSpringStiffnessIndex, TEXT("EdgeStiffness"));
	}
	
	// Bending constraint
	if (InBendingStiffness[0] > 0.f || InBendingStiffness[1] > 0.f ||
		(bInUseBendingElements && (InBucklingStiffness[0] > 0.f || InBucklingStiffness[1] > 0.f)))
	{
		if (bInUseBendingElements)
		{
			const int32 BendingElementStiffnessIndex = Properties.AddProperty(TEXT("BendingElementStiffness"), bEnable, bAnimatable);
			Properties.SetWeightedValue(BendingElementStiffnessIndex, InBendingStiffness[0], InBendingStiffness[1]);
			Properties.SetStringValue(BendingElementStiffnessIndex, TEXT("BendingStiffness"));

			Properties.AddValue(TEXT("BucklingRatio"), InBucklingRatio);

			if (InBucklingStiffness[0] > 0.f || InBucklingStiffness[1] > 0.f)
			{
				const int32 BucklingStiffnessIndex = Properties.AddProperty(TEXT("BucklingStiffness"), bEnable, bAnimatable);
				Properties.SetWeightedValue(BucklingStiffnessIndex, InBucklingStiffness[0], InBucklingStiffness[1]);
				Properties.SetStringValue(BucklingStiffnessIndex, TEXT("BucklingStiffness"));
			}
		}
		else  // Not using bending elements
		{
			const int32 BendingSpringStiffnessIndex = Properties.AddProperty(TEXT("BendingSpringStiffness"), bEnable, bAnimatable);
			Properties.SetWeightedValue(BendingSpringStiffnessIndex, InBendingStiffness[0], InBendingStiffness[1]);
			Properties.SetStringValue(BendingSpringStiffnessIndex, TEXT("BendingStiffness"));
		}
	}

	// Area constraint
	if (InAreaStiffness[0] > 0.f || InAreaStiffness[1] > 0.f)
	{
		const int32 AreaSpringStiffnessIndex = Properties.AddProperty(TEXT("AreaSpringStiffness"), bEnable, bAnimatable);
		Properties.SetWeightedValue(AreaSpringStiffnessIndex, InAreaStiffness[0], InAreaStiffness[1]);
		Properties.SetStringValue(AreaSpringStiffnessIndex, TEXT("AreaStiffness"));
	}

	// Long range attachment
	if (InTetherStiffness[0] > 0.f || InTetherStiffness[1] > 0.f)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const bool bUseGeodesicTethers = InTetherMode == ETetherMode::Geodesic;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		Properties.AddValue(TEXT("UseGeodesicTethers"), bUseGeodesicTethers);

		const int32 TetherStiffnessIndex = Properties.AddProperty(TEXT("TetherStiffness"), bEnable , bAnimatable);
		Properties.SetWeightedValue(TetherStiffnessIndex, InTetherStiffness[0], InTetherStiffness[1]);
		Properties.SetStringValue(TetherStiffnessIndex, TEXT("TetherStiffness"));
	
		const int32 TetherScaleIndex = Properties.AddProperty(TEXT("TetherScale"), bEnable, bAnimatable);
		Properties.SetWeightedValue(TetherScaleIndex, InTetherScale[0], InTetherScale[1]);
		Properties.SetStringValue(TetherScaleIndex, TEXT("TetherScale"));
	}

	// AnimDrive
	if (InAnimDriveStiffness[0] > 0.f || InAnimDriveStiffness[1] > 0.f)
	{
		const int32 AnimDriveStiffnessIndex = Properties.AddProperty(TEXT("AnimDriveStiffness"), bEnable, bAnimatable);
		Properties.SetWeightedValue(AnimDriveStiffnessIndex, InAnimDriveStiffness[0], InAnimDriveStiffness[1]);
		Properties.SetStringValue(AnimDriveStiffnessIndex, TEXT("AnimDriveStiffness"));

		const int32 AnimDriveDampingIndex = Properties.AddProperty(TEXT("AnimDriveDamping"), bEnable, bAnimatable);
		Properties.SetWeightedValue(AnimDriveDampingIndex, InAnimDriveDamping[0], InAnimDriveDamping[1]);
		Properties.SetStringValue(AnimDriveDampingIndex, TEXT("AnimDriveDamping"));
	}

	// Gravity
	{
		Properties.AddValue(TEXT("GravityScale"), InGravityScale, bEnable, bAnimatable);
		Properties.AddValue(TEXT("UseGravityOverride"), bInUseGravityOverride, bEnable, bAnimatable);
		Properties.AddValue(TEXT("GravityOverride"), FVector3f(InGravityOverride), bEnable, bAnimatable);
	}

	// Velocity scale
	{
		Properties.AddValue(TEXT("LinearVelocityScale"), FVector3f(InLinearVelocityScale), bEnable, bAnimatable);
		Properties.AddValue(TEXT("AngularVelocityScale"), InAngularVelocityScale, bEnable, bAnimatable);
		Properties.AddValue(TEXT("FictitiousAngularScale"), InFictitiousAngularScale, bEnable, bAnimatable);
	}

	// Aerodynamics
	Properties.AddValue(TEXT("UsePointBasedWindModel"), bInUsePointBasedWindModel);
	if (!bInUsePointBasedWindModel && (InDrag[0] > 0.f || InDrag[1] > 0.f || InLift[0] > 0.f || InLift[1] > 0.f))
	{
		const int32 DragIndex = Properties.AddProperty(TEXT("Drag"), bEnable, bAnimatable);
		Properties.SetWeightedValue(DragIndex, InDrag[0], InDrag[1]);
		Properties.SetStringValue(DragIndex, TEXT("Drag"));

		const int32 LiftIndex = Properties.AddProperty(TEXT("Lift"), bEnable, bAnimatable);
		Properties.SetWeightedValue(LiftIndex, InLift[0], InLift[1]);
		Properties.SetStringValue(LiftIndex, TEXT("Lift"));

		Properties.AddValue(TEXT("FluidDensity"), ClothingSimulationClothDefault::AirDensity, bEnable, bAnimatable);

		Properties.AddValue(TEXT("WindVelocity"), FVector3f(0.f), bEnable, bAnimatable);  // Wind velocity must exist to be animatable
	}

	// Pressure
	if (InPressure[0] != 0.f || InPressure[1] != 0.f)
	{
		const int32 PressureIndex = Properties.AddProperty(TEXT("Pressure"), bEnable, bAnimatable);
		Properties.SetWeightedValue(PressureIndex, InPressure[0], InPressure[1]);
		Properties.SetStringValue(PressureIndex, TEXT("Pressure"));
	}

	// Damping
	Properties.AddValue(TEXT("DampingCoefficient"), InDampingCoefficient, bEnable, bAnimatable);
	Properties.AddValue(TEXT("LocalDampingCoefficient"), InLocalDampingCoefficient, bEnable, bAnimatable);

	// Collision
	Properties.AddValue(TEXT("CollisionThickness"), InCollisionThickness, bEnable, bAnimatable);
	Properties.AddValue(TEXT("FrictionCoefficient"), InFrictionCoefficient, bEnable, bAnimatable);
	Properties.AddValue(TEXT("UseCCD"), bInUseCCD, bEnable, bAnimatable);
	Properties.AddValue(TEXT("UseSelfCollisions"), bInUseSelfCollisions);
	Properties.AddValue(TEXT("SelfCollisionThickness"), InSelfCollisionThickness);
	Properties.AddValue(TEXT("SelfCollisionFrictionCoefficient"), InSelfCollisionFrictionCoefficient);
	Properties.AddValue(TEXT("UseSelfIntersections"), bInUseSelfIntersections);

	// Max distance
	{
		const int32 MaxDistanceIndex = Properties.AddProperty(TEXT("MaxDistance"));
		Properties.SetWeightedValue(MaxDistanceIndex, 0.f, 1.f);  // Backward compatibility with legacy mask must use a unit range since the multiplier is in the mask
		Properties.SetStringValue(MaxDistanceIndex, TEXT("MaxDistance"));
	}

	// Backstop
	{
		const int32 BackstopScaleIndex = Properties.AddProperty(TEXT("BackstopDistance"));
		Properties.SetWeightedValue(BackstopScaleIndex, 0.f, 1.f);  // Backward compatibility with legacy mask must use a unit range since the multiplier is in the mask
		Properties.SetStringValue(BackstopScaleIndex, TEXT("BackstopDistance"));

		const int32 BackstopRadiusIndex = Properties.AddProperty(TEXT("BackstopRadius"));
		Properties.SetWeightedValue(BackstopRadiusIndex, 0.f, 1.f);  // Backward compatibility with legacy mask must use a unit range since the multiplier is in the mask
		Properties.SetStringValue(BackstopRadiusIndex, TEXT("BackstopRadius"));

		Properties.AddValue(TEXT("UseLegacyBackstop"), bInUseLegacyBackstop);
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Config = new FClothingSimulationConfig(PropertyCollection);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Set mesh and colliders
	SetMesh(InMesh);
	SetColliders(MoveTemp(InColliders));
}

FClothingSimulationCloth::~FClothingSimulationCloth()
{
	// If the PropertyCollection is owned by this object, so does the current config object
	if (PropertyCollection.IsValid())
	{
		delete Config;
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
void FClothingSimulationCloth::SetMesh(FClothingSimulationMesh* InMesh)
{
	Mesh = InMesh;
	
	// Reset LODs
	const int32 NumLODs = Mesh ? Mesh->GetNumLODs() : 0;
	LODData.Reset(NumLODs);
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		// Regenerate LOD weight maps lookup map
		const TArray<FName> WeightMapNames = Mesh->GetWeightMapNames(LODIndex);
		TMap<FString, TConstArrayView<FRealSingle>> WeightMaps;
		WeightMaps.Reserve(WeightMapNames.Num());

		const TArray<TConstArrayView<FRealSingle>> WeightMapArray = Mesh->GetWeightMaps(LODIndex);
		ensure(WeightMapArray.Num() == WeightMapNames.Num());

		for (int32 WeightMapIndex = 0; WeightMapIndex < WeightMapNames.Num(); ++WeightMapIndex)
		{
			WeightMaps.Add(WeightMapNames[WeightMapIndex].ToString(),
				WeightMapArray.IsValidIndex(WeightMapIndex) ?
					WeightMapArray[WeightMapIndex] :
					TConstArrayView<FRealSingle>());
		}

		const bool bUseGeodesicTethers = Config->GetProperties(LODIndex).GetValue<bool>(TEXT("UseGeodesicTethers"), ClothingSimulationClothDefault::bUseGeodesicTethers);

		// Add LOD data
		LODData.Add(MakeUnique<FLODData>(
			Mesh->GetNumPoints(LODIndex),
			Mesh->GetIndices(LODIndex),
			Mesh->GetPatternPositions(LODIndex),
			Mesh->GetPatternIndices(LODIndex),
			Mesh->GetPatternToWeldedIndices(LODIndex),
			MoveTemp(WeightMaps),
			Mesh->GetTethers(LODIndex, bUseGeodesicTethers)));
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
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FClothingSimulationCloth::SetConfig(FClothingSimulationConfig* InConfig)
{
	// If the PropertyCollection is owned by this object, so does the current config object
	if (PropertyCollection.IsValid())
	{
		delete Config;
		PropertyCollection.Reset();
	}

	if (InConfig)
	{
		Config = InConfig;
	}
	else
	{
		// Create a default empty config object for coherence
		PropertyCollection = MakeShared<FManagedArrayCollection>();
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Config = new FClothingSimulationConfig(PropertyCollection); 
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
		LODData[Index]->Add(Solver, this, Index);
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
	for (TUniquePtr<FLODData>& LODDatum: LODData)
	{
		LODDatum->Remove(Solver);
	}
}

int32 FClothingSimulationCloth::GetNumParticles(int32 InLODIndex) const
{
	return LODData.IsValidIndex(InLODIndex) ? LODData[InLODIndex]->NumParticles : 0;
}

int32 FClothingSimulationCloth::GetOffset(const FClothingSimulationSolver* Solver, int32 InLODIndex) const
{
	return LODData.IsValidIndex(InLODIndex) ? LODData[InLODIndex]->SolverData.FindChecked(Solver).Offset : 0;
}

TVec3<FRealSingle> FClothingSimulationCloth::GetGravity(const FClothingSimulationSolver* Solver) const
{
	check(Solver);
	check(Config);
	const Softs::FCollectionPropertyFacade& ConfigProperties = Config->GetProperties(GetLODIndex(Solver));

	const bool bUseGravityOverride = ConfigProperties.GetValue<bool>(TEXT("UseGravityOverride"));
	const TVec3<FRealSingle> GravityOverride = (TVec3<FRealSingle>)ConfigProperties.GetValue<FVector3f>(TEXT("GravityOverride"), FVector3f(0.f, 0.f, ClothingSimulationClothDefault::GravityZOverride));
	const FRealSingle GravityScale = (FRealSingle)ConfigProperties.GetValue<float>(TEXT("GravityScale"), 1.f);

	return Solver->IsClothGravityOverrideEnabled() && bUseGravityOverride ? GravityOverride : Solver->GetGravity() * GravityScale;
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

int32 FClothingSimulationCloth::GetNumParticles(const FClothingSimulationSolver* Solver) const
{
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	return LODData.IsValidIndex(LODIndex) ? GetNumParticles(LODIndex) : 0;
}

const FTriangleMesh& FClothingSimulationCloth::GetTriangleMesh(const FClothingSimulationSolver* Solver) const
{
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	static const FTriangleMesh EmptyTriangleMesh;
	return LODData.IsValidIndex(LODIndex) ? LODData[LODIndex]->SolverData.FindChecked(Solver).TriangleMesh : EmptyTriangleMesh;
}

// Deprecated for 5.3
const TArray<TConstArrayView<FRealSingle>>& FClothingSimulationCloth::GetWeightMaps(const FClothingSimulationSolver* Solver) const
{
	static const TArray<TConstArrayView<FRealSingle>> EmptyWeightMaps;
	return EmptyWeightMaps;  // Can't return a reference anymore, so returning an empty weightmap array is probably the safest for existing code
}

TConstArrayView<FRealSingle> FClothingSimulationCloth::GetWeightMapByName(const FClothingSimulationSolver* Solver, const FString& Name) const
{
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	return LODData.IsValidIndex(LODIndex) ? LODData[LODIndex]->WeightMaps.FindRef(Name) : TConstArrayView<FRealSingle>();
}

TConstArrayView<FRealSingle> FClothingSimulationCloth::GetWeightMapByProperty(const FClothingSimulationSolver* Solver, const FString& Property) const
{
	check(Config);
	const FString PropertyString = Config->GetProperties(GetLODIndex(Solver)).GetStringValue(Property);
	return GetWeightMapByName(Solver, PropertyString);
}

const TArray<TConstArrayView<TTuple<int32, int32, float>>>& FClothingSimulationCloth::GetTethers(const FClothingSimulationSolver* Solver) const
{
	const int32 LODIndex = LODIndices.FindChecked(Solver);
	static const TArray<TConstArrayView<TTuple<int32, int32, float>>> EmptyTethers;
	return LODData.IsValidIndex(LODIndex) ? LODData[LODIndex]->Tethers : EmptyTethers;
}

int32 FClothingSimulationCloth::GetReferenceBoneIndex() const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	return Mesh ? Mesh->GetReferenceBoneIndex() : INDEX_NONE;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	LODIndex = bUseLODIndexOverride && LODData.IsValidIndex(LODIndexOverride) ? LODIndexOverride : Mesh->GetLODIndex();

	// Update reference space transform from the mesh's reference bone transform  TODO: Add override in the style of LODIndexOverride
	const FRigidTransform3 OldReferenceSpaceTransform = ReferenceSpaceTransform;
	ReferenceSpaceTransform = Mesh->GetReferenceBoneTransform();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
	if (PrevOffset == INDEX_NONE || Offset == INDEX_NONE)
	{
		return;
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	Mesh->Update(Solver, PrevLODIndex, LODIndex, PrevOffset, Offset);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Update all LODs dirty properties, since it is easier done than re-updating all properties when switching LODs
	if (Config->IsLegacySingleLOD())
	{
		for (TUniquePtr<FLODData>& LODDatum : LODData)
		{
			LODDatum->Update(Solver, this);
		}
	}

	// Retrieve config
	check(Config);
	const Softs::FCollectionPropertyFacade& ConfigProperties = Config->GetProperties(LODIndex);

	// LOD Switching
	if (LODIndex != PrevLODIndex)
	{
		if (PrevLODIndex != INDEX_NONE)
		{
			// Disable previous LOD's particles
			LODData[PrevLODIndex]->Enable(Solver, false);
		}
		if (LODIndex != INDEX_NONE)
		{
			// Enable new LOD's particles
			LODData[LODIndex]->Enable(Solver, true);
			NumActiveKinematicParticles = LODData[LODIndex]->NumKinematicParticles;
			NumActiveDynamicParticles = LODData[LODIndex]->NumDynamicParticles;

			// Wrap new LOD based on previous LOD if possible (can only do 1 level LOD at a time, and if previous LOD exists)
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
			bNeedsReset = bNeedsReset || !Mesh->WrapDeformLOD(
PRAGMA_ENABLE_DEPRECATION_WARNINGS
				PrevLODIndex,
				LODIndex,
				Solver->GetNormals(PrevOffset),
				Solver->GetParticlePandInvMs(PrevOffset),
				Solver->GetParticleVs(PrevOffset),
				Solver->GetParticlePandInvMs(Offset),
				Solver->GetParticleXs(Offset),
				Solver->GetParticleVs(Offset));

			// Update the wind velocity field for the new LOD mesh
			Solver->SetWindAndPressureGeometry(GroupId, GetTriangleMesh(Solver), ConfigProperties, LODData[LODIndex]->WeightMaps);
		}
		else
		{
			NumActiveKinematicParticles = 0;
			NumActiveDynamicParticles = 0;
		}
	}

	// Update Cloth group parameters
	if (LODIndex != INDEX_NONE)
	{
		// Update Cloth group parameters  TODO: Cloth groups should exist as their own node object so that they can be used by several cloth objects
		// This was updated above for all LODs if single lod config.
		if (!Config->IsLegacySingleLOD())
		{
			LODData[LODIndex]->Update(Solver, this);
		}

		// TODO: Move all groupID updates out of the cloth update to allow to use of the same GroupId with different cloths

		// Set the reference input velocity and deal with teleport & reset; external forces depends on these values, so they must be initialized before then
		FVec3f OutLinearVelocityScale;
		FRealSingle OutAngularVelocityScale;

		if (bNeedsReset)
		{
			// Make sure not to do any pre-sim transform just after a reset
			OutLinearVelocityScale = FVec3f(1.f);
			OutAngularVelocityScale = 1.f;

			// Reset to start pose
			LODData[LODIndex]->ResetStartPose(Solver);
			for (FClothingSimulationCollider* Collider : Colliders)
			{
				Collider->ResetStartPose(Solver, this);
			}
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Cloth in group Id %d Needs reset."), GroupId);
		}
		else if (bNeedsTeleport)
		{
			// Remove all impulse velocity from the last frame
			OutLinearVelocityScale = FVec3f(0.f);
			OutAngularVelocityScale = 0.f;
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Cloth in group Id %d Needs teleport."), GroupId);
		}
		else
		{
			// Use the cloth config parameters
			OutLinearVelocityScale = ConfigProperties.GetValue<FVector3f>(TEXT("LinearVelocityScale"), FVector3f(ClothingSimulationClothDefault::VelocityScale));
			OutAngularVelocityScale = ConfigProperties.GetValue<float>(TEXT("AngularVelocityScale"), ClothingSimulationClothDefault::VelocityScale);
		}

		const FRealSingle FictitiousAngularScale = ConfigProperties.GetValue<float>(TEXT("FictitiousAngularScale"), ClothingSimulationClothDefault::FictitiousAngularScale);

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
		const bool bUsePointBasedWindModel = ConfigProperties.GetValue<bool>(TEXT("UsePointBasedWindModel"));
		Solver->AddExternalForces(GroupId, bUsePointBasedWindModel);

		const bool bPointBasedWindDisablesAccurateWind = ClothingSimulationClothConsoleVariables::CVarLegacyDisablesAccurateWind.GetValueOnAnyThread();
		const bool bEnableAerodynamics = !(bUsePointBasedWindModel && bPointBasedWindDisablesAccurateWind);
		Solver->SetWindAndPressureProperties(GroupId, ConfigProperties, LODData[LODIndex]->WeightMaps, bEnableAerodynamics);

		constexpr float WorldScale = 100.f;  // VelocityField wind is in m/s in the config (same as the wind unit), but cm/s in the solver  TODO: Cleanup the Solver SetWindVelocity functions to be consistent with the unit
		const FVec3f WindVelocity = ConfigProperties.GetValue<FVector3f>(TEXT("WindVelocity")) * WorldScale;
		Solver->SetWindVelocity(GroupId, WindVelocity + Solver->GetWindVelocity());

		// Update general solver properties
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		const Softs::FSolverReal MeshScale = Mesh->GetScale();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		const FRealSingle DampingCoefficient = ConfigProperties.GetValue<float>(TEXT("DampingCoefficient"), ClothingSimulationClothDefault::DampingCoefficient);
		const FRealSingle LocalDampingCoefficient = ConfigProperties.GetValue<float>(TEXT("LocalDampingCoefficient"));
		const FRealSingle CollisionThickness = ConfigProperties.GetValue<float>(TEXT("CollisionThickness"), ClothingSimulationClothDefault::CollisionThickness);
		const FRealSingle FrictionCoefficient = ConfigProperties.GetValue<float>(TEXT("FrictionCoefficient"), ClothingSimulationClothDefault::FrictionCoefficient);
		Solver->SetProperties(GroupId, DampingCoefficient, LocalDampingCoefficient, CollisionThickness * MeshScale, FrictionCoefficient);

		// Update use of continuous collision detection
		const bool bUseCCD = ConfigProperties.GetValue<bool>(TEXT("UseCCD"));
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
		LODData[LODIndex]->UpdateNormals(Solver);
	}
}

void FClothingSimulationCloth::UpdateFromCache(const FClothingSimulationCacheData& CacheData)
{
	if (const FTransform* CachedReferenceSpaceTransform = CacheData.CachedReferenceSpaceTransforms.Find(GetGroupId()))
	{
		ReferenceSpaceTransform = *CachedReferenceSpaceTransform;
		ReferenceSpaceTransform.SetScale3D(FVec3(1.f));
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

void FClothingSimulationCloth::SetMaterialProperties(const TVec2<FRealSingle>& InEdgeStiffness, const TVec2<FRealSingle>& InBendingStiffness, const TVec2<FRealSingle>& InAreaStiffness)
{
	Config->GetProperties().SetWeightedFloatValue(TEXT("EdgeSpringStiffness"), FVector2f(InEdgeStiffness));
	Config->GetProperties().SetWeightedFloatValue(TEXT("BendingSpringStiffness"), FVector2f(InBendingStiffness));
	Config->GetProperties().SetWeightedFloatValue(TEXT("AreaSpringStiffness"), FVector2f(InAreaStiffness));
}

void FClothingSimulationCloth::SetLongRangeAttachmentProperties(const TVec2<FRealSingle>& InTetherStiffness, const TVec2<FRealSingle>& InTetherScale)
{
	Config->GetProperties().SetWeightedFloatValue(TEXT("TetherStiffness"), FVector2f(InTetherStiffness));
	Config->GetProperties().SetWeightedFloatValue(TEXT("TetherScale"), FVector2f(InTetherScale));
}

void FClothingSimulationCloth::SetCollisionProperties(FRealSingle InCollisionThickness, FRealSingle InFrictionCoefficient, bool bInUseCCD, FRealSingle InSelfCollisionThickness)
{
	Config->GetProperties().SetValue(TEXT("CollisionThickness"), (float)InCollisionThickness);
	Config->GetProperties().SetValue(TEXT("FrictionCoefficient"), (float)InFrictionCoefficient);
	Config->GetProperties().SetValue(TEXT("UseCCD"), bInUseCCD);
	Config->GetProperties().SetValue(TEXT("SelfCollisionThickness"), (float)InSelfCollisionThickness);
}

void FClothingSimulationCloth::SetBackstopProperties(bool bInEnableBackstop)
{
	Config->GetProperties().SetEnabled(TEXT("BackstopRadius"), bInEnableBackstop);  // BackstopRadius controls whether the backstop is enabled or not
}

void FClothingSimulationCloth::SetDampingProperties(FRealSingle InDampingCoefficient, FRealSingle InLocalDampingCoefficient)
{
	Config->GetProperties().SetValue(TEXT("DampingCoefficient"), (float)InDampingCoefficient);
	Config->GetProperties().SetValue(TEXT("LocalDampingCoefficient"), (float)InLocalDampingCoefficient);
}

void FClothingSimulationCloth::SetAerodynamicsProperties(const TVec2<FRealSingle>& InDrag, const TVec2<FRealSingle>& InLift, FRealSingle InAirDensity, const FVec3& InWindVelocity)
{
	constexpr float WorldScale = 100.f;  // Interactor values are setup in engine scale, kg/cm^3 for air density and cm/s for wind velocity, but the properties are stored in kg/m^3 and m/s in the UI.
	Config->GetProperties().SetWeightedFloatValue(TEXT("Drag"), FVector2f(InDrag));
	Config->GetProperties().SetWeightedFloatValue(TEXT("Lift"), FVector2f(InLift));
	Config->GetProperties().SetValue(TEXT("FluidDensity"), (float)InAirDensity * FMath::Cube(WorldScale));
	Config->GetProperties().SetValue(TEXT("WindVelocity"), FVector3f(InWindVelocity) / WorldScale);
}

void FClothingSimulationCloth::SetPressureProperties(const TVec2<FRealSingle>& InPressure)
{
	Config->GetProperties().SetWeightedFloatValue(TEXT("Pressure"), FVector2f(InPressure));
}

void FClothingSimulationCloth::SetGravityProperties(FRealSingle InGravityScale, bool bInUseGravityOverride, const FVec3& InGravityOverride)
{
	Config->GetProperties().SetValue(TEXT("GravityScale"), (float)InGravityScale);
	Config->GetProperties().SetValue(TEXT("UseGravityOverride"), bInUseGravityOverride);
	Config->GetProperties().SetValue(TEXT("GravityOverride"), FVector3f(InGravityOverride));
}

void FClothingSimulationCloth::SetAnimDriveProperties(const TVec2<FRealSingle>& InAnimDriveStiffness, const TVec2<FRealSingle>& InAnimDriveDamping)
{
	Config->GetProperties().SetWeightedFloatValue(TEXT("AnimDriveStiffness"), FVector2f(InAnimDriveStiffness));
	Config->GetProperties().SetWeightedFloatValue(TEXT("AnimDriveDamping"), FVector2f(InAnimDriveDamping));
}

void FClothingSimulationCloth::GetAnimDriveProperties(TVec2<FRealSingle>& OutAnimDriveStiffness, TVec2<FRealSingle>& OutAnimDriveDamping)
{
	OutAnimDriveStiffness = TVec2<FRealSingle>(Config->GetProperties().GetWeightedFloatValue(TEXT("AnimDriveStiffness")));
	OutAnimDriveDamping = TVec2<FRealSingle>(Config->GetProperties().GetWeightedFloatValue(TEXT("AnimDriveDamping")));
}

void FClothingSimulationCloth::SetVelocityScaleProperties(const FVec3& InLinearVelocityScale, FRealSingle InAngularVelocityScale, FRealSingle InFictitiousAngularScale)
{
	Config->GetProperties().SetValue(TEXT("LinearVelocityScale"), FVector3f(InLinearVelocityScale));
	Config->GetProperties().SetValue(TEXT("AngularVelocityScale"), (float)InAngularVelocityScale);
	Config->GetProperties().SetValue(TEXT("FictitiousAngularScale"), (float)InFictitiousAngularScale);
}

}  // End namespace Chaos
