// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionNodes.h"
#include "Dataflow/DataflowCore.h"

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/Facades/CollectionMeshFacade.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineUtility.h"
#include "GeometryCollection/GeometryCollectionEngineRemoval.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "UObject/UnrealTypePrivate.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "StaticMeshAttributes.h"
#include "DynamicMeshEditor.h"
#include "Operations/MeshBoolean.h"
#include "Materials/Material.h"

#include "EngineGlobals.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "Voronoi/Voronoi.h"
#include "PlanarCut.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"
#include "FractureEngineClustering.h"
#include "FractureEngineSelection.h"
#include "GeometryCollection/Facades/CollectionBoundsFacade.h"
#include "GeometryCollection/Facades/CollectionAnchoringFacade.h"
#include "GeometryCollection/Facades/CollectionRemoveOnBreakFacade.h"
#include "GeometryCollection/Facades/CollectionTransformFacade.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/DynamicMesh3.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionNodes)

namespace Dataflow
{
	void GeometryCollectionEngineNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetCollectionFromAssetDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAppendCollectionAssetsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FPrintStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FLogStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBoundingBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExpandBoundingBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVectorToStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatToStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakePointsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeSphereDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralIntDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralBoolDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FIntToStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBoolToStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExpandVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FIntToFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FStringAppendDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomFloatInRangeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomUnitVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomUnitVectorInConeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRadiansToDegreesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDegreesToRadiansDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FHashStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FHashVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatToIntDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatArrayToIntArrayDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMathConstantsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetArrayElementDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetNumArrayElementsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetBoundingBoxesFromCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetCentroidsFromCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTransformCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBakeTransformsInCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTransformMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCompareIntDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBranchDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetSchemaDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRemoveOnBreakDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetAnchorStateDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FProximityDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionSetPivotDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAddCustomCollectionAttributeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetNumElementsInCollectionGroupDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetCollectionAttributeDataTypedDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetCollectionAttributeDataTypedDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBoolArrayToFaceSelectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatArrayToVertexSelectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexColorInCollectionFromVertexSelectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeTransformDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexColorInCollectionFromFloatArrayDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatArrayNormalizeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVectorArrayNormalizeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeQuaternionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMultiplyTransformDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FInvertTransformDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSelectionToVertexListDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FUnionIntArraysDataflowNode);

		// GeometryCollection
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("GeometryCollection", FLinearColor(0.55f, 0.45f, 1.0f), CDefaultNodeBodyTintColor);
		// Development
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Development", FLinearColor(1.f, 0.f, 0.f), CDefaultNodeBodyTintColor);
		// Utilities|String
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Utilities|String", FLinearColor(0.5f, 0.f, 0.5f), CDefaultNodeBodyTintColor);
		// Fracture
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Fracture", FLinearColor(1.f, 1.f, 0.8f), CDefaultNodeBodyTintColor);
		// Utilities
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Utilities", FLinearColor(1.f, 1.f, 0.f), CDefaultNodeBodyTintColor);
		// Math
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Math", FLinearColor(0.f, 0.4f, 0.8f), CDefaultNodeBodyTintColor);
		// Generators
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Generators", FLinearColor(.4f, 0.8f, 0.f), CDefaultNodeBodyTintColor);
	}
}

void FGetCollectionFromAssetDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		if (CollectionAsset)
		{
			if (const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> AssetCollection = CollectionAsset->GetGeometryCollection())
			{
				SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*AssetCollection), &Collection);
			}
			else
			{
				SetValue<FManagedArrayCollection>(Context, FManagedArrayCollection(), &Collection);
			}
		}
		else
		{
			SetValue<FManagedArrayCollection>(Context, FManagedArrayCollection(), &Collection);
		}
	}
}


void FAppendCollectionAssetsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection1))
	{
		FManagedArrayCollection InCollection1 = GetValue<DataType>(Context, &Collection1);
		const FManagedArrayCollection& InCollection2 = GetValue<DataType>(Context, &Collection2);
		TArray<FString> GeometryGroupGuidsLocal1, GeometryGroupGuidsLocal2;
		if (const TManagedArray<FString>* GuidArray1 = InCollection1.FindAttribute<FString>("Guid", FGeometryCollection::GeometryGroup))
		{
			GeometryGroupGuidsLocal1 = GuidArray1->GetConstArray();
		}
		InCollection1.Append(InCollection2);
		SetValue<DataType>(Context, InCollection1, &Collection1);
		if (const TManagedArray<FString>* GuidArray2 = InCollection2.FindAttribute<FString>("Guid", FGeometryCollection::GeometryGroup))
		{
			GeometryGroupGuidsLocal2 = GuidArray2->GetConstArray();
		}
		SetValue<TArray<FString>>(Context, GeometryGroupGuidsLocal1, &GeometryGroupGuidsOut1);
		SetValue<TArray<FString>>(Context, GeometryGroupGuidsLocal2, &GeometryGroupGuidsOut2);
	}
}


void FPrintStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FString Value = GetValue<FString>(Context, &String);

	if (bPrintToScreen)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Value);
	}
	if (bPrintToLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("Text, %s"), *Value);
	}
}

void FLogStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (bPrintToLog)
	{
		FString Value = GetValue<FString>(Context, &String);
		UE_LOG(LogTemp, Warning, TEXT("[Dataflow Log] %s"), *Value);
	}
}

void FMakeLiteralStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		SetValue<FString>(Context, Value, &String);
	}
}


void FBoundingBoxDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FBox>(&BoundingBox))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FBoundsFacade BoundsFacade(InCollection);
		const FBox& BoundingBoxInCollectionSpace = BoundsFacade.GetBoundingBoxInCollectionSpace();

		SetValue<FBox>(Context, BoundingBoxInCollectionSpace, &BoundingBox);
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


void FMakeSphereDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FSphere>(&Sphere))
	{
		FVector CenterVal = GetValue<FVector>(Context, &Center);
		float RadiusVal = GetValue<float>(Context, &Radius);

		SetValue<FSphere>(Context, FSphere(CenterVal, RadiusVal), &Sphere);
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
		FVector Value(GetValue<float>(Context, &X, X), GetValue<float>(Context, &Y, Y), GetValue<float>(Context, &Z, Z));
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
		if (bDeterministic)
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

		if (bDeterministic)
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
		if (bDeterministic)
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

		if (bDeterministic)
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

void FFloatArrayToIntArrayDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<int32>>(&IntArray))
	{
		TArray<float> FloatVal = GetValue<TArray<float>>(Context, &FloatArray);
		TArray<int32> RetVal; RetVal.SetNumUninitialized(FloatVal.Num());
		if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_Function_Floor)
		{
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				RetVal[i] = FMath::FloorToInt32(FloatVal[i]);
			}
		}
		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_Function_Ceil)
		{
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				RetVal[i] = FMath::CeilToInt32(FloatVal[i]);
			}
		}
		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_Function_Round)
		{
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				RetVal[i] = FMath::RoundToInt32(FloatVal[i]);
			}
		}
		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_Function_Truncate)
		{
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				RetVal[i] = int32(FMath::TruncToFloat(FloatVal[i]));
			}
		}

		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_NonZeroToIndex)
		{
			int32 RetValIndex = 0;
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				if (FloatVal[i] != 0.0)
				{
					RetVal[RetValIndex] = i;
					RetValIndex++;
				}
			}
			RetVal.SetNum(RetValIndex);
		}
		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_ZeroToIndex)
		{
			int32 RetValIndex = 0;
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				if (FloatVal[i] == 0.0)
				{
					RetVal[RetValIndex] = i;
					RetValIndex++;
				}
			}
			RetVal.SetNum(RetValIndex);
		}

		SetValue<TArray<int32>>(Context, RetVal, &IntArray);
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

void FGetArrayElementDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&Point))
	{
		const TArray<FVector>& Array = GetValue<TArray<FVector>>(Context, &Points);
		if (Array.Num() > 0 && Index >= 0 && Index < Array.Num())
		{
			SetValue<FVector>(Context, Array[Index], &Point);
			return;
		}

		SetValue<FVector>(Context, FVector(0.f), &Point);
	}
}

void FGetNumArrayElementsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&NumElements))
	{
		if (IsConnected<TArray<FVector>>(&Points))
		{
			SetValue<int32>(Context, GetValue<TArray<FVector>>(Context, &Points).Num(), &NumElements);
			return;
		}
		else if (IsConnected<TArray<FVector3f>>(&Vector3fArray))
		{
			SetValue<int32>(Context, GetValue<TArray<FVector3f>>(Context, &Vector3fArray).Num(), &NumElements);
			return;
		}

		SetValue<int32>(Context, 0, &NumElements);
	}
}

void FGetBoundingBoxesFromCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FBox>>(&BoundingBoxes))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		GeometryCollection::Facades::FBoundsFacade BoundsFacade(InCollection);
		const TManagedArray<FBox>& InBoundingBoxes = BoundsFacade.GetBoundingBoxes();

		GeometryCollection::Facades::FCollectionTransformFacade TransformFacade(InCollection);

		TArray<FBox> BoundingBoxesArr;
		for (int32 Idx = 0; Idx < InBoundingBoxes.Num(); ++Idx)
		{
			const FBox BoundingBoxInBoneSpace = InBoundingBoxes[Idx];

			// Transform from BoneSpace to CollectionSpace
			const FTransform CollectionSpaceTransform = TransformFacade.ComputeCollectionSpaceTransform(Idx);
			const FBox BoundingBoxInCollectionSpace = BoundingBoxInBoneSpace.TransformBy(CollectionSpaceTransform);

			if (IsConnected<FDataflowTransformSelection>(&TransformSelection))
			{
				if (InTransformSelection.IsSelected(Idx))
				{
					BoundingBoxesArr.Add(BoundingBoxInCollectionSpace);
				}
			}
			else
			{
				BoundingBoxesArr.Add(BoundingBoxInCollectionSpace);
			}

		}

		SetValue<TArray<FBox>>(Context, BoundingBoxesArr, &BoundingBoxes);
	}
}

void FGetCentroidsFromCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&Centroids))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		GeometryCollection::Facades::FBoundsFacade BoundsFacade(InCollection);
		const TArray<FVector>& InCentroids = BoundsFacade.GetCentroids();

		GeometryCollection::Facades::FCollectionTransformFacade TransformFacade(InCollection);

		TArray<FVector> CentroidsArr;
		for (int32 Idx = 0; Idx < InCentroids.Num(); ++Idx)
		{
			const FVector PositionInBoneSpace(InCentroids[Idx]);

			// Transform from BoneSpace to CollectionSpace
			const FTransform CollectionSpaceTransform = TransformFacade.ComputeCollectionSpaceTransform(Idx);
			const FVector PositionInCollectionSpace = CollectionSpaceTransform.TransformPosition(PositionInBoneSpace);

			if (IsConnected<FDataflowTransformSelection>(&TransformSelection))
			{
				if (InTransformSelection.IsSelected(Idx))
				{
					CentroidsArr.Add(PositionInCollectionSpace);
				}
			}
			else
			{
				CentroidsArr.Add(PositionInCollectionSpace);
			}
		}

		SetValue<TArray<FVector>>(Context, CentroidsArr, &Centroids);
	}
}


void FTransformCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		FTransform NewTransform = GeometryCollection::Facades::FCollectionTransformFacade::BuildTransform(Translate,
			(uint8)RotationOrder,
			Rotate,
			Scale,
			UniformScale,
			RotatePivot,
			ScalePivot,
			bInvertTransformation);

		GeometryCollection::Facades::FCollectionTransformFacade TransformFacade(InCollection);
		TransformFacade.Transform(NewTransform);

		SetValue<FManagedArrayCollection>(Context, InCollection, &Collection);
	}
}


void FBakeTransformsInCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FCollectionTransformFacade TransformFacade(InCollection);
		const TArray<FTransform>& CollectionSpaceTransforms = TransformFacade.ComputeCollectionSpaceTransforms();

		GeometryCollection::Facades::FCollectionMeshFacade MeshFacade(InCollection);

		const int32 NumTransforms = InCollection.NumElements(FGeometryCollection::TransformGroup);

		for (int32 TransformIdx = 0; TransformIdx < NumTransforms; ++TransformIdx)
		{
			MeshFacade.BakeTransform(TransformIdx, CollectionSpaceTransforms[TransformIdx]);
			TransformFacade.SetBoneTransformToIdentity(TransformIdx);
		}

		SetValue<FManagedArrayCollection>(Context, InCollection, &Collection);
	}
}


void FTransformMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh))
	{
		if (TObjectPtr<const UDynamicMesh> InMesh = GetValue<TObjectPtr<UDynamicMesh>>(Context, &Mesh))
		{
			// Creating a new mesh object from InMesh
			TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
			NewMesh->SetMesh(InMesh->GetMeshRef());

			FTransform NewTransform = GeometryCollection::Facades::FCollectionTransformFacade::BuildTransform(Translate,
				(uint8)RotationOrder,
				Rotate,
				Scale,
				UniformScale,
				RotatePivot,
				ScalePivot,
				bInvertTransformation);

			UE::Geometry::FDynamicMesh3& DynamicMesh = NewMesh->GetMeshRef();

			MeshTransforms::ApplyTransform(DynamicMesh, UE::Geometry::FTransformSRT3d(NewTransform), true);

			SetValue<TObjectPtr<UDynamicMesh>>(Context, NewMesh, &Mesh);
		}
		else
		{
			SetValue<TObjectPtr<UDynamicMesh>>(Context, NewObject<UDynamicMesh>(), &Mesh);
		}
	}
}


void FCompareIntDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<bool>(&Result))
	{
		int32 IntAValue = GetValue<int32>(Context, &IntA);
		int32 IntBValue = GetValue<int32>(Context, &IntB);
		bool ResultValue;

		if (Operation == ECompareOperationEnum::Dataflow_Compare_Equal)
		{
			ResultValue = IntAValue == IntBValue ? true : false;
		}
		else if (Operation == ECompareOperationEnum::Dataflow_Compare_Smaller)
		{
			ResultValue = IntAValue < IntBValue ? true : false;
		}
		else if (Operation == ECompareOperationEnum::Dataflow_Compare_SmallerOrEqual)
		{
			ResultValue = IntAValue <= IntBValue ? true : false;
		}
		else if (Operation == ECompareOperationEnum::Dataflow_Compare_Greater)
		{
			ResultValue = IntAValue > IntBValue ? true : false;
		}
		else if (Operation == ECompareOperationEnum::Dataflow_Compare_GreaterOrEqual)
		{
			ResultValue = IntAValue >= IntBValue ? true : false;
		}

		SetValue<bool>(Context, ResultValue, &Result);
	}
}

void FBranchDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh))
	{
		bool InCondition = GetValue<bool>(Context, &bCondition);

		if (InCondition)
		{
			if (TObjectPtr<UDynamicMesh> InMeshA = GetValue<TObjectPtr<UDynamicMesh>>(Context, &MeshA))
			{
				SetValue<TObjectPtr<UDynamicMesh>>(Context, InMeshA, &Mesh);

				return;
			}
		}
		else
		{
			if (TObjectPtr<UDynamicMesh> InMeshB = GetValue<TObjectPtr<UDynamicMesh>>(Context, &MeshB))
			{
				SetValue<TObjectPtr<UDynamicMesh>>(Context, InMeshB, &Mesh);

				return;
			}
		}

		SetValue<TObjectPtr<UDynamicMesh>>(Context, NewObject<UDynamicMesh>(), &Mesh);
	}
}


namespace {
	inline FName GetArrayTypeString(FManagedArrayCollection::EArrayType ArrayType)
	{
		switch (ArrayType)
		{
#define MANAGED_ARRAY_TYPE(a,A)	case EManagedArrayType::F##A##Type:\
			return FName(#A);
#include "GeometryCollection/ManagedArrayTypeValues.inl"
#undef MANAGED_ARRAY_TYPE
		}
		return FName();
	}
}

void FGetSchemaDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		FString OutputStr;
		OutputStr.Appendf(TEXT("\n----------------------------------------\n"));
		for (auto& Group : InCollection.GroupNames())
		{
			if (InCollection.HasGroup(Group))
			{
				int32 NumElems = InCollection.NumElements(Group);

				OutputStr.Appendf(TEXT("Group: %s  Number of Elements: %d\n"), *Group.ToString(), NumElems);
				OutputStr.Appendf(TEXT("Attributes:\n"));

				for (auto& Attr : InCollection.AttributeNames(Group))
				{
					if (InCollection.HasAttribute(Attr, Group))
					{
						FString TypeStr = GetArrayTypeString(InCollection.GetAttributeType(Attr, Group)).ToString();
						OutputStr.Appendf(TEXT("\t%s\t[%s]\n"), *Attr.ToString(), *TypeStr);
					}
				}

				OutputStr.Appendf(TEXT("\n--------------------\n"));
			}
		}
		OutputStr.Appendf(TEXT("----------------------------------------\n"));

