// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureEngineFracturing.h"
#include "FractureEngineSelection.h"
#include "Dataflow/DataflowSelection.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/TransformCollection.h"
#include "Voronoi/Voronoi.h"
#include "PlanarCut.h"
#include "GeometryCollection/Facades/CollectionMeshFacade.h"


static void AddAdditionalAttributesIfRequired(FManagedArrayCollection& InOutCollection)
{
	FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(InOutCollection, -1);
}

static bool GetValidGeoCenter(FManagedArrayCollection& InOutCollection, 
	const TManagedArray<int32>& TransformToGeometryIndex, 
	const TArray<FTransform>& Transforms, 
	const TManagedArray<int32>& Parents,
	const TManagedArray<TSet<int32>>& Children, 
	const TManagedArray<FBox>& BoundingBoxes, 
	const TManagedArray<int32>& SimulationTypes,
	int32 TransformIndex, 
	FVector& OutGeoCenter)
{

	if (SimulationTypes[TransformIndex] == FGeometryCollection::ESimulationTypes::FST_Rigid)
	{
		OutGeoCenter = Transforms[TransformIndex].TransformPosition(BoundingBoxes[TransformToGeometryIndex[TransformIndex]].GetCenter());

		return true;
	}
	else if (SimulationTypes[TransformIndex] == FGeometryCollection::ESimulationTypes::FST_None) // ie this is embedded geometry
	{
		int32 Parent = Parents[TransformIndex];
		int32 ParentGeo = Parent != INDEX_NONE ? TransformToGeometryIndex[Parent] : INDEX_NONE;
		if (ensureMsgf(ParentGeo != INDEX_NONE, TEXT("Embedded geometry should always have a rigid geometry parent!  Geometry collection may be malformed.")))
		{
			OutGeoCenter = Transforms[Parents[TransformIndex]].TransformPosition(BoundingBoxes[ParentGeo].GetCenter());
		}
		else
		{
			return false; // no valid value to return
		}

		return true;
	}
	else
	{
		FVector AverageCenter;
		int32 ValidVectors = 0;
		for (int32 ChildIndex : Children[TransformIndex])
		{

			if (GetValidGeoCenter(InOutCollection, TransformToGeometryIndex, Transforms, Parents, Children, BoundingBoxes, SimulationTypes, ChildIndex, OutGeoCenter))
			{
				if (ValidVectors == 0)
				{
					AverageCenter = OutGeoCenter;
				}
				else
				{
					AverageCenter += OutGeoCenter;
				}
				++ValidVectors;
			}
		}

		if (ValidVectors > 0)
		{
			OutGeoCenter = AverageCenter / ValidVectors;
			return true;
		}
	}

	return false;
}

