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

UCLASS(MinimalAPI)
class UDataflowEdNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	FGuid DataflowNodeGuid;
	TSharedPtr<Dataflow::FGraph> DataflowGraph;

public:

	// UEdGraphNode interface
	DATAFLOWENGINE_API virtual void AllocateDefaultPins();
	DATAFLOWENGINE_API virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const;
#if WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DATAFLOWENGINE_API virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
#endif // WITH_EDITOR && !UE_BUILD_SHIPPING
#if WITH_EDITOR
	DATAFLOWENGINE_API virtual FLinearColor GetNodeTitleColor() const override;
	DATAFLOWENGINE_API virtual FLinearColor GetNodeBodyTintColor() const override;
	DATAFLOWENGINE_API virtual FText GetTooltipText() const override;
	DATAFLOWENGINE_API virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	DATAFLOWENGINE_API virtual FText GetPinDisplayName(const UEdGraphPin* Pin) const override;
	DATAFLOWENGINE_API virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	DATAFLOWENGINE_API virtual void OnPinRemoved(UEdGraphPin* InRemovedPin) override;
#endif // WITH_EDITOR
	// End of UEdGraphNode interface

	// UObject interface
	DATAFLOWENGINE_API void Serialize(FArchive& Ar);
	// End UObject interface

	bool IsBound() { return DataflowGraph && DataflowNodeGuid.IsValid(); }

	TSharedPtr<Dataflow::FGraph> GetDataflowGraph() { return DataflowGraph; }
	TSharedPtr<const Dataflow::FGraph> GetDataflowGraph() const { return DataflowGraph; }
	void SetDataflowGraph(TSharedPtr<Dataflow::FGraph> InDataflowGraph) { DataflowGraph = InDataflowGraph; }

	DATAFLOWENGINE_API void UpdatePinsFromDataflowNode();

	FGuid GetDataflowNodeGuid() const { return DataflowNodeGuid; }
	void SetDataflowNodeGuid(FGuid InGuid) { DataflowNodeGuid = InGuid; }

	DATAFLOWENGINE_API TSharedPtr<FDataflowNode> GetDataflowNode();
	DATAFLOWENGINE_API TSharedPtr<const FDataflowNode> GetDataflowNode() const;

	/** Add a new option pin if the underlying Dataflow node AddPin member is overriden. */
	DATAFLOWENGINE_API void AddOptionPin();
	/** Remove an option pin if the underlying Dataflow node RemovePin member is overriden. */
	DATAFLOWENGINE_API void RemoveOptionPin();

	//
	// Node Rendering
	//
	DATAFLOWENGINE_API void SetAssetRender(bool bInRender);
	bool DoAssetRender() { return bRenderInAssetEditor; }
	DATAFLOWENGINE_API TArray<Dataflow::FRenderingParameter> GetRenderParameters() const;
	DATAFLOWENGINE_API virtual bool Render(GeometryCollection::Facades::FRenderingFacade& RenderData, const TSharedPtr<Dataflow::FContext> Context) const;

	UPROPERTY()
	bool bRenderInAssetEditor = false;

};

