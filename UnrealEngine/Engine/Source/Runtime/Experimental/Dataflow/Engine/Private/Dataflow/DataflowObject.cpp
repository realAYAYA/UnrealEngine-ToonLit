// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEdNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowObject)


#define LOCTEXT_NAMESPACE "UDataflow"

FDataflowAssetEdit::FDataflowAssetEdit(UDataflow* InAsset, FPostEditFunctionCallback InCallback)
	: PostEditCallback(InCallback)
	, Asset(InAsset)
{
}

FDataflowAssetEdit::~FDataflowAssetEdit()
{
	PostEditCallback();
}

Dataflow::FGraph* FDataflowAssetEdit::GetGraph()
{
	if (Asset)
	{
		return Asset->Dataflow.Get();
	}
	return nullptr;
}

UDataflow::UDataflow(const FObjectInitializer& ObjectInitializer)
	: UEdGraph(ObjectInitializer)
	, Dataflow(new Dataflow::FGraph())
{}


void UDataflow::PostEditCallback()
{
	// mark as dirty for the UObject
}

#if WITH_EDITOR

void UDataflow::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

void UDataflow::PostLoad()
{
#if WITH_EDITOR
	const TSet<FName>& DisabledNodes = Dataflow->GetDisabledNodes();

	for (UEdGraphNode* EdNode : Nodes)
	{
		// Not all nodes are UDataflowEdNode (There is now UDataflowEdNodeComment)
		if (UDataflowEdNode* DataflowEdNode = Cast<UDataflowEdNode>(EdNode))
		{
			DataflowEdNode->SetDataflowGraph(Dataflow);
		}

		if (DisabledNodes.Contains(FName(EdNode->GetName())))
		{
			EdNode->SetEnabledState(ENodeEnabledState::Disabled);
		}
	}
#endif

	UObject::PostLoad();
}

void UDataflow::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << *Dataflow.Get();
}

#undef LOCTEXT_NAMESPACE

