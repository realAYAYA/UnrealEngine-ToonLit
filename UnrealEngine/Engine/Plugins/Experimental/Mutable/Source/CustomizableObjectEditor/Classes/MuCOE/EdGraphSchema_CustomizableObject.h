// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2_Actions.h"
#include "HAL/PlatformMath.h"
#include "Internationalization/Text.h"
#include "MaterialTypes.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "EdGraphSchema_CustomizableObject.generated.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UObject;
struct FAssetData;
struct FEdGraphPinType;

/** Action to add a node to the graph */
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	/** Template of node we want to create */
	UPROPERTY()
	TObjectPtr<class UEdGraphNode> NodeTemplate;


	FCustomizableObjectSchemaAction_NewNode() 
		: FEdGraphSchemaAction()
		, NodeTemplate(NULL)
	{}

	FCustomizableObjectSchemaAction_NewNode(const FString& InNodeCategory, const FText& InMenuDesc, const FText& InToolTip, const int32 InGrouping, const FText& InKeywords = FText(), int32 InSectionID = 0)
		: FEdGraphSchemaAction(FText::FromString(InNodeCategory), InMenuDesc, InToolTip, InGrouping, InKeywords, InSectionID)
		, NodeTemplate(NULL)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** Reimplementaiton of EdGraphSchema::CreateNode(...). Performs the overlap calculation before calling AutowireNewNode(...). AutowireNewNode can induce a call to a ReconstrucNode() which removes pins required for the calculation. */
	static UEdGraphNode* CreateNode(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, class UEdGraphNode* InNodeTemplate);
	// End of FEdGraphSchemaAction interface

	template <typename NodeType>
	static NodeType* InstantSpawn(class UEdGraph* ParentGraph, NodeType* InTemplateNode, const FVector2D Location)
	{
		FEdGraphSchemaAction_K2NewNode Action;
		Action.NodeTemplate = InTemplateNode;

		return Cast<NodeType>(Action.PerformAction(ParentGraph, NULL, Location));
	}
};


/** Action to paste clipboard contents into the graph */
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectSchemaAction_Paste : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	// Simple type info
	static FName StaticGetTypeId() { static FName Type("FCustomizableObjectSchemaAction_Paste"); return Type; }
	virtual FName GetTypeId() const override { return StaticGetTypeId(); }

	FCustomizableObjectSchemaAction_Paste()
		: FEdGraphSchemaAction()
	{}

	FCustomizableObjectSchemaAction_Paste(const FText& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, FText::FromString(InToolTip), InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UEdGraphSchema_CustomizableObject : public UEdGraphSchema
{
public:
	GENERATED_BODY()

	UEdGraphSchema_CustomizableObject();

	// Allowable PinType.PinCategory values
	static const FName PC_Object;
	static const FName PC_Material;
	static const FName PC_Mesh;
	static const FName PC_Layout;
	static const FName PC_Image;
	static const FName PC_Projector;
	static const FName PC_GroupProjector;
	static const FName PC_Color;
	static const FName PC_Float;
	static const FName PC_Bool;
	static const FName PC_Enum;
	static const FName PC_Stack;
	static const FName PC_MaterialAsset;
	
	// Begin EdGraphSchema interface
	virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const override;
	void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const override;

	FLinearColor GetPinTypeColor(const FName& PinType) const;

	void GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	// End EdGraphSchema interface

	/** Utility to create a node creating action */
	static TSharedPtr<FCustomizableObjectSchemaAction_NewNode> AddNewNodeAction(FGraphActionListBuilderBase& ContextMenuBuilder, const FString& Category, const FText& MenuDesc, const FText& Tooltip, const int32 Grouping = 0, const FString& Keywords = FString());

	/** */
	void GetBreakLinkToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin ) const;

private: 
/** Enum containing all the object types that we are able to convert onto a node when dragging
 * and dropping and asset of that type onto the CO graph.
 * Each value of this enumeration will, in practice, have a CO node to be represented by.
 */
// TODO: Replace the usage of this enum class with something similar to typeID (not casting)
enum class ESpawnableObjectType : int32
{
	None = -1,		// Invalid value
	
	UTexture2D,
	USkeletalMesh,
	UStaticMesh,
	UMaterialInterface,
};

public:

	/**
	 * Provided an asset object check if mutable does have a relative node to be able to host this asset's data.
	 *  @param InAsset - The asset object we want to check if can be used to generate a new mutable node containing it.
	 *  @param OutObjectType - The type of object of the asset.
	 *  @return True if the asset can be used to spawn a mutable node, false if not.
	 *  @note All possible object types that we can convert onto mutable nodes must be defined here.
	 */
	bool IsSpawnableAsset(const FAssetData& InAsset, ESpawnableObjectType& OutObjectType) const;

	static TSharedPtr<class ICustomizableObjectEditor> GetCustomizableObjectEditor(const class UEdGraph* ParentGraph);
	UEdGraphNode* AddComment(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) const;

	/**
	 * Break non allowed connections once the connection has been created.
	 * Allows the nodes to have greater connectivity granularity than simply forbidding to create a connection with UEdGraphSchema_CustomizableObject::CanCreateConnection.
	 */ 
	virtual bool TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const override;

	/** Given a pin category name, get its friendly name (user readable). */
	static FText GetPinCategoryName(const FName& PinCategory);

	/** Given a Material parameter type, returns the Mutable pin category. */
	static const FName& GetPinCategory(EMaterialParameterType Type);
};

