// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionSelectionNodes.h"
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
#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "StaticMeshAttributes.h"
#include "DynamicMeshEditor.h"
#include "Operations/MeshBoolean.h"

#include "EngineGlobals.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "Voronoi/Voronoi.h"
#include "PlanarCut.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"
#include "FractureEngineClustering.h"
#include "FractureEngineSelection.h"
#include "GeometryCollection/Facades/CollectionTransformSelectionFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionSelectionNodes)

namespace Dataflow
{

	void GeometryCollectionSelectionNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionAllDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionSetOperationDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionInfoDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionNoneDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionInvertDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionRandomDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionRootDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionCustomDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionFromIndexArrayDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionParentDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionByPercentageDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionChildrenDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionSiblingsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionLevelDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionTargetLevelDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionContactDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionLeafDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionClusterDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionBySizeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionByVolumeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionInBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionInSphereDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionByFloatAttrDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSelectFloatArrayIndicesInRangeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionTransformSelectionByIntAttrDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionVertexSelectionCustomDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionFaceSelectionCustomDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionSelectionConvertDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionFaceSelectionInvertDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionVertexSelectionByPercentageDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCollectionVertexSelectionSetOperationDataflowNode);

		// GeometryCollection|Selection
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("GeometryCollection|Selection", FLinearColor(1.f, 1.f, 0.05f), CDefaultNodeBodyTintColor);
	}
}


void FCollectionTransformSelectionAllDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectAll();

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionSetOperationDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FDataflowTransformSelection& InTransformSelectionA = GetValue<FDataflowTransformSelection>(Context, &TransformSelectionA);
		const FDataflowTransformSelection& InTransformSelectionB = GetValue<FDataflowTransformSelection>(Context, &TransformSelectionB);

		FDataflowTransformSelection NewTransformSelection;

		if (InTransformSelectionA.Num() == InTransformSelectionB.Num())
		{
			if (Operation == ESetOperationEnum::Dataflow_SetOperation_AND)
			{
				InTransformSelectionA.AND(InTransformSelectionB, NewTransformSelection);
			}
			else if (Operation == ESetOperationEnum::Dataflow_SetOperation_OR)
			{
				InTransformSelectionA.OR(InTransformSelectionB, NewTransformSelection);
			}
			else if (Operation == ESetOperationEnum::Dataflow_SetOperation_XOR)
			{
				InTransformSelectionA.XOR(InTransformSelectionB, NewTransformSelection);
			}
			else if (Operation == ESetOperationEnum::Dataflow_SetOperation_Subtract)
			{
				InTransformSelectionA.Subtract(InTransformSelectionB, NewTransformSelection);
			}
		}
		else
		{
			// ERROR: INPUT TRANSFORMSELECTIONS HAVE DIFFERENT NUMBER OF ELEMENTS
			FString ErrorStr = "Input TransformSelections have different number of elements.";
			UE_LOG(LogTemp, Error, TEXT("[Dataflow ERROR] %s"), *ErrorStr);
		}

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
}


namespace {
	struct BoneInfo {
		int32 BoneIndex;
		int32 Level;
	};
}

static void ExpandRecursive(const int32 BoneIndex, int32 Level, const TManagedArray<TSet<int32>>& Children, TArray<BoneInfo>& BoneHierarchy)
{
	BoneHierarchy.Add({ BoneIndex, Level });

	TSet<int32> ChildrenSet = Children[BoneIndex];
	if (ChildrenSet.Num() > 0)
	{
		for (auto& Child : ChildrenSet)
		{
			ExpandRecursive(Child, Level + 1, Children, BoneHierarchy);
		}
	}
}