//
// TODO: Rewrite this using facades
//
void FFractureEngineFracturing::GenerateExplodedViewAttribute(FManagedArrayCollection& InOutCollection, const FVector& InScale, float InUniformScale)
{
	// Check if InOutCollection is not empty
	if (InOutCollection.HasAttribute("Transform", FGeometryCollection::TransformGroup))
	{
		InOutCollection.AddAttribute<FVector3f>("ExplodedVector", FGeometryCollection::TransformGroup, FManagedArrayCollection::FConstructionParameters(FName(), false));
		check(InOutCollection.HasAttribute("ExplodedVector", FGeometryCollection::TransformGroup));

		TManagedArray<FVector3f>& ExplodedVectors = InOutCollection.ModifyAttribute<FVector3f>("ExplodedVector", FGeometryCollection::TransformGroup);
		const TManagedArray<FTransform3f>& Transforms = InOutCollection.GetAttribute<FTransform3f>("Transform", FGeometryCollection::TransformGroup);
		const TManagedArray<int32>& TransformToGeometryIndices = InOutCollection.GetAttribute<int32>("TransformToGeometryIndex", FGeometryCollection::TransformGroup);
		const TManagedArray<FBox>& BoundingBoxes = InOutCollection.GetAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup);

		// Make sure we have valid "Level"
		AddAdditionalAttributesIfRequired(InOutCollection);

		const TManagedArray<int32>& Levels = InOutCollection.GetAttribute<int32>("Level", FTransformCollection::TransformGroup);
		const TManagedArray<int32>& Parents = InOutCollection.GetAttribute<int32>("Parent", FTransformCollection::TransformGroup);
		const TManagedArray<TSet<int32>>& Children = InOutCollection.GetAttribute<TSet<int32>>("Children", FGeometryCollection::TransformGroup);
		const TManagedArray<int32>& SimulationTypes = InOutCollection.GetAttribute<int32>("SimulationType", FGeometryCollection::TransformGroup);

		int32 ViewFractureLevel = -1;
		int32 MaxFractureLevel = ViewFractureLevel;
		for (int32 Idx = 0, ni = Transforms.Num(); Idx < ni; ++Idx)
		{
			if (Levels[Idx] > MaxFractureLevel)
				MaxFractureLevel = Levels[Idx];
		}

		TArray<FTransform> TransformArr;
		GeometryCollectionAlgo::GlobalMatrices(Transforms, Parents, TransformArr);

		TArray<FVector> TransformedCenters;
		TransformedCenters.SetNumUninitialized(TransformArr.Num());

		int32 TransformsCount = 0;

		FVector Center(ForceInitToZero);
		for (int32 Idx = 0, ni = Transforms.Num(); Idx < ni; ++Idx)
		{
			ExplodedVectors[Idx] = FVector3f::ZeroVector;
			FVector GeoCenter;

			if (GetValidGeoCenter(InOutCollection, TransformToGeometryIndices, TransformArr, Parents, Children, BoundingBoxes, SimulationTypes, Idx, GeoCenter))
			{
				TransformedCenters[Idx] = GeoCenter;
				if ((ViewFractureLevel < 0) || Levels[Idx] == ViewFractureLevel)
				{
					Center += TransformedCenters[Idx];
					++TransformsCount;
				}
			}
		}

		Center /= TransformsCount;

		for (int Level = 1; Level <= MaxFractureLevel; Level++)
		{
			for (int32 Idx = 0, ni = TransformArr.Num(); Idx < ni; ++Idx)
			{
				if ((ViewFractureLevel < 0) || Levels[Idx] == ViewFractureLevel)
				{
					FVector ScaleVec = InScale * InUniformScale;
					ExplodedVectors[Idx] = (FVector3f)(TransformedCenters[Idx] - Center) * (FVector3f)ScaleVec;
				}
				else
				{
					if (Parents[Idx] > -1)
					{
						ExplodedVectors[Idx] = ExplodedVectors[Parents[Idx]];
					}
				}
			}
		}
	}
}


static float GetMaxVertexMovement(float Grout, float Amplitude, int OctaveNumber, float Persistence)
{
	float MaxDisp = Grout;
	float AmplitudeScaled = Amplitude;

	for (int32 OctaveIdx = 0; OctaveIdx < OctaveNumber; OctaveIdx++, AmplitudeScaled *= Persistence)
	{
		MaxDisp += FMath::Abs(AmplitudeScaled);
	}

	return MaxDisp;
}


void FFractureEngineFracturing::VoronoiFracture(FManagedArrayCollection& InOutCollection,
	const FDataflowTransformSelection& InTransformSelection,
	const TArray<FVector>& InSites,
	float InRandomSeed,
	float InChanceToFracture,
	bool InGroupFracture,
	float InGrout,
	float InAmplitude,
	float InFrequency,
	float InPersistence,
	float InLacunarity,
	int32 InOctaveNumber,
	float InPointSpacing,
	bool InAddSamplesForCollision,
	float InCollisionSampleSpacing)
{
	if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InOutCollection.NewCopy<FGeometryCollection>()))
	{
		if (InSites.Num() > 0)
		{
			//
			// Compute BoundingBox for InCollection
			//
			FBox BoundingBox(ForceInit);

			if (InOutCollection.HasAttribute("Transform", FGeometryCollection::TransformGroup) &&
				InOutCollection.HasAttribute("Parent", FGeometryCollection::TransformGroup) &&
				InOutCollection.HasAttribute("TransformIndex", FGeometryCollection::GeometryGroup) &&
				InOutCollection.HasAttribute("BoundingBox", FGeometryCollection::GeometryGroup))
			{
				const TManagedArray<FTransform3f>& Transforms = InOutCollection.GetAttribute<FTransform3f>("Transform", FGeometryCollection::TransformGroup);
				const TManagedArray<int32>& ParentIndices = InOutCollection.GetAttribute<int32>("Parent", FGeometryCollection::TransformGroup);
				const TManagedArray<int32>& TransformIndices = InOutCollection.GetAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
				const TManagedArray<FBox>& BoundingBoxes = InOutCollection.GetAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup);

				TArray<FMatrix> TmpGlobalMatrices;
				GeometryCollectionAlgo::GlobalMatrices(Transforms, ParentIndices, TmpGlobalMatrices);

				if (TmpGlobalMatrices.Num() > 0)
				{
					for (int32 BoxIdx = 0; BoxIdx < BoundingBoxes.Num(); ++BoxIdx)
					{
						const int32 TransformIndex = TransformIndices[BoxIdx];
						BoundingBox += BoundingBoxes[BoxIdx].TransformBy(TmpGlobalMatrices[TransformIndex]);
					}
				}

				//
				// Compute Voronoi Bounds
				//
				FBox VoronoiBounds = BoundingBox;
				VoronoiBounds += FBox(InSites);

				VoronoiBounds = VoronoiBounds.ExpandBy(GetMaxVertexMovement(InGrout, InAmplitude, InOctaveNumber, InPersistence) + KINDA_SMALL_NUMBER);

				//
				// Voronoi Fracture
				//
				FNoiseSettings NoiseSettings;
				NoiseSettings.Amplitude = InAmplitude;
				NoiseSettings.Frequency = InFrequency;
				NoiseSettings.Octaves = InOctaveNumber;
				NoiseSettings.PointSpacing = InPointSpacing;
				NoiseSettings.Lacunarity = InLacunarity;
				NoiseSettings.Persistence = InPersistence;

				FVoronoiDiagram Voronoi(InSites, VoronoiBounds, .1f);
				
				FPlanarCells VoronoiPlanarCells = FPlanarCells(InSites, Voronoi);
				VoronoiPlanarCells.InternalSurfaceMaterials.NoiseSettings = NoiseSettings;

				TArray<int32> TransformSelectionArr = InTransformSelection.AsArray();
				if (!FFractureEngineSelection::IsBoneSelectionValid(InOutCollection, TransformSelectionArr))
				{
					return;
				}
				
				int ResultGeometryIndex = CutMultipleWithPlanarCells(VoronoiPlanarCells, *GeomCollection, TransformSelectionArr, InGrout, InCollisionSampleSpacing, InRandomSeed, FTransform().Identity);

				InOutCollection = (const FManagedArrayCollection&)(*GeomCollection);
			}
		}
	}
}


