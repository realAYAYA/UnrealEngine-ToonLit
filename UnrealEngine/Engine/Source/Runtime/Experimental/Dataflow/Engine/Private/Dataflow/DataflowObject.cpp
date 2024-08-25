// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowObjectInterface.h"

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

void UDataflow::EvaluateTerminalNodeByName(FName NodeName, UObject* Asset)
{
	ensureAlwaysMsgf(false, TEXT("Deprecated use the dataflow blueprint library from now on"));
}

void UDataflow::PostEditCallback()
{
	// mark as dirty for the UObject
}

void UDataflow::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	UDataflow* const This = CastChecked<UDataflow>(InThis);
	This->Dataflow->AddReferencedObjects(Collector);
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
		UDataflowEdNode* DataflowEdNode = Cast<UDataflowEdNode>(EdNode);

		// Not all nodes are UDataflowEdNode (There is now UDataflowEdNodeComment)
		if (DataflowEdNode)
		{
			DataflowEdNode->SetDataflowGraph(Dataflow);
			DataflowEdNode->UpdatePinsFromDataflowNode();
		}

		if (DisabledNodes.Contains(FName(EdNode->GetName())))
		{
			EdNode->SetEnabledState(ENodeEnabledState::Disabled);
		}
		else
		{
			if (DataflowEdNode)
			{
				if (DataflowEdNode->DoAssetRender())
				{
					RenderTargets.Add(DataflowEdNode);
				}
			}
		}
	}
#endif

	LastModifiedRenderTarget = Dataflow::FTimestamp::Current();
	UObject::PostLoad();
}

void UDataflow::AddRenderTarget(UDataflowEdNode* InNode)
{
	LastModifiedRenderTarget = Dataflow::FTimestamp::Current();
	InNode->bRenderInAssetEditor = true;
	RenderTargets.AddUnique(InNode);
}

void UDataflow::RemoveRenderTarget(UDataflowEdNode* InNode)
{
	LastModifiedRenderTarget = Dataflow::FTimestamp::Current();
	InNode->bRenderInAssetEditor = false;
	RenderTargets.Remove(InNode);
}


void UDataflow::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Dataflow->Serialize(Ar, this);
}

#undef LOCTEXT_NAMESPACE

