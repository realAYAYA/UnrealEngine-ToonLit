// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "ClothEditorContextObject.generated.h"

class UDataflow;
class UEdGraphNode;

namespace UE::Chaos::ClothAsset
{
enum class EClothPatternVertexType : uint8;
}
struct FManagedArrayCollection;


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothEditorContextObject : public UObject
{
	GENERATED_BODY()

public:

	void Init(TWeakPtr<SDataflowGraphEditor> DataflowGraphEditor, UE::Chaos::ClothAsset::EClothPatternVertexType InConstructionViewMode, TWeakPtr<FManagedArrayCollection> SelectedClothCollection);

	/**
	* Get a single selected node of the specified type. Return nullptr if the specified node is not selected, or if multiple nodes are selected
	*/
	template<typename NodeType>
	NodeType* GetSingleSelectedNodeOfType() const
	{
		const TSharedPtr<SDataflowGraphEditor> GraphEditor = DataflowGraphEditor.Pin();

		if (UEdGraphNode* const SingleSelectedNode = GraphEditor->GetSingleSelectedNode())
		{
			UDataflowEdNode* const SelectedDataflowEdNode = CastChecked<UDataflowEdNode>(SingleSelectedNode);
			const TSharedPtr<FDataflowNode> DataflowNode = SelectedDataflowEdNode->GetDataflowNode();
			if (NodeType* const NodeTypeNode = DataflowNode->AsType<NodeType>())
			{
				return NodeTypeNode;
			}
		}

		return nullptr;
	}

	void SetClothCollection(UE::Chaos::ClothAsset::EClothPatternVertexType ViewMode, TWeakPtr<FManagedArrayCollection> ClothCollection);

	const TWeakPtr<const FManagedArrayCollection> GetSelectedClothCollection() const { return SelectedClothCollection; }
	UE::Chaos::ClothAsset::EClothPatternVertexType GetConstructionViewMode() const { return ConstructionViewMode; }
private:

	TWeakPtr<SDataflowGraphEditor> DataflowGraphEditor;

	UE::Chaos::ClothAsset::EClothPatternVertexType ConstructionViewMode;
	TWeakPtr<const FManagedArrayCollection> SelectedClothCollection;
};


