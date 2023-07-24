// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowRenderingFactory.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "GeometryCollection/GeometryCollection.h"
#include "Internationalization/Text.h"
#include "Misc/Build.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "DataflowEdNode.generated.h"

class FArchive;
class UEdGraphPin;
class UObject;
namespace Dataflow { class FGraph; class FRenderingParameters; }
namespace GeometryCollection::Facades { class FRenderingFacade; }

UCLASS()
class DATAFLOWENGINE_API UDataflowEdNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	FGuid DataflowNodeGuid;
	TSharedPtr<Dataflow::FGraph> DataflowGraph;

public:

	// UEdGraphNode interface
	virtual void AllocateDefaultPins();
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const;
#if WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
#endif // WITH_EDITOR && !UE_BUILD_SHIPPING
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FLinearColor GetNodeBodyTintColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
#endif // WITH_EDITOR
	// End of UEdGraphNode interface

	// UObject interface
	void Serialize(FArchive& Ar);
	// End UObject interface

	bool IsBound() { return DataflowGraph && DataflowNodeGuid.IsValid(); }

	TSharedPtr<Dataflow::FGraph> GetDataflowGraph() { return DataflowGraph; }
	TSharedPtr<const Dataflow::FGraph> GetDataflowGraph() const { return DataflowGraph; }
	void SetDataflowGraph(TSharedPtr<Dataflow::FGraph> InDataflowGraph) { DataflowGraph = InDataflowGraph; }

	FGuid GetDataflowNodeGuid() const { return DataflowNodeGuid; }
	void SetDataflowNodeGuid(FGuid InGuid) { DataflowNodeGuid = InGuid; }

	TSharedPtr<FDataflowNode> GetDataflowNode();
	TSharedPtr<const FDataflowNode> GetDataflowNode() const;

	//
	// Node Rendering
	//
	void SetAssetRender(bool bInRender);
	bool DoAssetRender() { return bRenderInAssetEditor; }
	TArray<Dataflow::FRenderingParameter> GetRenderParameters() const;
	virtual bool Render(GeometryCollection::Facades::FRenderingFacade& RenderData, TSharedPtr<Dataflow::FContext> Context) const;

	UPROPERTY()
	bool bRenderInAssetEditor = false;

};

