// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "SGraphNode.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "CustomizableObjectNodeStaticMesh.generated.h"

class FArchive;
class FAssetThumbnail;
class FAssetThumbnailPool;
class SOverlay;
class SVerticalBox;
class UCustomizableObjectLayout;
class UCustomizableObjectNodeRemapPins;
class UCustomizableObjectNodeRemapPinsByName;
class UMaterialInterface;
class UObject;
class UStaticMesh;
class UTexture2D;
struct FPropertyChangedEvent;
struct FSlateBrush;


// Class to render the Static Mesh thumbnail of a CustomizableObjectNodeStaticMesh
class SGraphNodeStaticMesh : public SGraphNode
{
public:

	SLATE_BEGIN_ARGS(SGraphNodeStaticMesh) {}
	SLATE_END_ARGS();

	SGraphNodeStaticMesh() : SGraphNode() {};

	// Builds the SGraphNodeStaticMesh when needed
	void Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode);

	// Calls the needed functions to build the SGraphNode widgets
	void UpdateGraphNode();

	// Overriden functions to build the SGraphNode widgets
	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override;
	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
	virtual bool ShouldAllowCulling() const override { return false; }

	// Callbacks for the widget
	void OnExpressionPreviewChanged(const ECheckBoxState NewCheckedState);
	ECheckBoxState IsExpressionPreviewChecked() const;
	const FSlateBrush* GetExpressionPreviewArrow() const;
	EVisibility ExpressionPreviewVisibility() const;

public:

	// Single property that only draws the combo box widget of the static mesh
	TSharedPtr<class ISinglePropertyView> StaticMeshSelector;

	// Pointer to the NodeStaticMesh that owns this SGraphNode
	class UCustomizableObjectNodeStaticMesh* NodeStaticMesh;

private:

	// Classes needed to get and render the thumbnail of the Static Mesh
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	//This parameter defines the size of the thumbnail widget inside the Node
	float WidgetSize;

	// This parameter defines the resolution of the thumbnail
	uint32 ThumbnailSize;

};


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeStaticMeshMaterial
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TObjectPtr<UEdGraphPin_Deprecated> MeshPin = nullptr;

	UPROPERTY()
	TObjectPtr<UEdGraphPin_Deprecated> LayoutPin = nullptr;

	UPROPERTY()
	TArray< TObjectPtr<UEdGraphPin_Deprecated>> ImagePins;

	UPROPERTY()
	FEdGraphPinReference MeshPinRef;

	UPROPERTY()
	FEdGraphPinReference LayoutPinRef;

	UPROPERTY()
	TArray<FEdGraphPinReference> ImagePinsRef;
};


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeStaticMeshLOD
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FCustomizableObjectNodeStaticMeshMaterial> Materials;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeStaticMesh : public UCustomizableObjectNodeMesh
{ 
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TObjectPtr<UStaticMesh> StaticMesh;

	/** Images */
	UPROPERTY()
	TArray<FCustomizableObjectNodeStaticMeshLOD> LODs;

	/** Default pin when there is no mesh. */
	UPROPERTY()
	FEdGraphPinReference DefaultPin;
	
	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Serialize(FArchive& Ar) override;

	// UEdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	bool ProvidesCustomPinRelevancyTest() const override { return true; }
	bool IsPinRelevant(const UEdGraphPin* Pin) const override;
	UCustomizableObjectNodeRemapPinsByName* CreateRemapPinsByName() const override;

	bool IsNodeOutDatedAndNeedsRefresh() override;
	FString GetRefreshMessage() const override;

	// UCustomizableObjectNodeMesh interface
	virtual UTexture2D* FindTextureForPin(const UEdGraphPin* Pin) const override;
	virtual void GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVIndex) const override;
	virtual TArray<UCustomizableObjectLayout*> GetLayouts(const UEdGraphPin* OutPin) const override;
	virtual UObject* GetMesh() const override;
	virtual UEdGraphPin* GetMeshPin(int32 LOD, int MaterialIndex) const override;
	virtual void GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const override;

	/** Returns the material assossiated to the given output pin. */
	UMaterialInterface* GetMaterialFor(const UEdGraphPin* Pin) const;

	// Creates the SGraph Node widget for the thumbnail
	TSharedPtr<SGraphNode> CreateVisualWidget() override;

	// Determines if the Node is collapsed or not
	bool bCollapsed = true;

	// Pointer to the SGraphNodeStaticMesh
	TWeakPtr< SGraphNodeStaticMesh > GraphNodeStaticMesh;

};