void FFractureEngineFracturing::PlaneCutter(FManagedArrayCollection& InOutCollection,
	const FDataflowTransformSelection& InTransformSelection,
	const FBox& InBoundingBox,
	int32 InNumPlanes,
	float InRandomSeed,
	float InGrout,
	float InAmplitude,
	float InFrequency,
	float InPersistence,
	float InLacunarity,
	int32 InOctaveNumber,
	float InPointSpacing,
	bool InAddSamplesForCollision,
	float InCollisionSampleSpacing)
{

	if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InOutCollection.NewCopy<FGeometryCollection>()))
	{
		TArray<FPlane> CuttingPlanes;
		TArray<FTransform> CuttingPlaneTransforms;

		FRandomStream RandStream(InRandomSeed);

		FBox Bounds = InBoundingBox;
		const FVector Extent(Bounds.Max - Bounds.Min);

		CuttingPlaneTransforms.Reserve(CuttingPlaneTransforms.Num() + InNumPlanes);
		for (int32 ii = 0; ii < InNumPlanes; ++ii)
		{
			FVector Position(Bounds.Min + FVector(RandStream.FRand(), RandStream.FRand(), RandStream.FRand()) * Extent);
			CuttingPlaneTransforms.Emplace(FTransform(FRotator(RandStream.FRand() * 360.0f, RandStream.FRand() * 360.0f, 0.0f), Position));
		}

		for (const FTransform& Transform : CuttingPlaneTransforms)
		{
			CuttingPlanes.Add(FPlane(Transform.GetLocation(), Transform.GetUnitAxis(EAxis::Z)));
		}

		FInternalSurfaceMaterials InternalSurfaceMaterials;
		FNoiseSettings NoiseSettings;

		if (InAmplitude > 0.f)
		{
			NoiseSettings.Amplitude = InAmplitude;
			NoiseSettings.Frequency = InFrequency;
			NoiseSettings.Lacunarity = InLacunarity;
			NoiseSettings.Persistence = InPersistence;
			NoiseSettings.Octaves = InOctaveNumber;
			NoiseSettings.PointSpacing = InPointSpacing;

			InternalSurfaceMaterials.NoiseSettings = NoiseSettings;
		}

		float CollisionSampleSpacingVal = InCollisionSampleSpacing;
		float GroutVal = InGrout;

		TArray<int32> TransformSelectionArr = InTransformSelection.AsArray();
		if (!FFractureEngineSelection::IsBoneSelectionValid(InOutCollection, TransformSelectionArr))
		{
			return;
		}

		int ResultGeometryIndex = CutMultipleWithMultiplePlanes(CuttingPlanes, InternalSurfaceMaterials, *GeomCollection, TransformSelectionArr, GroutVal, CollisionSampleSpacingVal, InRandomSeed, FTransform().Identity);

		InOutCollection = (const FManagedArrayCollection&)(*GeomCollection);
	}
}