static void BuildHierarchicalOutput(const TManagedArray<int32>& Parents, 
	const TManagedArray<TSet<int32>>& Children, 
	const TManagedArray<FString>& BoneNames,
	const FDataflowTransformSelection& TransformSelection, 
	FString& OutputStr)
{
	TArray<BoneInfo> BoneHierarchy;

	int32 NumElements = Parents.Num();
	for (int32 Index = 0; Index < NumElements; ++Index)
	{
		if (Parents[Index] == FGeometryCollection::Invalid)
		{
			ExpandRecursive(Index, 0, Children, BoneHierarchy);
		}
	}

	// Get level max
	int32 LevelMax = -1;
	int32 BoneNameLengthMax = -1;
	for (int32 Idx = 0; Idx < BoneHierarchy.Num(); ++Idx)
	{
		if (BoneHierarchy[Idx].Level > LevelMax)
		{
			LevelMax = BoneHierarchy[Idx].Level;
		}

		int32 BoneNameLength = BoneNames[Idx].Len();
		if (BoneNameLength > BoneNameLengthMax)
		{
			BoneNameLengthMax = BoneNameLength;
		}
	}

	const int32 BoneIndexWidth = 2 + LevelMax * 2 + 6;
	const int32 BoneNameWidth = BoneNameLengthMax + 2;
	const int32 SelectedWidth = 10;

	for (int32 Idx = 0; Idx < BoneHierarchy.Num(); ++Idx)
	{
		FString BoneIndexStr, BoneNameStr;
		BoneIndexStr.Reserve(BoneIndexWidth);
		BoneNameStr.Reserve(BoneNameWidth);

		if (BoneHierarchy[Idx].Level == 0)
		{
			BoneIndexStr.Appendf(TEXT("[%d]"), BoneHierarchy[Idx].BoneIndex);
		}
		else
		{
			BoneIndexStr.Appendf(TEXT(" |"));
			for (int32 Idx1 = 0; Idx1 < BoneHierarchy[Idx].Level; ++Idx1)
			{
				BoneIndexStr.Appendf(TEXT("--"));
			}
			BoneIndexStr.Appendf(TEXT("[%d]"), BoneHierarchy[Idx].BoneIndex);
		}
		BoneIndexStr = BoneIndexStr.RightPad(BoneIndexWidth);

		BoneNameStr.Appendf(TEXT("%s"), *BoneNames[Idx]);
		BoneNameStr = BoneNameStr.RightPad(BoneNameWidth);

		OutputStr.Appendf(TEXT("%s%s%s\n\n"), *BoneIndexStr, *BoneNameStr, (TransformSelection.IsSelected(BoneHierarchy[Idx].BoneIndex) ? TEXT("Selected") : TEXT("---")));
	}

}


void FCollectionTransformSelectionInfoDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		FString OutputStr;

		OutputStr.Appendf(TEXT("\n----------------------------------------\n"));
		OutputStr.Appendf(TEXT("Number of Elements: %d\n"), InTransformSelection.Num());

		// Hierarchical display
		if (InCollection.HasGroup(FGeometryCollection::TransformGroup) &&
			InCollection.HasAttribute("Parent", FGeometryCollection::TransformGroup) &&
			InCollection.HasAttribute("Children", FGeometryCollection::TransformGroup) &&
			InCollection.HasAttribute("BoneName", FGeometryCollection::TransformGroup))
		{
			if (InTransformSelection.Num() == InCollection.NumElements(FGeometryCollection::TransformGroup))
			{
				const TManagedArray<int32>& Parents = InCollection.GetAttribute<int32>("Parent", FGeometryCollection::TransformGroup);
				const TManagedArray<TSet<int32>>& Children = InCollection.GetAttribute<TSet<int32>>("Children", FGeometryCollection::TransformGroup);
				const TManagedArray<FString>& BoneNames = InCollection.GetAttribute<FString>("BoneName", FGeometryCollection::TransformGroup);

				BuildHierarchicalOutput(Parents, Children, BoneNames, InTransformSelection, OutputStr);
			}
			else
			{
				// ERROR: TransformSelection doesn't match the Collection
				FString ErrorStr = "TransformSelection doesn't match the Collection.";
				UE_LOG(LogTemp, Error, TEXT("[Dataflow ERROR] %s"), *ErrorStr);
			}
		}
		else
		// Simple display
		{
			for (int32 Idx = 0; Idx < InTransformSelection.Num(); ++Idx)
			{
				OutputStr.Appendf(TEXT("%4d: %s\n"), Idx, (InTransformSelection.IsSelected(Idx) ? TEXT("Selected") : TEXT("---")));
			}
		}

		OutputStr.Appendf(TEXT("----------------------------------------\n"));

		SetValue(Context, MoveTemp(OutputStr), &String);
	}
}


void FCollectionTransformSelectionNoneDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectNone();

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionInvertDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		FDataflowTransformSelection InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		InTransformSelection.Invert();

		SetValue(Context, MoveTemp(InTransformSelection), &TransformSelection);
	}
}


void FCollectionTransformSelectionRandomDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		float RandomSeedVal = GetValue<float>(Context, &RandomSeed);
		float RandomThresholdVal = GetValue<float>(Context, &RandomThreshold);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectRandom(bDeterministic, RandomSeedVal, RandomThresholdVal);

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionRootDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectRootBones();

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionCustomDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if (InCollection.HasGroup(FGeometryCollection::TransformGroup))
		{
			const int32 NumTransforms = InCollection.NumElements(FGeometryCollection::TransformGroup);

			FDataflowTransformSelection NewTransformSelection;
			NewTransformSelection.Initialize(NumTransforms, false);

			const FString InBoneIndices = GetValue<FString>(Context, &BoneIndicies);

			TArray<FString> Indicies;
			InBoneIndices.ParseIntoArray(Indicies, TEXT(" "), true);

			for (FString IndexStr : Indicies)
			{
				if (IndexStr.IsNumeric())
				{
					int32 Index = FCString::Atoi(*IndexStr);
					if (Index >= 0 && Index < NumTransforms)
					{
						NewTransformSelection.SetSelected(Index);
					}
					else
					{
						// ERROR: INVALID INDEX
						FString ErrorStr = "Invalid specified index found.";
						UE_LOG(LogTemp, Error, TEXT("[Dataflow ERROR] %s"), *ErrorStr);
					}
				}
			}

			SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
		}
		else
		{
			SetValue(Context, FDataflowTransformSelection(), &TransformSelection);
		}
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	}
}