		SetValue<FString>(Context, OutputStr, &String);
	}
}


void FRemoveOnBreakDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const bool& InEnableRemoval = GetValue(Context, &bEnabledRemoval, true);
		const FVector2f& InPostBreakTimer = GetValue(Context, &PostBreakTimer);
		const FVector2f& InRemovalTimer = GetValue(Context, &RemovalTimer);
		const bool& InClusterCrumbling = GetValue(Context, &bClusterCrumbling);

		// we are making a copy of the collection because we are modifying it 
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FCollectionRemoveOnBreakFacade RemoveOnBreakFacade(InCollection);
		RemoveOnBreakFacade.DefineSchema();

		GeometryCollection::Facades::FRemoveOnBreakData Data;
		Data.SetBreakTimer(InPostBreakTimer.X, InPostBreakTimer.Y);
		Data.SetRemovalTimer(InRemovalTimer.X, InRemovalTimer.Y);
		Data.SetEnabled(InEnableRemoval);
		Data.SetClusterCrumbling(InClusterCrumbling);

		// selection is optional
		if (IsConnected<FDataflowTransformSelection>(&TransformSelection))
		{
			const FDataflowTransformSelection& InTransformSelection = GetValue(Context, &TransformSelection);
			TArray<int32> TransformIndices;
			InTransformSelection.AsArray(TransformIndices);
			RemoveOnBreakFacade.SetFromIndexArray(TransformIndices, Data);
		}
		else
		{
			RemoveOnBreakFacade.SetToAll(Data);
		}

		// move the collection to the output to avoid making another copy
		SetValue<FManagedArrayCollection>(Context, MoveTemp(InCollection), &Collection);
	}
}

void FSetAnchorStateDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowTransformSelection InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			Chaos::Facades::FCollectionAnchoringFacade AnchoringFacade(*GeomCollection);
			if (!AnchoringFacade.HasAnchoredAttribute())
			{
				AnchoringFacade.AddAnchoredAttribute();
			}

			bool bAnchored = (AnchorState == EAnchorStateEnum::Dataflow_AnchorState_Anchored) ? true : false;
			TArray<int32> BoneIndices;
			InTransformSelection.AsArray(BoneIndices);
			AnchoringFacade.SetAnchored(BoneIndices, bAnchored);

			if (bSetNotSelectedBonesToOppositeState)
			{
				InTransformSelection.Invert();
				InTransformSelection.AsArray(BoneIndices);
				AnchoringFacade.SetAnchored(BoneIndices, !bAnchored);
			}

			SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
		}
	}
}


void FProximityDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			FGeometryCollectionProximityPropertiesInterface::FProximityProperties Properties = GeomCollection->GetProximityProperties();

			Properties.Method = (EProximityMethod)ProximityMethod;
			Properties.DistanceThreshold = DistanceThreshold;
			Properties.bUseAsConnectionGraph = bUseAsConnectionGraph;
			Properties.RequireContactAmount = ContactThreshold;

			GeomCollection->SetProximityProperties(Properties);

			// Invalidate proximity
			FGeometryCollectionProximityUtility ProximityUtility(GeomCollection.Get());
			ProximityUtility.InvalidateProximity();
			ProximityUtility.UpdateProximity();

			SetValue<FManagedArrayCollection>(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
		}
	}
}


void FCollectionSetPivotDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const FTransform& InTransform = GetValue<FTransform>(Context, &Transform);

		GeometryCollection::Facades::FCollectionTransformFacade TransformFacade(InCollection);
		TransformFacade.SetPivot(InTransform);

		SetValue<FManagedArrayCollection>(Context, InCollection, &Collection);
	}
}


static FName GetGroupName(const EStandardGroupNameEnum& InGroupName)
{
	FName GroupNameToUse;
	if (InGroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Transform)
	{
		GroupNameToUse = FGeometryCollection::TransformGroup;
	}
	else if (InGroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Geometry)
	{
		GroupNameToUse = FGeometryCollection::GeometryGroup;
	}
	else if (InGroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Faces)
	{
		GroupNameToUse = FGeometryCollection::FacesGroup;
	}
	else if (InGroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Vertices)
	{
		GroupNameToUse = FGeometryCollection::VerticesGroup;
	}
	else if (InGroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Material)
	{
		GroupNameToUse = FGeometryCollection::MaterialGroup;
	}
	else if (InGroupName == EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Breaking)
	{
		GroupNameToUse = FGeometryCollection::BreakingGroup;
	}

	return GroupNameToUse;
}


template<typename T>
static void AddAndFillAttribute(FManagedArrayCollection& InCollection, FName AttributeName, FName GroupName, const T& DefaultValue)
{
	TManagedArrayAccessor<T> CustomAttribute(InCollection, AttributeName, GroupName);
	CustomAttribute.AddAndFill(DefaultValue);
}

void FAddCustomCollectionAttributeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const int32 InNumElements = GetValue<int32>(Context, &NumElements);

		FName GroupNameToUse;
		if (GroupName != EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Custom)
		{
			GroupNameToUse = GetGroupName(GroupName);
		}
		else
		{
			GroupNameToUse = FName(*CustomGroupName);
		}

		if (GroupNameToUse.GetStringLength() > 0 && AttrName.Len() > 0)
		{
			// If the group already exists don't change the number of elements
			if (!InCollection.HasGroup(GroupNameToUse))
			{
				InCollection.AddGroup(GroupNameToUse);
				InCollection.AddElements(InNumElements, GroupNameToUse);
			}

			FName AttributeNameToUse = FName(*AttrName);

			switch (CustomAttributeType)
			{
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_UInt8:
				AddAndFillAttribute<uint8>(InCollection, AttributeNameToUse, GroupNameToUse, uint8(0));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Int32:
				AddAndFillAttribute<int32>(InCollection, AttributeNameToUse, GroupNameToUse, 0);
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Float:
				AddAndFillAttribute<float>(InCollection, AttributeNameToUse, GroupNameToUse, float(0.0));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Double:
				AddAndFillAttribute<double>(InCollection, AttributeNameToUse, GroupNameToUse, double(0.0));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Bool:
				AddAndFillAttribute<bool>(InCollection, AttributeNameToUse, GroupNameToUse, false);
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_String:
				AddAndFillAttribute<FString>(InCollection, AttributeNameToUse, GroupNameToUse, FString());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Vector2f:
				AddAndFillAttribute<FVector2f>(InCollection, AttributeNameToUse, GroupNameToUse, FVector2f(EForceInit::ForceInitToZero));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Vector3f:
				AddAndFillAttribute<FVector3f>(InCollection, AttributeNameToUse, GroupNameToUse, FVector3f(EForceInit::ForceInitToZero));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Vector3d:
				AddAndFillAttribute<FVector3d>(InCollection, AttributeNameToUse, GroupNameToUse, FVector3d(EForceInit::ForceInitToZero));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Vector4f:
				AddAndFillAttribute<FVector4f>(InCollection, AttributeNameToUse, GroupNameToUse, FVector4f(EForceInit::ForceInitToZero));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_LinearColor:
				AddAndFillAttribute<FLinearColor>(InCollection, AttributeNameToUse, GroupNameToUse, FLinearColor(0.f, 0.f, 0.f, 1.f));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Transform:
				AddAndFillAttribute<FTransform>(InCollection, AttributeNameToUse, GroupNameToUse, FTransform(FTransform::Identity));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Quat4f:
				AddAndFillAttribute<FQuat4f>(InCollection, AttributeNameToUse, GroupNameToUse, FQuat4f(ForceInitToZero));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Box:
				AddAndFillAttribute<FBox>(InCollection, AttributeNameToUse, GroupNameToUse, FBox(ForceInit));
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Guid:
				AddAndFillAttribute<FGuid>(InCollection, AttributeNameToUse, GroupNameToUse, FGuid());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Int32Set:
				AddAndFillAttribute<TSet<int32>>(InCollection, AttributeNameToUse, GroupNameToUse, TSet<int32>());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Int32Array:
				AddAndFillAttribute<TArray<int32>>(InCollection, AttributeNameToUse, GroupNameToUse, TArray<int32>());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_IntVector:
				AddAndFillAttribute<FIntVector>(InCollection, AttributeNameToUse, GroupNameToUse, FIntVector());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_IntVector2:
				AddAndFillAttribute<FIntVector2>(InCollection, AttributeNameToUse, GroupNameToUse, FIntVector2());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_IntVector4:
				AddAndFillAttribute<FIntVector4>(InCollection, AttributeNameToUse, GroupNameToUse, FIntVector4());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_IntVector2Array:
				AddAndFillAttribute<TArray<FIntVector2>>(InCollection, AttributeNameToUse, GroupNameToUse, TArray<FIntVector2>());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_FloatArray:
				AddAndFillAttribute<TArray<float>>(InCollection, AttributeNameToUse, GroupNameToUse, TArray<float>());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_Vector2fArray:
				AddAndFillAttribute<TArray<FVector2f>>(InCollection, AttributeNameToUse, GroupNameToUse, TArray<FVector2f>());
				break;
			case ECustomAttributeTypeEnum::Dataflow_CustomAttributeType_FVector3fArray:
				AddAndFillAttribute<TArray<FVector3f>>(InCollection, AttributeNameToUse, GroupNameToUse, TArray<FVector3f>());
				break;
			}
		}

		SetValue<FManagedArrayCollection>(Context, MoveTemp(InCollection), &Collection);
	}
}


void FGetNumElementsInCollectionGroupDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&NumElements))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		
		int32 OutNumElements = 0;

		FName GroupNameToUse;
		if (GroupName != EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Custom)
		{
			GroupNameToUse = GetGroupName(GroupName);
		}
		else
		{
			GroupNameToUse = FName(*CustomGroupName);
		}

		if (GroupNameToUse.GetStringLength() > 0)
		{
			if (InCollection.HasGroup(GroupNameToUse))
			{
				OutNumElements = InCollection.NumElements(GroupNameToUse);
			}
		}

		SetValue<int32>(Context, OutNumElements, &NumElements);
	}
}


void FGetCollectionAttributeDataTypedDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<bool>>(&BoolAttributeData) ||
		Out->IsA<TArray<float>>(&FloatAttributeData) ||
		Out->IsA<TArray<double>>(&DoubleAttributeData) ||
		Out->IsA<TArray<int32>>(&Int32AttributeData) ||
		Out->IsA<TArray<FString>>(&StringAttributeData) ||
		Out->IsA<TArray<FVector3f>>(&Vector3fAttributeData) ||
		Out->IsA<TArray<FVector3d>>(&Vector3dAttributeData))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		FName GroupNameToUse;
		if (GroupName != EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Custom)
		{
			GroupNameToUse = GetGroupName(GroupName);
		}
		else
		{
			GroupNameToUse = FName(*CustomGroupName);
		}

		SetValue<TArray<bool>>(Context, TArray<bool>(), &BoolAttributeData);
		SetValue<TArray<float>>(Context, TArray<float>(), &FloatAttributeData);
		SetValue<TArray<double>>(Context, TArray<double>(), &DoubleAttributeData);
		SetValue<TArray<int32>>(Context, TArray<int32>(), &Int32AttributeData);
		SetValue<TArray<FString>>(Context, TArray<FString>(), &StringAttributeData);
		SetValue<TArray<FVector3f>>(Context, TArray<FVector3f>(), &Vector3fAttributeData);
		SetValue<TArray<FVector3d>>(Context, TArray<FVector3d>(), &Vector3dAttributeData);

		if (GroupNameToUse.GetStringLength() > 0 && AttrName.Len() > 0)
		{
			if (InCollection.HasGroup(GroupNameToUse))
			{
				if (InCollection.HasAttribute(FName(*AttrName), GroupNameToUse))
				{
					FString TypeStr = GetArrayTypeString(InCollection.GetAttributeType(FName(*AttrName), GroupNameToUse)).ToString();

					if (TypeStr == FString("Bool"))
					{
						const TManagedArray<bool>& AttributeArr = InCollection.GetAttribute<bool>(FName(*AttrName), GroupNameToUse);
						SetValue<TArray<bool>>(Context, AttributeArr.GetConstArray(), &BoolAttributeData);
					}
					else if (TypeStr == FString("Float"))
					{
						const TManagedArray<float>& AttributeArr = InCollection.GetAttribute<float>(FName(*AttrName), GroupNameToUse);
						SetValue<TArray<float>>(Context, AttributeArr.GetConstArray(), &FloatAttributeData);
					}
					else if (TypeStr == FString("Double"))
					{
						const TManagedArray<double>& AttributeArr = InCollection.GetAttribute<double>(FName(*AttrName), GroupNameToUse);
						SetValue<TArray<double>>(Context, AttributeArr.GetConstArray(), &DoubleAttributeData);
					}
					else if (TypeStr == FString("Int32"))
					{
						const TManagedArray<int32>& AttributeArr = InCollection.GetAttribute<int32>(FName(*AttrName), GroupNameToUse);
						SetValue<TArray<int32>>(Context, AttributeArr.GetConstArray(), &Int32AttributeData);
					}
					else if (TypeStr == FString("String"))
					{
						const TManagedArray<FString>& AttributeArr = InCollection.GetAttribute<FString>(FName(*AttrName), GroupNameToUse);
						SetValue<TArray<FString>>(Context, AttributeArr.GetConstArray(), &StringAttributeData);
					}
					else if (TypeStr == FString("Vector"))
					{
						const TManagedArray<FVector3f>& AttributeArr = InCollection.GetAttribute<FVector3f>(FName(*AttrName), GroupNameToUse);
						SetValue<TArray<FVector3f>>(Context, AttributeArr.GetConstArray(), &Vector3fAttributeData);
					}
					else if (TypeStr == FString("Vector3d"))
					{
						const TManagedArray<FVector3d>& AttributeArr = InCollection.GetAttribute<FVector3d>(FName(*AttrName), GroupNameToUse);
						SetValue<TArray<FVector3d>>(Context, AttributeArr.GetConstArray(), &Vector3dAttributeData);
					}
				}
			}
		}
	}
}

template<typename T>
static void SetAttributeData(const FDataflowNode* DataflowNode, Dataflow::FContext& Context, FManagedArrayCollection& InCollection, const TArray<T>& Property, FName AttributeName, FName GroupName)
{
	if (DataflowNode && DataflowNode->IsConnected<TArray<T>>(&Property))
	{
		TArray<T> AttributeData = DataflowNode->GetValue<TArray<T>>(Context, &Property);
		TManagedArray<T>& AttributeArray = InCollection.ModifyAttribute<T>(AttributeName, GroupName);

		if (AttributeData.Num() == AttributeArray.Num())
		{
			for (int32 Idx = 0; Idx < AttributeArray.Num(); ++Idx)
			{
				AttributeArray[Idx] = AttributeData[Idx];
			}
		}
	}
}

void FSetCollectionAttributeDataTypedDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		FName GroupNameToUse;
		if (GroupName != EStandardGroupNameEnum::Dataflow_EStandardGroupNameEnum_Custom)
		{
			GroupNameToUse = GetGroupName(GroupName);
		}
		else
		{
			GroupNameToUse = FName(*CustomGroupName);
		}

		if (GroupNameToUse.GetStringLength() > 0 && AttrName.Len() > 0)
		{
			if (InCollection.HasGroup(GroupNameToUse))
			{
				if (InCollection.HasAttribute(FName(*AttrName), GroupNameToUse))
				{
					FName AttributeName = FName(*AttrName);
					FString TypeStr = GetArrayTypeString(InCollection.GetAttributeType(AttributeName, GroupNameToUse)).ToString();
					
					if (TypeStr == FString("Bool"))
					{
						SetAttributeData<bool>(this, Context, InCollection, BoolAttributeData, AttributeName, GroupNameToUse);
					}
					else if (TypeStr == FString("Float"))
					{
						SetAttributeData<float>(this, Context, InCollection, FloatAttributeData, AttributeName, GroupNameToUse);
					}
					else if (TypeStr == FString("Double"))
					{
						SetAttributeData<double>(this, Context, InCollection, DoubleAttributeData, AttributeName, GroupNameToUse);
					}
					else if (TypeStr == FString("Int32"))
					{
						SetAttributeData<int32>(this, Context, InCollection, Int32AttributeData, AttributeName, GroupNameToUse);
					}
					else if (TypeStr == FString("String"))
					{
						SetAttributeData<FString>(this, Context, InCollection, StringAttributeData, AttributeName, GroupNameToUse);
					}
					else if (TypeStr == FString("Vector"))
					{
						SetAttributeData<FVector3f>(this, Context, InCollection, Vector3fAttributeData, AttributeName, GroupNameToUse);
					}
					else if (TypeStr == FString("Vector3d"))
					{
						SetAttributeData<FVector3d>(this, Context, InCollection, Vector3dAttributeData, AttributeName, GroupNameToUse);
					}
				}
			}
		}

		SetValue<FManagedArrayCollection>(Context, MoveTemp(InCollection), &Collection);
	}
}


void FBoolArrayToFaceSelectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowFaceSelection>(&FaceSelection))
	{
		const TArray<bool>& InBoolAttributeData = GetValue<TArray<bool>>(Context, &BoolAttributeData);

		FDataflowFaceSelection NewFaceSelection;
		NewFaceSelection.Initialize(InBoolAttributeData.Num(), false);
		NewFaceSelection.SetFromArray(InBoolAttributeData);

		SetValue<FDataflowFaceSelection>(Context, NewFaceSelection, &FaceSelection);
	}
}


void FFloatArrayToVertexSelectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowVertexSelection>(&VertexSelection))
	{
		const TArray<float>& InFloatArray = GetValue<TArray<float>>(Context, &FloatArray);

		FDataflowVertexSelection NewVertexSelection;
		NewVertexSelection.Initialize(InFloatArray.Num(), false);

		for (int32 Idx = 0; Idx < InFloatArray.Num(); ++Idx)
		{
			if (Operation == ECompareOperationEnum::Dataflow_Compare_Equal)
			{
				if (InFloatArray[Idx] == Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
			else if (Operation == ECompareOperationEnum::Dataflow_Compare_Smaller)
			{
				if (InFloatArray[Idx] < Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
			else if (Operation == ECompareOperationEnum::Dataflow_Compare_SmallerOrEqual)
			{
				if (InFloatArray[Idx] <= Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
			else if (Operation == ECompareOperationEnum::Dataflow_Compare_Greater)
			{
				if (InFloatArray[Idx] > Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
			else if (Operation == ECompareOperationEnum::Dataflow_Compare_GreaterOrEqual)
			{
				if (InFloatArray[Idx] >= Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
		}

		SetValue<FDataflowVertexSelection>(Context, NewVertexSelection, &VertexSelection);
	}
}


void FSetVertexColorInCollectionFromVertexSelectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const FDataflowVertexSelection& InVertexSelection = GetValue<FDataflowVertexSelection>(Context, &VertexSelection);

		if (InCollection.NumElements(FGeometryCollection::VerticesGroup) == InVertexSelection.Num())
		{
			const int32 NumVertices = InCollection.NumElements(FGeometryCollection::VerticesGroup);

//			TManagedArray<FLinearColor>& VertexColors = InCollection.ModifyAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);
			if (TManagedArray<FLinearColor>* VertexColors = InCollection.FindAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup))
			{
				for (int32 Idx = 0; Idx < NumVertices; ++Idx)
				{
					if (InVertexSelection.IsSelected(Idx))
					{
						(*VertexColors)[Idx] = SelectedColor;
					}
					else
					{
						(*VertexColors)[Idx] = NonSelectedColor;
					}
				}
			}
		}

		SetValue<FManagedArrayCollection>(Context, MoveTemp(InCollection), &Collection);
	}
}

void FSelectionToVertexListDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	const FDataflowVertexSelection& InVertexSelection = GetValue<FDataflowVertexSelection>(Context, &VertexSelection);
	SetValue< TArray<int32> >(Context, InVertexSelection.AsArray(), &VertexList);
}

void FMakeTransformDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FTransform>(&OutTransform))
	{
		SetValue<FTransform>(Context,
			FTransform(FQuat::MakeFromEuler(GetValue<FVector>(Context, &InRotation))
			, GetValue<FVector>(Context, &InTranslation)
			, GetValue<FVector>(Context, &InScale))
			, &OutTransform);
	}
}


void FSetVertexColorInCollectionFromFloatArrayDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TArray<float>& InFloatArray = GetValue<TArray<float>>(Context, &FloatArray);

		const int32 NumVertices = InCollection.NumElements(FGeometryCollection::VerticesGroup);

		if (InFloatArray.Num() == NumVertices)
		{
			if (TManagedArray<FLinearColor>* VertexColors = InCollection.FindAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup))
			{
				for (int32 Idx = 0; Idx < NumVertices; ++Idx)
				{
					(*VertexColors)[Idx] = FLinearColor(Scale * InFloatArray[Idx], Scale * InFloatArray[Idx], Scale * InFloatArray[Idx]);
				}
			}
		}

		SetValue<FManagedArrayCollection>(Context, MoveTemp(InCollection), &Collection);
	}
}


void FFloatArrayNormalizeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<float>>(&OutFloatArray))
	{
		const TArray<float>& InInFloatArray = GetValue<TArray<float>>(Context, &InFloatArray);
		const FDataflowVertexSelection& InSelection = GetValue<FDataflowVertexSelection>(Context, &Selection);
		const float InMinRange = GetValue<float>(Context, &MinRange);
		const float InMaxRange = GetValue<float>(Context, &MaxRange);

		const int32 NumElems = InFloatArray.Num();

		TArray<float> NewFloatArray;
		NewFloatArray.Init(false, NumElems);

		if (IsConnected<FDataflowVertexSelection>(&Selection))
		{
			if (InInFloatArray.Num() == InSelection.Num())
			{
				// Compute Min/Max
				float MinValue = 1e9, MaxValue = 1e-9;

				for (int32 Idx = 0; Idx < NumElems; ++Idx)
				{
					if (InSelection.IsSelected(Idx))
					{
						if (InInFloatArray[Idx] < MinValue)
						{
							MinValue = InInFloatArray[Idx];
						}

						if (InInFloatArray[Idx] > MaxValue)
						{
							MaxValue = InInFloatArray[Idx];
						}
					}
				}

				for (int32 Idx = 0; Idx < NumElems; ++Idx)
				{
					if (InSelection.IsSelected(Idx))
					{
						// Normalize it
						NewFloatArray[Idx] = (NewFloatArray[Idx] - MinValue) / (MaxValue - MinValue);

						// Transform it into (MinRange, MaxRange)
						NewFloatArray[Idx] = InMinRange + NewFloatArray[Idx] * (InMaxRange - InMinRange);
					}
				}

				SetValue<TArray<float>>(Context, NewFloatArray, &OutFloatArray);
				return;
			}
		}
		else
		{
			// Compute Min/Max
			float MinValue = 1e9, MaxValue = 1e-9;

			for (int32 Idx = 0; Idx < NumElems; ++Idx)
			{
				if (InInFloatArray[Idx] < MinValue)
				{
					MinValue = InInFloatArray[Idx];
				}

				if (InInFloatArray[Idx] > MaxValue)
				{
					MaxValue = InInFloatArray[Idx];
				}
			}

			for (int32 Idx = 0; Idx < NumElems; ++Idx)
			{
				// Normalize it
				NewFloatArray[Idx] = (NewFloatArray[Idx] - MinValue) / (MaxValue - MinValue);

				// Transform it into (MinRange, MaxRange)
				NewFloatArray[Idx] = InMinRange + NewFloatArray[Idx] * (InMaxRange - InMinRange);
			}

			SetValue<TArray<float>>(Context, NewFloatArray, &OutFloatArray);
			return;
		}

		SetValue<TArray<float>>(Context, TArray<float>(), &OutFloatArray);
	}
}


void FVectorArrayNormalizeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&OutVectorArray))
	{
		const TArray<FVector>& InInVectorArray = GetValue<TArray<FVector>>(Context, &InVectorArray);
		const FDataflowVertexSelection& InSelection = GetValue<FDataflowVertexSelection>(Context, &Selection);
		const float InMagnitude = GetValue<float>(Context, &Magnitude);

		const int32 NumElems = InInVectorArray.Num();

		TArray<FVector> NewVectorArray;
		NewVectorArray.Init(FVector(0.f), NumElems);

		if (IsConnected<FDataflowVertexSelection>(&Selection))
		{
			if (InInVectorArray.Num() == InSelection.Num())
			{
				for (int32 Idx = 0; Idx < NumElems; ++Idx)
				{
					FVector Vector = InInVectorArray[Idx];

					if (InSelection.IsSelected(Idx))
					{
						Vector.Normalize();
						Vector *= InMagnitude;
						NewVectorArray[Idx] = Vector;
					}
				}

				SetValue<TArray<FVector>>(Context, NewVectorArray, &OutVectorArray);
				return;
			}
		}
		else
		{
			for (int32 Idx = 0; Idx < NumElems; ++Idx)
			{
				FVector Vector = InInVectorArray[Idx];

				Vector.Normalize();
				Vector *= InMagnitude;
				NewVectorArray[Idx] = Vector;
			}

			SetValue<TArray<FVector>>(Context, NewVectorArray, &OutVectorArray);
			return;
		}

		SetValue<TArray<FVector>>(Context, TArray<FVector>(), &OutVectorArray);
	}
}


void FMakeQuaternionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FQuat>(&Quaternion))
	{
		FQuat Value(GetValue<float>(Context, &X, X), GetValue<float>(Context, &Y, Y), GetValue<float>(Context, &Z, Z), GetValue<float>(Context, &W, W));
		SetValue<FQuat>(Context, Value, &Quaternion);
	}
}

void FMultiplyTransformDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FTransform>(&OutTransform))
	{
		SetValue<FTransform>(Context,
			GetValue<FTransform>(Context, &InLeftTransform, FTransform::Identity)
			*GetValue<FTransform>(Context, &InRightTransform, FTransform::Identity)
			, &OutTransform);
	}
}

void FInvertTransformDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FTransform>(&OutTransform))
	{
		FTransform InXf = GetValue<FTransform>(Context, &InTransform, FTransform::Identity);
		FTransform OutXf = InXf.Inverse();
		SetValue<FTransform>(Context, OutXf, &OutTransform);
	}
}

void FUnionIntArraysDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<int32>>(&OutArray))
	{
		TArray<int32> Array1 = GetValue<TArray<int32>>(Context, &InArray1);
		TArray<int32> Array2 = GetValue<TArray<int32>>(Context, &InArray2);
		TArray<int32> OutputArray;
		for (int32 i = 0; i < Array1.Num(); i++)
		{
			OutputArray.AddUnique(Array1[i]);
		}
		for (int32 i = 0; i < Array2.Num(); i++)
		{
			OutputArray.AddUnique(Array2[i]);
		}
		SetValue<TArray<int32>>(Context, OutputArray, &OutArray);
	}
}
