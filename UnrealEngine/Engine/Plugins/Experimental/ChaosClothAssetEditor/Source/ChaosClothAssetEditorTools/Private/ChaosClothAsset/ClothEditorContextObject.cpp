// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditorContextObject.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothEditorContextObject)

void UClothEditorContextObject::Init(TWeakPtr<SDataflowGraphEditor> InDataflowGraphEditor,UE::Chaos::ClothAsset::EClothPatternVertexType InConstructionViewMode, TWeakPtr<FManagedArrayCollection> InSelectedClothCollection)
{
	DataflowGraphEditor = InDataflowGraphEditor;
	ConstructionViewMode = InConstructionViewMode;
	SelectedClothCollection = InSelectedClothCollection;
}

void UClothEditorContextObject::SetClothCollection(UE::Chaos::ClothAsset::EClothPatternVertexType ViewMode, TWeakPtr<FManagedArrayCollection> ClothCollection)
{
	ConstructionViewMode = ViewMode;
	SelectedClothCollection = ClothCollection;
}