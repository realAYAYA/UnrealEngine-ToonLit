// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEdNode.h"

#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowRenderingFactory.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"
#include "Logging/LogMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowEdNode)

#if WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "EdGraph/EdGraphPin.h"
#endif

#define LOCTEXT_NAMESPACE "DataflowEdNode"

DEFINE_LOG_CATEGORY_STATIC(DATAFLOWNODE_LOG, Error, All);


UDataflowEdNode::UDataflowEdNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bCanRenameNode = true;
#endif // WITH_EDITOR && !UE_BUILD_SHIPPING
}

void UDataflowEdNode::SetAssetRender(bool bInRender)
{
	bRenderInAssetEditor = bInRender;
	if (IsBound())
	{
#if WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (UDataflow* DataflowObject = Cast< UDataflow>(GetGraph()))
		{
			if (bRenderInAssetEditor)
				DataflowObject->AddRenderTarget(this);
			else
				DataflowObject->RemoveRenderTarget(this);
		}
#endif
	}
}


TSharedPtr<FDataflowNode> UDataflowEdNode::GetDataflowNode()
{
	if(TSharedPtr<Dataflow::FGraph> Dataflow = GetDataflowGraph())
	{
		return Dataflow->FindBaseNode(GetDataflowNodeGuid());
	}
	return TSharedPtr<FDataflowNode>(nullptr);
}

TSharedPtr<const FDataflowNode> UDataflowEdNode::GetDataflowNode() const
{
	if (TSharedPtr<const Dataflow::FGraph> Dataflow = GetDataflowGraph())
	{
		return Dataflow->FindBaseNode(GetDataflowNodeGuid());
	}
	return TSharedPtr<FDataflowNode>(nullptr);
}


void UDataflowEdNode::AllocateDefaultPins()
{
	UE_LOG(DATAFLOWNODE_LOG, Verbose, TEXT("UDataflowEdNode::AllocateDefaultPins()"));
	// called on node creation from UI. 

#if WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DataflowGraph)
	{
		if (DataflowNodeGuid.IsValid())
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(DataflowNodeGuid))
			{
				for (const Dataflow::FPin& Pin : DataflowNode->GetPins())
				{
					if (Pin.Direction == Dataflow::FPin::EDirection::INPUT)
					{
						CreatePin(EEdGraphPinDirection::EGPD_Input, Pin.Type, Pin.Name);
					}					
					if (Pin.Direction == Dataflow::FPin::EDirection::OUTPUT)
					{
						CreatePin(EEdGraphPinDirection::EGPD_Output, Pin.Type, Pin.Name);
					}
				}
			}
		}
	}
#endif // WITH_EDITOR && !UE_BUILD_SHIPPING
}


FText UDataflowEdNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(GetName());
}

