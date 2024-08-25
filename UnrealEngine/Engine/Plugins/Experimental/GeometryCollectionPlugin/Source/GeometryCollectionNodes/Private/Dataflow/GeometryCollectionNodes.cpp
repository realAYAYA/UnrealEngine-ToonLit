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
#include "GeometryCollection/Facades/CollectionHierarchyFacade.h"
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
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBoundingBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExpandBoundingBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetBoxLengthsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExpandVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FStringAppendDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FHashStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FHashVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetBoundingBoxesFromCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetRootIndexFromCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetCentroidsFromCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTransformCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBakeTransformsInCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTransformMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCompareIntDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCompareFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBranchMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBranchCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetSchemaDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRemoveOnBreakDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetAnchorStateDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FProximityDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionSetPivotDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAddCustomCollectionAttributeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetNumElementsInCollectionGroupDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetCollectionAttributeDataTypedDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetCollectionAttributeDataTypedDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexColorInCollectionFromVertexSelectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexColorInCollectionFromFloatArrayDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMultiplyTransformDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FInvertTransformDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSelectionToVertexListDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBranchFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBranchIntDataflowNode);

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
				SetValue(Context, (const FManagedArrayCollection&)(*AssetCollection), &Collection);
			}
			else
			{
				SetValue(Context, FManagedArrayCollection(), &Collection);
			}
		}
		else
		{
			SetValue(Context, FManagedArrayCollection(), &Collection);
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
		SetValue(Context, MoveTemp(InCollection1), &Collection1);
		if (const TManagedArray<FString>* GuidArray2 = InCollection2.FindAttribute<FString>("Guid", FGeometryCollection::GeometryGroup))
		{
			GeometryGroupGuidsLocal2 = GuidArray2->GetConstArray();
		}
		SetValue(Context, MoveTemp(GeometryGroupGuidsLocal1), &GeometryGroupGuidsOut1);
		SetValue(Context, MoveTemp(GeometryGroupGuidsLocal2), &GeometryGroupGuidsOut2);
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



void FBoundingBoxDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FBox>(&BoundingBox))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FBoundsFacade BoundsFacade(InCollection);
		const FBox& BoundingBoxInCollectionSpace = BoundsFacade.GetBoundingBoxInCollectionSpace();

		SetValue(Context, BoundingBoxInCollectionSpace, &BoundingBox);
	}
}

void FGetBoxLengthsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Lengths))
	{
		const TArray<FBox>& InBoxes = GetValue(Context, &Boxes);

		TArray<float> OutLengths;
		OutLengths.SetNumUninitialized(InBoxes.Num());
		for (int32 Idx = 0; Idx < InBoxes.Num(); ++Idx)
		{
			const FBox& Box = InBoxes[Idx];
			OutLengths[Idx] = BoxToMeasurement(Box);
		}

		SetValue(Context, MoveTemp(OutLengths), &Lengths);
	}
}


void FExpandBoundingBoxDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FBox BBox = GetValue<FBox>(Context, &BoundingBox);

	if (Out->IsA<FVector>(&Min))
	{
		SetValue(Context, BBox.Min, &Min);
	}
	else if (Out->IsA<FVector>(&Max))
	{
		SetValue(Context, BBox.Max, &Max);
	}
	else if (Out->IsA<FVector>(&Center))
	{
		SetValue(Context, BBox.GetCenter(), &Center);
	}
	else if (Out->IsA<FVector>(&HalfExtents))
	{
		SetValue(Context, BBox.GetExtent(), &HalfExtents);
	}
	else if (Out->IsA<float>(&Volume))
	{
		SetValue(Context, (float)BBox.GetVolume(), &Volume);
	}
}


void FExpandVectorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FVector VectorVal = GetValue<FVector>(Context, &Vector);

	if (Out->IsA<float>(&X))
	{
		SetValue(Context, (float)VectorVal.X, &X);
	}
	else if (Out->IsA<float>(&Y))
	{
		SetValue(Context, (float)VectorVal.Y, &Y);
	}
	else if (Out->IsA<float>(&Z))
	{
		SetValue(Context, (float)VectorVal.Z, &Z);
	}
}

void FStringAppendDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		FString StringOut = GetValue<FString>(Context, &String1) + GetValue<FString>(Context, &String2);
		SetValue(Context, MoveTemp(StringOut), &String);
	}
}


void FHashStringDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&Hash))
	{
		SetValue(Context, (int32)GetTypeHash(GetValue<FString>(Context, &String)), &Hash);
	}
}

void FHashVectorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&Hash))
	{
		SetValue(Context, (int32)GetTypeHash(GetValue<FVector>(Context, &Vector)), &Hash);
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

		SetValue(Context, MoveTemp(BoundingBoxesArr), &BoundingBoxes);
	}
}

void FGetRootIndexFromCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&RootIndex))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		Chaos::Facades::FCollectionHierarchyFacade HierarchyFacade(InCollection);
		SetValue(Context, HierarchyFacade.GetRootIndex(), &RootIndex);
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

		SetValue(Context, MoveTemp(CentroidsArr), &Centroids);
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

		SetValue(Context, MoveTemp(InCollection), &Collection);
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

		SetValue(Context, MoveTemp(InCollection), &Collection);
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

			SetValue(Context, NewMesh, &Mesh);
		}
		else
		{
			SetValue(Context, TObjectPtr<UDynamicMesh>(NewObject<UDynamicMesh>()), &Mesh);
		}
	}
}

namespace
{
	// helper to apply an ECompareOperationEnum operation to various numeric types
	template <typename T>
	static bool ApplyDataflowOperationComparison(T A, T B, ECompareOperationEnum Operation)
	{
		switch (Operation)
		{
		case ECompareOperationEnum::Dataflow_Compare_Equal:
			return A == B;
		case ECompareOperationEnum::Dataflow_Compare_Smaller:
			return A < B;
		case ECompareOperationEnum::Dataflow_Compare_SmallerOrEqual:
			return A <= B;
		case ECompareOperationEnum::Dataflow_Compare_Greater:
			return A > B;
		case ECompareOperationEnum::Dataflow_Compare_GreaterOrEqual:
			return A >= B;
		default:
			ensureMsgf(false, TEXT("Invalid ECompareOperationEnum value: %u"), (uint8)Operation);
		}

		return false;
	}
}


void FCompareIntDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<bool>(&Result))
	{
		const int32 IntAValue = GetValue<int32>(Context, &IntA);
		const int32 IntBValue = GetValue<int32>(Context, &IntB);
		const bool ResultValue = ApplyDataflowOperationComparison(IntAValue, IntBValue, Operation);

		SetValue(Context, ResultValue, &Result);
	}
}


void FCompareFloatDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Result))
	{
		const float AValue = GetValue(Context, &FloatA);
		const float BValue = GetValue(Context, &FloatB);
		const bool ResultValue = ApplyDataflowOperationComparison(AValue, BValue, Operation);

		SetValue(Context, ResultValue, &Result);
	}
}


void FBranchMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<UDynamicMesh>>(&Mesh))
	{
		bool InCondition = GetValue<bool>(Context, &bCondition);

		if (InCondition)
		{
			if (TObjectPtr<UDynamicMesh> InMeshA = GetValue<TObjectPtr<UDynamicMesh>>(Context, &MeshA))
			{
				SetValue(Context, InMeshA, &Mesh);

				return;
			}
		}
		else
		{
			if (TObjectPtr<UDynamicMesh> InMeshB = GetValue<TObjectPtr<UDynamicMesh>>(Context, &MeshB))
			{
				SetValue(Context, InMeshB, &Mesh);

				return;
			}
		}

		SetValue(Context, TObjectPtr<UDynamicMesh>(NewObject<UDynamicMesh>()), &Mesh);
	}
}


void FBranchCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&ChosenCollection))
	{
		bool InCondition = GetValue<bool>(Context, &bCondition);

		if (InCondition)
		{
			if (IsConnected(&TrueCollection))
			{
				const FManagedArrayCollection& InTrueCollection = GetValue(Context, &TrueCollection);
				SetValue(Context, InTrueCollection, &ChosenCollection);
				return;
			}
		}
		else
		{
			if (IsConnected(&FalseCollection))
			{
				const FManagedArrayCollection& InFalseCollection = GetValue(Context, &FalseCollection);
				SetValue(Context, InFalseCollection, &ChosenCollection);
				return;
			}
		}
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

		SetValue(Context, MoveTemp(OutputStr), &String);
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
		SetValue(Context, MoveTemp(InCollection), &Collection);
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

			SetValue<const FManagedArrayCollection&>(Context, *GeomCollection, &Collection);
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
			Properties.ContactMethod = (EProximityContactMethod)FilterContactMethod;
			Properties.DistanceThreshold = GetValue(Context, &DistanceThreshold);
			Properties.bUseAsConnectionGraph = bUseAsConnectionGraph;
			Properties.ContactAreaMethod = (EConnectionContactMethod)ContactAreaMethod;
			Properties.RequireContactAmount = GetValue(Context, &ContactThreshold);

			GeomCollection->SetProximityProperties(Properties);

			UE::GeometryCollectionConvexUtility::FConvexHulls TransformedExistingHulls;
			bool bUseExistingHulls = false;
			if (!bRecomputeConvexHulls)
			{
				bUseExistingHulls = UE::GeometryCollectionConvexUtility::GetExistingConvexHullsInSharedSpace(GeomCollection.Get(), TransformedExistingHulls, true);
			}

			// Invalidate proximity
			FGeometryCollectionProximityUtility ProximityUtility(GeomCollection.Get());
			ProximityUtility.InvalidateProximity();
			ProximityUtility.UpdateProximity(bUseExistingHulls ? &TransformedExistingHulls : nullptr);

			SetValue<const FManagedArrayCollection&>(Context, *GeomCollection, &Collection);
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

		SetValue(Context, MoveTemp(InCollection), &Collection);
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

		SetValue(Context, MoveTemp(InCollection), &Collection);
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

		SetValue(Context, OutNumElements, &NumElements);
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

		SetValue(Context, TArray<bool>(), &BoolAttributeData);
		SetValue(Context, TArray<float>(), &FloatAttributeData);
		SetValue(Context, TArray<double>(), &DoubleAttributeData);
		SetValue(Context, TArray<int32>(), &Int32AttributeData);
		SetValue(Context, TArray<FString>(), &StringAttributeData);
		SetValue(Context, TArray<FVector3f>(), &Vector3fAttributeData);
		SetValue(Context, TArray<FVector3d>(), &Vector3dAttributeData);

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
						TArray<bool> BoolArray = AttributeArr.GetAsBoolArray();
						SetValue(Context, MoveTemp(BoolArray), &BoolAttributeData);
					}
					else if (TypeStr == FString("Float"))
					{
						const TManagedArray<float>& AttributeArr = InCollection.GetAttribute<float>(FName(*AttrName), GroupNameToUse);
						SetValue(Context, AttributeArr.GetConstArray(), &FloatAttributeData);
					}
					else if (TypeStr == FString("Double"))
					{
						const TManagedArray<double>& AttributeArr = InCollection.GetAttribute<double>(FName(*AttrName), GroupNameToUse);
						SetValue(Context, AttributeArr.GetConstArray(), &DoubleAttributeData);
					}
					else if (TypeStr == FString("Int32"))
					{
						const TManagedArray<int32>& AttributeArr = InCollection.GetAttribute<int32>(FName(*AttrName), GroupNameToUse);
						SetValue(Context, AttributeArr.GetConstArray(), &Int32AttributeData);
					}
					else if (TypeStr == FString("String"))
					{
						const TManagedArray<FString>& AttributeArr = InCollection.GetAttribute<FString>(FName(*AttrName), GroupNameToUse);
						SetValue(Context, AttributeArr.GetConstArray(), &StringAttributeData);
					}
					else if (TypeStr == FString("Vector"))
					{
						const TManagedArray<FVector3f>& AttributeArr = InCollection.GetAttribute<FVector3f>(FName(*AttrName), GroupNameToUse);
						SetValue(Context, AttributeArr.GetConstArray(), &Vector3fAttributeData);
					}
					else if (TypeStr == FString("Vector3d"))
					{
						const TManagedArray<FVector3d>& AttributeArr = InCollection.GetAttribute<FVector3d>(FName(*AttrName), GroupNameToUse);
						SetValue(Context, AttributeArr.GetConstArray(), &Vector3dAttributeData);
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

		SetValue(Context, MoveTemp(InCollection), &Collection);
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

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FSelectionToVertexListDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	const FDataflowVertexSelection& InVertexSelection = GetValue<FDataflowVertexSelection>(Context, &VertexSelection);
	SetValue(Context, InVertexSelection.AsArray(), &VertexList);
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

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}



void FMultiplyTransformDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FTransform>(&OutTransform))
	{
		SetValue(Context,
			GetValue<FTransform>(Context, &InLeftTransform, FTransform::Identity)
			*GetValue<FTransform>(Context, &InRightTransform, FTransform::Identity)
			, &OutTransform);
	}
}

void FInvertTransformDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FTransform>(&OutTransform))
	{
		const FTransform InXf = GetValue<FTransform>(Context, &InTransform, FTransform::Identity);
		const FTransform OutXf = InXf.Inverse();
		SetValue(Context, OutXf, &OutTransform);
	}
}

void FBranchFloatDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		bool InCondition = GetValue<bool>(Context, &bCondition);

		if (InCondition)
		{
			const float InA = GetValue<float>(Context, &A, A);

			SetValue(Context, InA, &ReturnValue);
		}
		else
		{
			const float InB = GetValue<float>(Context, &B, B);

			SetValue(Context, InB, &ReturnValue);
		}
	}
}

void FBranchIntDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&ReturnValue))
	{
		bool InCondition = GetValue<bool>(Context, &bCondition);

		if (InCondition)
		{
			const int32 InA = GetValue<int32>(Context, &A, A);

			SetValue(Context, InA, &ReturnValue);
		}
		else
		{
			const int32 InB = GetValue<int32>(Context, &B, B);

			SetValue(Context, InB, &ReturnValue);
		}
	}
}

