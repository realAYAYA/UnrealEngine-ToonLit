// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionNodes.h"
#include "Dataflow/DataflowCore.h"

#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineUtility.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "UObject/UnrealTypePrivate.h"

#include "EngineGlobals.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "Math/UnrealMathUtility.h"
#include "PlanarCut.h"
#include "Voronoi/Voronoi.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionNodes)

namespace Dataflow
{
	void GeometryCollectionEngineAssetNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetCollectionAssetDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExampleCollectionEditDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetCollectionAssetDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FResetGeometryCollectionDataflowNode);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FPrintStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FLogStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBoundingBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExpandBoundingBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVectorToStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatToStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakePointsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FUniformScatterPointsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRadialScatterPointsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralIntDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralBoolDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FIntToStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBoolToStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExpandVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FIntToFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVoronoiFractureDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FStringAppendDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomFloatInRangeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomUnitVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomUnitVectorInConeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRadiansToDegreesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDegreesToRadiansDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExplodedViewDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCreateNonOverlappingConvexHullsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FPlaneCutterDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FHashStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FHashVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatToIntDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMathConstantsDataflowNode);
	}
}

void FGetCollectionAssetDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Output))
	{
		if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
		{
			if (UGeometryCollection* CollectionAsset = Cast<UGeometryCollection>(EngineContext->Owner))
			{
				if (const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> AssetCollection = CollectionAsset->GetGeometryCollection())
				{
					SetValue<DataType>(Context, DataType(*AssetCollection), &Output);
				}
			}
		}
	}
}

void FExampleCollectionEditDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		if (bActive)
		{
			TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices");
			for (int i = 0; i < Vertex->Num(); i++)
			{
				(*Vertex)[i][1] *= Scale;
			}
		}
		SetValue<DataType>(Context, InCollection, &Collection);
	}
}

void FSetCollectionAssetDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	DataType InCollection = GetValue<DataType>(Context, &Collection);

	if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
	{
		if (UGeometryCollection* CollectionAsset = Cast<UGeometryCollection>(EngineContext->Owner))
		{
			TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> NewCollection(InCollection.NewCopy<FGeometryCollection>());
			CollectionAsset->SetGeometryCollection(NewCollection);
			CollectionAsset->InvalidateCollection();
		}
	}
}

void FResetGeometryCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		SetValue<DataType>(Context, Collection, &Collection); // prime to avoid ensure

		if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
		{
			if (UGeometryCollection* GeometryCollectionObject = Cast<UGeometryCollection>(EngineContext->Owner))
			{
				GeometryCollectionObject->Reset();

				const UObject* Owner = EngineContext->Owner;
				FName AName("GeometrySource");
				if (Owner && Owner->GetClass())
				{
					if (const ::FProperty* UEProperty = Owner->GetClass()->FindPropertyByName(AName))
					{
						if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(UEProperty))
						{
							FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Owner);
							const int32 ArraySize = ArrayHelper.Num();
							for (int32 Index = 0; Index < ArraySize; ++Index)
							{
								if (FGeometryCollectionSource* SourceObject = (FGeometryCollectionSource*)(ArrayHelper.GetRawPtr(Index)))
								{
									if (UObject* ResolvedObject = SourceObject->SourceGeometryObject.ResolveObject())
									{
										if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ResolvedObject))
										{
											TArray<UMaterialInterface*> Materials;
											Materials.Reserve(StaticMesh->GetStaticMaterials().Num());

											for (int32 Index2 = 0; Index2 < StaticMesh->GetStaticMaterials().Num(); ++Index2)
											{
												UMaterialInterface* CurrMaterial = StaticMesh->GetMaterial(Index2);
												Materials.Add(CurrMaterial);
											}

											// Geometry collections usually carry the selection material, which we'll delete before appending
											UMaterialInterface* BoneSelectedMaterial = LoadObject<UMaterialInterface>(nullptr, UGeometryCollection::GetSelectedMaterialPath(), nullptr, LOAD_None, nullptr);
											GeometryCollectionObject->Materials.Remove(BoneSelectedMaterial);
											Materials.Remove(BoneSelectedMaterial);

											FGeometryCollectionEngineConversion::AppendStaticMesh(StaticMesh, Materials, FTransform(), GeometryCollectionObject);

										}
									}
								}
							}
						}
					}
				}
				GeometryCollectionObject->UpdateConvexGeometry();
				GeometryCollectionObject->InitializeMaterials();
				GeometryCollectionObject->InvalidateCollection();

				if (const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> AssetCollection = GeometryCollectionObject->GetGeometryCollection())
				{
					SetValue<DataType>(Context, *AssetCollection, &Collection);
				}
			}
		}
	}
}

void FPrintStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FString Value = GetValue<FString>(Context, &String);

	if (PrintToScreen)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Value);
	}
	if (PrintToLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("Text, %s"), *Value);
	}
}

void FLogStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (PrintToLog)
	{
		FString Value = GetValue<FString>(Context, &String);
		UE_LOG(LogTemp, Warning, TEXT("Text, %s"), *Value);
	}
}

void FMakeLiteralStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		SetValue<FString>(Context, Value, &String);
	}
}

void ComputeBoundingBox(FBoundingBoxDataflowNode::DataType& Collection, FBox& BoundingBox)
{
	if (Collection.HasAttribute("Transform", "Transform") &&
		Collection.HasAttribute("Parent", "Transform") &&
		Collection.HasAttribute("TransformIndex", "Geometry") &&
		Collection.HasAttribute("BoundingBox", "Geometry"))
	{
		const TManagedArray<FTransform>& Transforms = Collection.GetAttribute<FTransform>("Transform", "Transform");
		const TManagedArray<int32>& ParentIndices = Collection.GetAttribute<int32>("Parent", "Transform");
		const TManagedArray<int32>& TransformIndices = Collection.GetAttribute<int32>("TransformIndex", "Geometry");
		const TManagedArray<FBox>& BoundingBoxes = Collection.GetAttribute<FBox>("BoundingBox", "Geometry");

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
	}
}

void FBoundingBoxDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FBox>(&BoundingBox))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		FBox BBox(ForceInit);

		ComputeBoundingBox(InCollection, BBox);

		SetValue<FBox>(Context, BBox, &BoundingBox);
	}
}

void FExpandBoundingBoxDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FBox BBox = GetValue<FBox>(Context, &BoundingBox);

	if (Out->IsA<FVector>(&Min))
	{
		SetValue<FVector>(Context, BBox.Min, &Min);
	}
	else if (Out->IsA<FVector>(&Max))
	{
		SetValue<FVector>(Context, BBox.Max, &Max);
	}
	else if (Out->IsA<FVector>(&Center))
	{
		SetValue<FVector>(Context, BBox.GetCenter(), &Center);
	}
	else if (Out->IsA<FVector>(&HalfExtents))
	{
		SetValue<FVector>(Context, BBox.GetExtent(), &HalfExtents);
	}
	else if (Out->IsA<float>(&Volume))
	{
		SetValue<float>(Context, BBox.GetVolume(), &Volume);
	}
}

void FVectorToStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		FString Value = GetValue<FVector>(Context, &Vector).ToString();
		SetValue<FString>(Context, Value, &String);
	}
}

void FFloatToStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		FString Value = FString::Printf(TEXT("%f"), GetValue<float>(Context, &Float));
		SetValue<FString>(Context, Value, &String);
	}
}

void FMakePointsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&Points))
	{
		SetValue<TArray<FVector>>(Context, Point, &Points);
	}
}

void FMakeBoxDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FBox>(&Box))
	{
		if (DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax)
		{
			FVector MinVal = GetValue<FVector>(Context, &Min);
			FVector MaxVal = GetValue<FVector>(Context, &Max);

			SetValue<FBox>(Context, FBox(MinVal, MaxVal), &Box);
		}
		else if (DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_CenterSize)
		{
			FVector CenterVal = GetValue<FVector>(Context, &Center);
			FVector SizeVal = GetValue<FVector>(Context, &Size);

			SetValue<FBox>(Context, FBox(CenterVal - 0.5 * SizeVal, CenterVal + 0.5 * SizeVal), &Box);
		}
	}
}

void FUniformScatterPointsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&Points))
	{
		FBox BBox = GetValue<FBox>(Context, &BoundingBox);
		if (BBox.GetVolume() > 0.f)
		{
			FRandomStream RandStream(GetValue<float>(Context, &RandomSeed));

			const FVector Extent(BBox.Max - BBox.Min);
			const int32 NumPoints = RandStream.RandRange(GetValue<int32>(Context, &MinNumberOfPoints), GetValue<int32>(Context, &MaxNumberOfPoints));

			TArray<FVector> PointsArr;
			PointsArr.Reserve(NumPoints);
			for (int32 idx = 0; idx < NumPoints; ++idx)
			{
				PointsArr.Emplace(BBox.Min + FVector(RandStream.FRand(), RandStream.FRand(), RandStream.FRand()) * Extent);
			}

			SetValue<TArray<FVector>>(Context, PointsArr, &Points);
		}
	}
}

void FRadialScatterPointsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&Points))
	{
		const FVector::FReal RadialStep = GetValue<float>(Context, &Radius) / GetValue<int32>(Context, &RadialSteps);
		const FVector::FReal AngularStep = 2 * PI / GetValue<int32>(Context, &AngularSteps);

		FRandomStream RandStream(GetValue<float>(Context, &RandomSeed));
		FVector UpVector(GetValue<FVector>(Context, &Normal));
		UpVector.Normalize();
		FVector BasisX, BasisY;
		UpVector.FindBestAxisVectors(BasisX, BasisY);

		TArray<FVector> PointsArr;

		FVector::FReal Len = RadialStep * .5;
		for (int32 ii = 0; ii < GetValue<int32>(Context, &RadialSteps); ++ii, Len += RadialStep)
		{
			FVector::FReal Angle = FMath::DegreesToRadians(GetValue<float>(Context, &AngleOffset));
			for (int32 kk = 0; kk < AngularSteps; ++kk, Angle += AngularStep)
			{
				FVector RotatingOffset = Len * (FMath::Cos(Angle) * BasisX + FMath::Sin(Angle) * BasisY);
				PointsArr.Emplace(GetValue<FVector>(Context, &Center) + RotatingOffset + (RandStream.VRand() * RandStream.FRand() * Variability));
			}
		}

		SetValue<TArray<FVector>>(Context, PointsArr, &Points);
	}
}

void FMakeLiteralFloatDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Float))
	{
		SetValue<float>(Context, Value, &Float);
	}
}

void FMakeLiteralIntDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&Int))
	{
		SetValue<int32>(Context, Value, &Int);
	}
}

void FMakeLiteralBoolDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<bool>(&Bool))
	{
		SetValue<bool>(Context, Value, &Bool);
	}
}

void FMakeLiteralVectorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&Vector))
	{
		SetValue<FVector>(Context, Value, &Vector);
	}
}

void FIntToStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		FString Value = FString::Printf(TEXT("%d"), GetValue<int32>(Context, &Int));
		SetValue<FString>(Context, Value, &String);
	}
}

void FBoolToStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		FString Value = FString::Printf(TEXT("%s"), GetValue<bool>(Context, &Bool) ? TEXT("true") : TEXT("false"));
		SetValue<FString>(Context, Value, &String);
	}
}

void FExpandVectorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FVector VectorVal = GetValue<FVector>(Context, &Vector);

	if (Out->IsA<float>(&X))
	{
		SetValue<float>(Context, VectorVal.X, &X);
	}
	else if (Out->IsA<float>(&Y))
	{
		SetValue<float>(Context, VectorVal.Y, &Y);
	}
	else if (Out->IsA<float>(&Z))
	{
		SetValue<float>(Context, VectorVal.Z, &Z);
	}
}

void FIntToFloatDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Float))
	{
		float Value = float(GetValue<int32>(Context, &Int));
		SetValue<float>(Context, Value, &Float);
	}
}

float GetMaxVertexMovement(float Grout, float Amplitude, int OctaveNumber, float Persistence)
{
	float MaxDisp = Grout;
	float AmplitudeScaled = Amplitude;
	for (int32 OctaveIdx = 0; OctaveIdx < OctaveNumber; OctaveIdx++, AmplitudeScaled *= Persistence)
	{
		MaxDisp += FMath::Abs(AmplitudeScaled);
	}
	return MaxDisp;
}

void FVoronoiFractureDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection CollectionPtr = GetValue<FManagedArrayCollection>(Context, &Collection);
		if (FGeometryCollection* GeomCollection = CollectionPtr.NewCopy<FGeometryCollection>())
		{
			TArray<FVector> Sites = GetValue<TArray<FVector>>(Context, &Points);
			if (Sites.Num() > 0)
			{
				//
				// Compute BoundingBox for ManagedArrayIn
				//
				FBox BoundingBox(ForceInit);

				if (GeomCollection->HasAttribute("Transform", "Transform") &&
					GeomCollection->HasAttribute("Parent", "Transform") &&
					GeomCollection->HasAttribute("TransformIndex", "Geometry") &&
					GeomCollection->HasAttribute("BoundingBox", "Geometry"))
				{
					const TManagedArray<FTransform>& Transforms = GeomCollection->GetAttribute<FTransform>("Transform", "Transform");
					const TManagedArray<int32>& ParentIndices = GeomCollection->GetAttribute<int32>("Parent", "Transform");
					const TManagedArray<int32>& TransformIndices = GeomCollection->GetAttribute<int32>("TransformIndex", "Geometry");
					const TManagedArray<FBox>& BoundingBoxes = GeomCollection->GetAttribute<FBox>("BoundingBox", "Geometry");

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
					VoronoiBounds += FBox(Sites);

					float GroutVal = GetValue<float>(Context, &Grout);
					float AmplitudeVal = GetValue<float>(Context, &Amplitude);
					int32 OctaveNumberVal = GetValue<int32>(Context, &OctaveNumber);
					float PersistenceVal = GetValue<float>(Context, &Persistence);

					VoronoiBounds = VoronoiBounds.ExpandBy(GetMaxVertexMovement(GroutVal, AmplitudeVal, OctaveNumberVal, PersistenceVal) + KINDA_SMALL_NUMBER);

					//
					// Voronoi Fracture
					//
					FNoiseSettings NoiseSettings;
					NoiseSettings.Amplitude = AmplitudeVal;
					NoiseSettings.Frequency = GetValue<float>(Context, &Frequency);
					NoiseSettings.Octaves = OctaveNumberVal;
					NoiseSettings.PointSpacing = GetValue<float>(Context, &PointSpacing);
					NoiseSettings.Lacunarity = GetValue<float>(Context, &Lacunarity);
					NoiseSettings.Persistence = GetValue<float>(Context, &Persistence);;

					FVoronoiDiagram Voronoi(Sites, VoronoiBounds, .1f);

					FPlanarCells VoronoiPlanarCells = FPlanarCells(Sites, Voronoi);
					VoronoiPlanarCells.InternalSurfaceMaterials.NoiseSettings = NoiseSettings;

					const TArrayView<const int32>& TransformIndicesArray(TransformIndices.GetConstArray());

					float CollisionSampleSpacingVal = GetValue<float>(Context, &CollisionSampleSpacing);
					float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

					int ResultGeometryIndex = CutMultipleWithPlanarCells(VoronoiPlanarCells, *GeomCollection, TransformIndicesArray, GroutVal, CollisionSampleSpacingVal, RandomSeedVal, FTransform().Identity);

					//					Out->SetValue<FManagedArrayCollection>(FManagedArrayCollection(*GeomCollection), Context);
					SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
				}
			}
		}
	}
}

void FStringAppendDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		const FString StringOut = GetValue<FString>(Context, &String1) + GetValue<FString>(Context, &String2);
		SetValue<FString>(Context, StringOut, &String);
	}
}

void FRandomFloatDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Float))
	{
		if (Deterministic)
		{
			float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

			FRandomStream Stream(RandomSeedVal);
			SetValue<float>(Context, Stream.FRand(), &Float);
		}
		else
		{
			SetValue<float>(Context, FMath::FRand(), &Float);
		}
	}
}

void FRandomFloatInRangeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Float))
	{
		float MinVal = GetValue<float>(Context, &Min);
		float MaxVal = GetValue<float>(Context, &Max);

		if (Deterministic)
		{
			float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

			FRandomStream Stream(RandomSeedVal);
			SetValue<float>(Context, Stream.FRandRange(MinVal, MaxVal), &Float);
		}
		else
		{
			SetValue<float>(Context, FMath::FRandRange(MinVal, MaxVal), &Float);
		}
	}
}

void FRandomUnitVectorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&Vector))
	{
		if (Deterministic)
		{
			float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

			FRandomStream Stream(RandomSeedVal);
			SetValue<FVector>(Context, Stream.VRand(), &Vector);
		}
		else
		{
			SetValue<FVector>(Context, FMath::VRand(), &Vector);
		}
	}
}

void FRandomUnitVectorInConeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&Vector))
	{
		FVector ConeDirectionVal = GetValue<FVector>(Context, &ConeDirection);
		float ConeHalfAngleVal = GetValue<float>(Context, &ConeHalfAngle);

		if (Deterministic)
		{
			float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

			FRandomStream Stream(RandomSeedVal);
			SetValue<FVector>(Context, Stream.VRandCone(ConeDirectionVal, ConeHalfAngleVal), &Vector);
		}
		else
		{
			SetValue<FVector>(Context, FMath::VRandCone(ConeDirectionVal, ConeHalfAngleVal), &Vector);
		}
	}
}

void FRadiansToDegreesDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Degrees))
	{
		SetValue<float>(Context, FMath::RadiansToDegrees(GetValue<float>(Context, &Radians)), &Degrees);
	}
}

void FDegreesToRadiansDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Radians))
	{
		SetValue<float>(Context, FMath::DegreesToRadians(GetValue<float>(Context, &Degrees)), &Radians);
	}
}

void AddAdditionalAttributesIfRequired(FGeometryCollection* GeometryCollection)
{
	FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(GeometryCollection, -1);
}

bool FExplodedViewDataflowNode::GetValidGeoCenter(FGeometryCollection* Collection, const TManagedArray<int32>& TransformToGeometryIndex, const TArray<FTransform>& Transforms, const TManagedArray<TSet<int32>>& Children, const TManagedArray<FBox>& BoundingBox, int32 TransformIndex, FVector& OutGeoCenter)
{
	if (Collection->IsRigid(TransformIndex))
	{
		OutGeoCenter = Transforms[TransformIndex].TransformPosition(BoundingBox[TransformToGeometryIndex[TransformIndex]].GetCenter());

		return true;
	}
	else if (Collection->SimulationType[TransformIndex] == FGeometryCollection::ESimulationTypes::FST_None) // ie this is embedded geometry
	{
		int32 Parent = Collection->Parent[TransformIndex];
		int32 ParentGeo = Parent != INDEX_NONE ? TransformToGeometryIndex[Parent] : INDEX_NONE;
		if (ensureMsgf(ParentGeo != INDEX_NONE, TEXT("Embedded geometry should always have a rigid geometry parent!  Geometry collection may be malformed.")))
		{
			OutGeoCenter = Transforms[Collection->Parent[TransformIndex]].TransformPosition(BoundingBox[ParentGeo].GetCenter());
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

			if (GetValidGeoCenter(Collection, TransformToGeometryIndex, Transforms, Children, BoundingBox, ChildIndex, OutGeoCenter))
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

void FExplodedViewDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection CollectionPtr = GetValue<FManagedArrayCollection>(Context, &Collection);
		if (FGeometryCollection* GeomCollection = CollectionPtr.NewCopy<FGeometryCollection>())
		{
			GeomCollection->AddAttribute<FVector3f>("ExplodedVector", FGeometryCollection::TransformGroup, FManagedArrayCollection::FConstructionParameters(FName()));
			check(GeomCollection->HasAttribute("ExplodedVector", FGeometryCollection::TransformGroup));

			TManagedArray<FVector3f>& ExplodedVectors = GeomCollection->ModifyAttribute<FVector3f>("ExplodedVector", FGeometryCollection::TransformGroup);
			const TManagedArray<FTransform>& Transform = GeomCollection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
			const TManagedArray<int32>& TransformToGeometryIndex = GeomCollection->GetAttribute<int32>("TransformToGeometryIndex", FGeometryCollection::TransformGroup);
			const TManagedArray<FBox>& BoundingBox = GeomCollection->GetAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup);

			// Make sure we have valid "Level"
			AddAdditionalAttributesIfRequired(GeomCollection);

			const TManagedArray<int32>& Levels = GeomCollection->GetAttribute<int32>("Level", FTransformCollection::TransformGroup);
			const TManagedArray<int32>& Parent = GeomCollection->GetAttribute<int32>("Parent", FTransformCollection::TransformGroup);
			const TManagedArray<TSet<int32>>& Children = GeomCollection->GetAttribute<TSet<int32>>("Children", FGeometryCollection::TransformGroup);

			int32 ViewFractureLevel = -1;
			int32 MaxFractureLevel = ViewFractureLevel;
			for (int32 Idx = 0, ni = Transform.Num(); Idx < ni; ++Idx)
			{
				if (Levels[Idx] > MaxFractureLevel)
					MaxFractureLevel = Levels[Idx];
			}

			TArray<FTransform> Transforms;
			GeometryCollectionAlgo::GlobalMatrices(Transform, GeomCollection->Parent, Transforms);

			TArray<FVector> TransformedCenters;
			TransformedCenters.SetNumUninitialized(Transforms.Num());

			int32 TransformsCount = 0;

			FVector Center(ForceInitToZero);
			for (int32 Idx = 0, ni = Transform.Num(); Idx < ni; ++Idx)
			{
				ExplodedVectors[Idx] = FVector3f::ZeroVector;
				FVector GeoCenter;

				if (GetValidGeoCenter(GeomCollection, TransformToGeometryIndex, Transforms, Children, BoundingBox, Idx, GeoCenter))
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
				for (int32 Idx = 0, ni = Transforms.Num(); Idx < ni; ++Idx)
				{
					if ((ViewFractureLevel < 0) || Levels[Idx] == ViewFractureLevel)
					{
						FVector ScaleVal = GetValue<FVector>(Context, &Scale);
						float UniformScaleVal = GetValue<float>(Context, &UniformScale);

						FVector ScaleVec = ScaleVal * UniformScaleVal;
						ExplodedVectors[Idx] = (FVector3f)(TransformedCenters[Idx] - Center) * (FVector3f)ScaleVec;
					}
					else
					{
						if (Parent[Idx] > -1)
						{
							ExplodedVectors[Idx] = ExplodedVectors[Parent[Idx]];
						}
					}
				}
			}

			SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
		}
	}
}

void FCreateNonOverlappingConvexHullsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection CollectionPtr = GetValue<FManagedArrayCollection>(Context, &Collection);
		if (FGeometryCollection* GeomCollection = CollectionPtr.NewCopy<FGeometryCollection>())
		{
			float CanRemoveFractionVal = GetValue<float>(Context, &CanRemoveFraction);
			float CanExceedFractionVal = GetValue<float>(Context, &CanExceedFraction);
			float SimplificationDistanceThresholdVal = GetValue<float>(Context, &SimplificationDistanceThreshold);

			FGeometryCollectionConvexUtility::FGeometryCollectionConvexData ConvexData = FGeometryCollectionConvexUtility::CreateNonOverlappingConvexHullData(GeomCollection, CanRemoveFractionVal, SimplificationDistanceThresholdVal, CanExceedFractionVal);

			SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
		}
	}
}

void FPlaneCutterDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection CollectionPtr = GetValue<FManagedArrayCollection>(Context, &Collection);
		if (FGeometryCollection* GeomCollection = CollectionPtr.NewCopy<FGeometryCollection>())
		{
			TArray<FPlane> CuttingPlanes;
			TArray<FTransform> CuttingPlaneTransforms;

			float RandomSeedVal = GetValue<float>(Context, &RandomSeed);
			FRandomStream RandStream(RandomSeedVal);

			FBox Bounds = GetValue<FBox>(Context, &BoundingBox);
			const FVector Extent(Bounds.Max - Bounds.Min);

			CuttingPlaneTransforms.Reserve(CuttingPlaneTransforms.Num() + NumPlanes);
			for (int32 ii = 0; ii < NumPlanes; ++ii)
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

			float AmplitudeVal = GetValue<float>(Context, &Amplitude);
			if (AmplitudeVal > 0.0f)
			{
				NoiseSettings.Amplitude = AmplitudeVal;
				NoiseSettings.Frequency = GetValue<float>(Context, &Frequency);
				NoiseSettings.Lacunarity = GetValue<float>(Context, &Lacunarity);
				NoiseSettings.Persistence = GetValue<float>(Context, &Persistence);
				NoiseSettings.Octaves = GetValue<int32>(Context, &OctaveNumber);
				NoiseSettings.PointSpacing = GetValue<float>(Context, &PointSpacing);

				InternalSurfaceMaterials.NoiseSettings = NoiseSettings;
			}

			if (GeomCollection->HasAttribute("TransformIndex", "Geometry"))
			{
				const TManagedArray<int32>& TransformIndices = GeomCollection->GetAttribute<int32>("TransformIndex", "Geometry");
				const TArrayView<const int32>& TransformIndicesArray(TransformIndices.GetConstArray());

				float CollisionSampleSpacingVal = GetValue<float>(Context, &CollisionSampleSpacing);
				float GroutVal = GetValue<float>(Context, &Grout);

				int ResultGeometryIndex = CutMultipleWithMultiplePlanes(CuttingPlanes, InternalSurfaceMaterials, *GeomCollection, TransformIndicesArray, GroutVal, CollisionSampleSpacingVal, RandomSeedVal, FTransform().Identity);

				SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
			}
		}
	}
}

void FHashStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&Hash))
	{
		SetValue<int32>(Context, GetTypeHash(GetValue<FString>(Context, &String)), &Hash);
	}
}

void FHashVectorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&Hash))
	{
		SetValue<int32>(Context, GetTypeHash(GetValue<FVector>(Context, &Vector)), &Hash);
	}
}

void FFloatToIntDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&Int))
	{
		float FloatVal = GetValue<float>(Context, &Float);
		if (Function == EFloatToIntFunctionEnum::Dataflow_FloatToInt_Function_Floor)
		{
			SetValue<int32>(Context, FMath::FloorToInt32(FloatVal), &Int);
		}
		else if (Function == EFloatToIntFunctionEnum::Dataflow_FloatToInt_Function_Ceil)
		{
			SetValue<int32>(Context, FMath::CeilToInt32(FloatVal), &Int);
		}
		else if (Function == EFloatToIntFunctionEnum::Dataflow_FloatToInt_Function_Round)
		{
			SetValue<int32>(Context, FMath::RoundToInt32(FloatVal), &Int);
		}
		else if (Function == EFloatToIntFunctionEnum::Dataflow_FloatToInt_Function_Truncate)
		{
			SetValue<int32>(Context, int32(FMath::TruncToFloat(FloatVal)), &Int);
		}
	}
}

void FMathConstantsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Float))
	{
		if (Constant == EMathConstantsEnum::Dataflow_MathConstants_Pi)
		{
			SetValue<float>(Context, FMathf::Pi, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_HalfPi)
		{
			SetValue<float>(Context, FMathf::HalfPi, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_TwoPi)
		{
			SetValue<float>(Context, FMathf::TwoPi, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_FourPi)
		{
			SetValue<float>(Context, FMathf::FourPi, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_InvPi)
		{
			SetValue<float>(Context, FMathf::InvPi, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_InvTwoPi)
		{
			SetValue<float>(Context, FMathf::InvTwoPi, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_Sqrt2)
		{
			SetValue<float>(Context, FMathf::Sqrt2, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_InvSqrt2)
		{
			SetValue<float>(Context, FMathf::InvSqrt2, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_Sqrt3)
		{
			SetValue<float>(Context, FMathf::Sqrt3, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_InvSqrt3)
		{
			SetValue<float>(Context, FMathf::InvSqrt3, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_FloatToInt_Function_E)
		{
			SetValue<float>(Context, 2.71828182845904523536f, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_FloatToInt_Function_Gamma)
		{
			SetValue<float>(Context, 0.577215664901532860606512090082f, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_FloatToInt_Function_GoldenRatio)
		{
			SetValue<float>(Context, 1.618033988749894f, &Float);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_FloatToInt_Function_ZeroTolerance)
		{
			SetValue<float>(Context, FMathf::ZeroTolerance, &Float);
		}
	}
}