#if WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UDataflowEdNode::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (ensure(IsBound()))
	{
		if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(DataflowNodeGuid))
		{
			if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
			{
				if (FDataflowInput* ConnectionInput = DataflowNode->FindInput(FName(Pin->GetName())))
				{
					DataflowGraph->ClearConnections(ConnectionInput);
					for (UEdGraphPin* LinkedCon : Pin->LinkedTo)
					{
						if (UDataflowEdNode* LinkedNode = Cast<UDataflowEdNode>(LinkedCon->GetOwningNode()))
						{
							if (ensure(LinkedNode->IsBound()))
							{
								if (TSharedPtr<FDataflowNode> LinkedDataflowNode = DataflowGraph->FindBaseNode(LinkedNode->GetDataflowNodeGuid()))
								{
									if (FDataflowOutput* LinkedConOutput = LinkedDataflowNode->FindOutput(FName(LinkedCon->GetName())))
									{
										DataflowGraph->Connect(LinkedConOutput, ConnectionInput);
									}
								}
							}
						}
					}
				}
			}
			else if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
			{
				if (FDataflowOutput* ConnectionOutput = DataflowNode->FindOutput(FName(Pin->GetName())))
				{
					DataflowGraph->ClearConnections(ConnectionOutput);
					for (UEdGraphPin* LinkedCon : Pin->LinkedTo)
					{
						if (UDataflowEdNode* LinkedNode = Cast<UDataflowEdNode>(LinkedCon->GetOwningNode()))
						{
							if (ensure(LinkedNode->IsBound()))
							{
								if (TSharedPtr<FDataflowNode> LinkedDataflowNode = DataflowGraph->FindBaseNode(LinkedNode->GetDataflowNodeGuid()))
								{
									if (FDataflowInput* LinkedConInput = LinkedDataflowNode->FindInput(FName(LinkedCon->GetName())))
									{
										DataflowGraph->Connect(ConnectionOutput, LinkedConInput);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	Super::PinConnectionListChanged(Pin);
}

#endif // WITH_EDITOR && !UE_BUILD_SHIPPING

void UDataflowEdNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << DataflowNodeGuid;
}

#if WITH_EDITOR

FLinearColor UDataflowEdNode::GetNodeTitleColor() const
{
	if (DataflowGraph)
	{
		if (DataflowNodeGuid.IsValid())
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(DataflowNodeGuid))
			{
				return Dataflow::FNodeColorsRegistry::Get().GetNodeTitleColor(DataflowNode->GetCategory());
			}
		}
	}
	return FDataflowNode::DefaultNodeTitleColor;
}

FLinearColor UDataflowEdNode::GetNodeBodyTintColor() const
{
	if (DataflowGraph)
	{
		if (DataflowNodeGuid.IsValid())
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(DataflowNodeGuid))
			{
				return Dataflow::FNodeColorsRegistry::Get().GetNodeBodyTintColor(DataflowNode->GetCategory());
			}
		}
	}
	return FDataflowNode::DefaultNodeBodyTintColor;
}

FText UDataflowEdNode::GetTooltipText() const
{
	if (DataflowGraph)
	{
		if (DataflowNodeGuid.IsValid())
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(DataflowNodeGuid))
			{
				return FText::FromString(DataflowNode->GetToolTip());
			}
		}
	}

	return FText::FromString("");

}

void UDataflowEdNode::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	if (DataflowGraph)
	{
		if (DataflowNodeGuid.IsValid())
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(DataflowNodeGuid))
			{		
				FString MetaDataStr;
				
				TArray<FString> PinMetaData = DataflowNode->GetPinMetaData(Pin.PinName);
			
				if (Pin.Direction == EGPD_Input && PinMetaData.Contains(FDataflowNode::DataflowIntrinsic.ToString()))
				{
					MetaDataStr = "[Intrinsic]";
				}
				if (Pin.Direction == EGPD_Output && PinMetaData.Contains(FDataflowNode::DataflowPassthrough.ToString()))
				{
					MetaDataStr = "[Passthrough]";
				}

				FString NameStr = Pin.PinName.ToString();
				if (MetaDataStr.Len() > 0)
				{
					NameStr.Appendf(TEXT(" %s"), *MetaDataStr);
				}

				HoverTextOut.Appendf(TEXT("%s\n%s\n\n%s"), *NameStr, *Pin.PinType.PinCategory.ToString(), *DataflowNode->GetPinToolTip(Pin.PinName));
			}
		}
	}
}

#endif


TArray<Dataflow::FRenderingParameter> UDataflowEdNode::GetRenderParameters() const
{
	if (TSharedPtr<const FDataflowNode> DataflowNode = GetDataflowNode())
	{
		return DataflowNode->GetRenderParameters();
	}
	return 	TArray<Dataflow::FRenderingParameter>();
}


bool UDataflowEdNode::Render(GeometryCollection::Facades::FRenderingFacade& RenderData, TSharedPtr<Dataflow::FContext> Context) const
{
	bool bNeedsRefresh = false;
	if (DataflowGraph)
	{
		if (TSharedPtr<const FDataflowNode> NodeTarget = DataflowGraph->FindBaseNode(FName(GetName())))
		{
			if (Dataflow::FRenderingFactory* Factory = Dataflow::FRenderingFactory::GetInstance())
			{
				if (GetRenderParameters().Num())
				{
					int32 GeometryIndex = RenderData.StartGeometryGroup(GetDataflowNodeGuid().ToString());
					for (Dataflow::FRenderingParameter& Parameter : GetRenderParameters())
					{
						Factory->RenderNodeOutput(RenderData, {NodeTarget.Get(), Parameter, *Context});
						bNeedsRefresh = true;
					}
					RenderData.EndGeometryGroup(GeometryIndex);
				}
			}
		}
	}
	return bNeedsRefresh;
}

#undef LOCTEXT_NAMESPACE