void FCollectionTransformSelectionFromIndexArrayDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);

		if (InCollection.HasGroup(FGeometryCollection::TransformGroup))
		{
			const int32 NumTransforms = InCollection.NumElements(FGeometryCollection::TransformGroup);

			const TArray<int32>& InBoneIndices = GetValue(Context, &BoneIndices);

			FDataflowTransformSelection NewTransformSelection;
			NewTransformSelection.Initialize(NumTransforms, false);
			for (int32 SelectedIdx : InBoneIndices)
			{
				if (SelectedIdx >= 0 && SelectedIdx < NumTransforms)
				{
					NewTransformSelection.SetSelected(SelectedIdx);
				}
				else
				{
					UE_LOG(LogChaos, Error, TEXT("[Dataflow ERROR] Invalid selection index %d is outside valid bone index range [0, %d)"), SelectedIdx, NumTransforms);
				}
			}

			SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
		}
		else
		{
			SetValue(Context, FDataflowTransformSelection(), &TransformSelection);
		}
	}
	else if (Out->IsA(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionParentDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowTransformSelection InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		TArray<int32> SelectionArr = InTransformSelection.AsArray();
		TransformSelectionFacade.SelectParent(SelectionArr);

		InTransformSelection.SetFromArray(SelectionArr);
		
		SetValue(Context, MoveTemp(InTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionByPercentageDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		FDataflowTransformSelection InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		int32 InPercentage = GetValue<int32>(Context, &Percentage);
		float InRandomSeed = GetValue<float>(Context, &RandomSeed);

		TArray<int32> SelectionArr = InTransformSelection.AsArray();

		GeometryCollection::Facades::FCollectionTransformSelectionFacade::SelectByPercentage(SelectionArr, InPercentage, bDeterministic, InRandomSeed);

		InTransformSelection.SetFromArray(SelectionArr);
		SetValue(Context, MoveTemp(InTransformSelection), &TransformSelection);
	}
}


void FCollectionTransformSelectionChildrenDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowTransformSelection InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		TArray<int32> SelectionArr = InTransformSelection.AsArray();

		TransformSelectionFacade.SelectChildren(SelectionArr);
		InTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(InTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionSiblingsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowTransformSelection InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		TArray<int32> SelectionArr = InTransformSelection.AsArray();

		TransformSelectionFacade.SelectSiblings(SelectionArr);
		InTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(InTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionLevelDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowTransformSelection InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		TArray<int32> SelectionArr = InTransformSelection.AsArray();

		TransformSelectionFacade.SelectLevel(SelectionArr);
		InTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(InTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionTargetLevelDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);

		int InTargetLevel = GetValue(Context, &TargetLevel);

		TArray<int32> AllAtLevel = TransformSelectionFacade.GetBonesExactlyAtLevel(InTargetLevel, bSkipEmbedded);

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(AllAtLevel);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionContactDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowTransformSelection InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		TArray<int32> SelectionArr = InTransformSelection.AsArray();

		TransformSelectionFacade.SelectLevel(SelectionArr);
		InTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(InTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionLeafDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectLeaf();

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionClusterDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectCluster();

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionBySizeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		float InSizeMin = GetValue<float>(Context, &SizeMin);
		float InSizeMax = GetValue<float>(Context, &SizeMax);
		bool bInsideRange = RangeSetting == ERangeSettingEnum::Dataflow_RangeSetting_InsideRange;

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectBySize(InSizeMin, InSizeMax, bInclusive, bInsideRange, bUseRelativeSize);

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionByVolumeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		float InVolumeMin = GetValue<float>(Context, &VolumeMin);
		float InVolumeMax = GetValue<float>(Context, &VolumeMax);
		bool bInsideRange = RangeSetting == ERangeSettingEnum::Dataflow_RangeSetting_InsideRange;

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectByVolume(InVolumeMin, InVolumeMax, bInclusive, bInsideRange);

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionInBoxDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const FBox& InBox = GetValue<FBox>(Context, &Box);
		const FTransform& InTransform = GetValue<FTransform>(Context, &Transform);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);

		TArray<int32> SelectionArr;
		if (Type == ESelectSubjectTypeEnum::Dataflow_SelectSubjectType_Vertices)
		{
			SelectionArr = TransformSelectionFacade.SelectVerticesInBox(InBox, InTransform, bAllVerticesMustContainedInBox);
		}
		else if (Type == ESelectSubjectTypeEnum::Dataflow_SelectSubjectType_BoundingBox)
		{
			SelectionArr = TransformSelectionFacade.SelectBoundingBoxInBox(InBox, InTransform);
		}
		else if (Type == ESelectSubjectTypeEnum::Dataflow_SelectSubjectType_Centroid)
		{
			SelectionArr = TransformSelectionFacade.SelectCentroidInBox(InBox, InTransform);
		}

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionInSphereDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const FSphere& InSphere = GetValue<FSphere>(Context, &Sphere);
		const FTransform& InTransform = GetValue<FTransform>(Context, &Transform);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);

		TArray<int32> SelectionArr;
		if (Type == ESelectSubjectTypeEnum::Dataflow_SelectSubjectType_Vertices)
		{
			SelectionArr = TransformSelectionFacade.SelectVerticesInSphere(InSphere, InTransform, bAllVerticesMustContainedInSphere);
		}
		else if (Type == ESelectSubjectTypeEnum::Dataflow_SelectSubjectType_BoundingBox)
		{
			SelectionArr = TransformSelectionFacade.SelectBoundingBoxInSphere(InSphere, InTransform);
		}
		else if (Type == ESelectSubjectTypeEnum::Dataflow_SelectSubjectType_Centroid)
		{
			SelectionArr = TransformSelectionFacade.SelectCentroidInSphere(InSphere, InTransform);
		}

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionTransformSelectionByFloatAttrDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		float InMin = GetValue<float>(Context, &Min);
		float InMax = GetValue<float>(Context, &Max);
		bool bInsideRange = RangeSetting == ERangeSettingEnum::Dataflow_RangeSetting_InsideRange;

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectByFloatAttribute(GroupName, AttrName, InMin, InMax, bInclusive, bInsideRange);

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}

void FSelectFloatArrayIndicesInRangeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Indices))
	{
		const TArray<float>& InValues = GetValue(Context, &Values);
		float InMin = GetValue(Context, &Min);
		float InMax = GetValue(Context, &Max);
		bool bInsideRange = RangeSetting == ERangeSettingEnum::Dataflow_RangeSetting_InsideRange;

		TArray<int32> OutIndices;
		for (int32 Idx = 0; Idx < InValues.Num(); ++Idx)
		{
			const float FloatValue = InValues[Idx];

			if (bInsideRange && FloatValue > Min && FloatValue < Max)
			{
				OutIndices.Add(Idx);
			}
			else if (!bInsideRange && (FloatValue < Min || FloatValue > Max))
			{
				OutIndices.Add(Idx);
			}
			else if (bInclusive && (FloatValue == Min || FloatValue == Max))
			{
				OutIndices.Add(Idx);
			}
		}

		SetValue(Context, MoveTemp(OutIndices), &Indices);
	}
}

void FCollectionTransformSelectionByIntAttrDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		int32 InMin = GetValue<int32>(Context, &Min);
		int32 InMax = GetValue<int32>(Context, &Max);
		bool bInsideRange = RangeSetting == ERangeSettingEnum::Dataflow_RangeSetting_InsideRange;

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
		const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectByIntAttribute(GroupName, AttrName, InMin, InMax, bInclusive, bInsideRange);

		FDataflowTransformSelection NewTransformSelection;
		NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
		NewTransformSelection.SetFromArray(SelectionArr);

		SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionVertexSelectionCustomDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowVertexSelection>(&VertexSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if (InCollection.HasGroup(FGeometryCollection::VerticesGroup))
		{
			const int32 NumVertices = InCollection.NumElements(FGeometryCollection::VerticesGroup);

			FDataflowVertexSelection NewVertexSelection;
			NewVertexSelection.Initialize(NumVertices, false);

			const FString InVertexIndicies = GetValue<FString>(Context, &VertexIndicies);

			TArray<FString> Indicies;
			InVertexIndicies.ParseIntoArray(Indicies, TEXT(" "), true);

			for (FString IndexStr : Indicies)
			{
				if (IndexStr.IsNumeric())
				{
					int32 Index = FCString::Atoi(*IndexStr);
					if (Index >= 0 && Index < NumVertices)
					{
						NewVertexSelection.SetSelected(Index);
					}
					else
					{
						// ERROR: INVALID INDEX
						FString ErrorStr = "Invalid specified index found.";
						UE_LOG(LogTemp, Error, TEXT("[Dataflow ERROR] %s"), *ErrorStr);
					}
				}
			}

			SetValue(Context, MoveTemp(NewVertexSelection), &VertexSelection);
		}
		else
		{
			SetValue(Context, FDataflowVertexSelection(), &VertexSelection);
		}
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionFaceSelectionCustomDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowFaceSelection>(&FaceSelection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if (InCollection.HasGroup(FGeometryCollection::GeometryGroup))
		{
			const int32 NumFaces = InCollection.NumElements(FGeometryCollection::FacesGroup);

			FDataflowFaceSelection NewFaceSelection;
			NewFaceSelection.Initialize(NumFaces, false);

			const FString InFaceIndicies = GetValue<FString>(Context, &FaceIndicies);

			TArray<FString> Indicies;
			InFaceIndicies.ParseIntoArray(Indicies, TEXT(" "), true);

			for (FString IndexStr : Indicies)
			{
				if (IndexStr.IsNumeric())
				{
					int32 Index = FCString::Atoi(*IndexStr);
					if (Index >= 0 && Index < NumFaces)
					{
						NewFaceSelection.SetSelected(Index);
					}
					else
					{
						// ERROR: INVALID INDEX
						FString ErrorStr = "Invalid specified index found.";
						UE_LOG(LogTemp, Error, TEXT("[Dataflow ERROR] %s"), *ErrorStr);
					}
				}
			}

			SetValue(Context, MoveTemp(NewFaceSelection), &FaceSelection);
		}
		else
		{
			SetValue(Context, FDataflowFaceSelection(), &FaceSelection);
		}
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionSelectionConvertDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowTransformSelection>(&TransformSelection))
	{
		if (IsConnected<FDataflowVertexSelection>(&VertexSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			const FDataflowVertexSelection& InVertexSelection = GetValue<FDataflowVertexSelection>(Context, &VertexSelection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertVertexSelectionToTransformSelection(InVertexSelection.AsArray(), bAllElementsMustBeSelected);

			FDataflowTransformSelection NewTransformSelection;
			NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
			NewTransformSelection.SetFromArray(SelectionArr);

			SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
		}
		else if (IsConnected<FDataflowFaceSelection>(&FaceSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			const FDataflowFaceSelection& InFaceSelection = GetValue<FDataflowFaceSelection>(Context, &FaceSelection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertFaceSelectionToTransformSelection(InFaceSelection.AsArray(), bAllElementsMustBeSelected);

			FDataflowTransformSelection NewTransformSelection;
			NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
			NewTransformSelection.SetFromArray(SelectionArr);

			SetValue(Context, MoveTemp(NewTransformSelection), &TransformSelection);
		}
		else if (IsConnected<FDataflowTransformSelection>(&TransformSelection))
		{
			// Passthrough
			const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);
			SetValue(Context, InTransformSelection, &TransformSelection);
		}
	}
	else if (Out->IsA<FDataflowFaceSelection>(&FaceSelection))
	{
		if (IsConnected<FDataflowVertexSelection>(&VertexSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			const FDataflowVertexSelection& InVertexSelection = GetValue<FDataflowVertexSelection>(Context, &VertexSelection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertVertexSelectionToFaceSelection(InVertexSelection.AsArray(), bAllElementsMustBeSelected);

			FDataflowFaceSelection NewFaceSelection;
			NewFaceSelection.Initialize(InCollection.NumElements(FGeometryCollection::FacesGroup), false);
			NewFaceSelection.SetFromArray(SelectionArr);

			SetValue(Context, MoveTemp(NewFaceSelection), &FaceSelection);
		}
		else if (IsConnected<FDataflowFaceSelection>(&FaceSelection))
		{
			// Passthrough
			const FDataflowFaceSelection& InFaceSelection = GetValue<FDataflowFaceSelection>(Context, &FaceSelection);
			SetValue(Context, InFaceSelection, &FaceSelection);
		}
		else if (IsConnected<FDataflowTransformSelection>(&TransformSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertTransformSelectionToFaceSelection(InTransformSelection.AsArray());

			FDataflowFaceSelection NewFaceSelection;
			NewFaceSelection.Initialize(InCollection.NumElements(FGeometryCollection::FacesGroup), false);
			NewFaceSelection.SetFromArray(SelectionArr);

			SetValue(Context, MoveTemp(NewFaceSelection), &FaceSelection);
		}
	}
	else if (Out->IsA<FDataflowVertexSelection>(&VertexSelection))
	{
		if (IsConnected<FDataflowVertexSelection>(&VertexSelection))
		{
			// Passthrough
			const FDataflowVertexSelection& InVertexSelection = GetValue<FDataflowVertexSelection>(Context, &VertexSelection);
			SetValue(Context, InVertexSelection, &VertexSelection);
		}
		else if (IsConnected<FDataflowFaceSelection>(&FaceSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			const FDataflowFaceSelection& InFaceSelection = GetValue<FDataflowFaceSelection>(Context, &FaceSelection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertFaceSelectionToVertexSelection(InFaceSelection.AsArray());

			FDataflowVertexSelection NewVertexSelection;
			NewVertexSelection.Initialize(InCollection.NumElements(FGeometryCollection::VerticesGroup), false);
			NewVertexSelection.SetFromArray(SelectionArr);

			SetValue(Context, MoveTemp(NewVertexSelection), &VertexSelection);
		}
		else if (IsConnected<FDataflowTransformSelection>(&TransformSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertTransformSelectionToVertexSelection(InTransformSelection.AsArray());

			FDataflowVertexSelection NewVertexSelection;
			NewVertexSelection.Initialize(InCollection.NumElements(FGeometryCollection::VerticesGroup), false);
			NewVertexSelection.SetFromArray(SelectionArr);

			SetValue(Context, MoveTemp(NewVertexSelection), &VertexSelection);
		}
	}
	else if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FCollectionFaceSelectionInvertDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowFaceSelection>(&FaceSelection))
	{
		FDataflowFaceSelection InFaceSelection = GetValue<FDataflowFaceSelection>(Context, &FaceSelection);

		InFaceSelection.Invert();

		SetValue<FDataflowFaceSelection>(Context, MoveTemp(InFaceSelection), &FaceSelection);
	}
}


void FCollectionVertexSelectionByPercentageDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowVertexSelection>(&VertexSelection))
	{
		FDataflowVertexSelection InVertexSelection = GetValue<FDataflowVertexSelection>(Context, &VertexSelection);

		int32 InPercentage = GetValue<int32>(Context, &Percentage);
		float InRandomSeed = GetValue<float>(Context, &RandomSeed);

		TArray<int32> SelectionArr = InVertexSelection.AsArray();

		GeometryCollection::Facades::FCollectionTransformSelectionFacade::SelectByPercentage(SelectionArr, InPercentage, bDeterministic, InRandomSeed);

		InVertexSelection.SetFromArray(SelectionArr);
		SetValue(Context, MoveTemp(InVertexSelection), &VertexSelection);
	}
}


void FCollectionVertexSelectionSetOperationDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowVertexSelection>(&VertexSelection))
	{
		const FDataflowVertexSelection& InVertexSelectionA = GetValue<FDataflowVertexSelection>(Context, &VertexSelectionA);
		const FDataflowVertexSelection& InVertexSelectionB = GetValue<FDataflowVertexSelection>(Context, &VertexSelectionB);

		FDataflowVertexSelection NewVertexSelection;

		if (InVertexSelectionA.Num() == InVertexSelectionB.Num())
		{
			if (Operation == ESetOperationEnum::Dataflow_SetOperation_AND)
			{
				InVertexSelectionA.AND(InVertexSelectionB, NewVertexSelection);
			}
			else if (Operation == ESetOperationEnum::Dataflow_SetOperation_OR)
			{
				InVertexSelectionA.OR(InVertexSelectionB, NewVertexSelection);
			}
			else if (Operation == ESetOperationEnum::Dataflow_SetOperation_XOR)
			{
				InVertexSelectionA.XOR(InVertexSelectionB, NewVertexSelection);
			}
			else if (Operation == ESetOperationEnum::Dataflow_SetOperation_Subtract)
			{
				InVertexSelectionA.Subtract(InVertexSelectionB, NewVertexSelection);
			}
		}
		else
		{
			// ERROR: INPUT TRANSFORMSELECTIONS HAVE DIFFERENT NUMBER OF ELEMENTS
			FString ErrorStr = "Input VertexSelections have different number of elements.";
			UE_LOG(LogTemp, Error, TEXT("[Dataflow ERROR] %s"), *ErrorStr);
		}

		SetValue(Context, MoveTemp(NewVertexSelection), &VertexSelection);
	}
}

