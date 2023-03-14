// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "GameplayTagContainer.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "SGraphNode.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "CustomizableObjectNodeSkeletalMesh.generated.h"

class FArchive;
class FAssetThumbnail;
class FAssetThumbnailPool;
class FSkeletalMeshModel;
class ISinglePropertyView;
class SOverlay;
class SVerticalBox;
class UAnimInstance;
class UCustomizableObjectLayout;
class UCustomizableObjectNodeRemapPins;
class UCustomizableObjectNodeRemapPinsByName;
class UMaterialInterface;
class UObject;
class USkeletalMesh;
class UTexture2D;
struct FPropertyChangedEvent;
struct FSkeletalMaterial;
struct FSlateBrush;


// Class to render the Skeletal Mesh thumbnail of a CustomizableObjectNodeSkeletalMesh
class SGraphNodeSkeletalMesh : public SGraphNode
{
public:

	SLATE_BEGIN_ARGS(SGraphNodeSkeletalMesh) {}
	SLATE_END_ARGS();

	SGraphNodeSkeletalMesh() : SGraphNode() {};

	// Builds the SGraphNodeSkeletalMesh when needed
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

	// Single property that only draws the combo box widget of the skeletal mesh
	TSharedPtr<ISinglePropertyView> SkeletalMeshSelector;

	// Pointer to the NodeSkeletalMesh that owns this SGraphNode
	class UCustomizableObjectNodeSkeletalMesh* NodeSkeletalMesh;

private:
	
	// Classes needed to get and render the thumbnail of the Skeletal Mesh
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	//This parameter defines the size of the thumbnail widget inside the Node
	float WidgetSize;

	// This parameter defines the resolution of the thumbnail
	uint32 ThumbnailSize;

};



USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeSkeletalMeshMaterial
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TObjectPtr<UEdGraphPin_Deprecated> MeshPin = nullptr;

	UPROPERTY()
	TArray< TObjectPtr<UEdGraphPin_Deprecated> > LayoutPins;

	UPROPERTY()
	TArray< TObjectPtr<UEdGraphPin_Deprecated> > ImagePins;

	UPROPERTY()
	FEdGraphPinReference MeshPinRef;

	UPROPERTY()
	TArray<FEdGraphPinReference> LayoutPinsRef;

	UPROPERTY()
	TArray<FEdGraphPinReference> ImagePinsRef;
};


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeSkeletalMeshLOD
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FCustomizableObjectNodeSkeletalMeshMaterial> Materials;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeSkeletalMesh : public UCustomizableObjectNodeMesh
{
public:
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Images */
	UPROPERTY()
	TArray<FCustomizableObjectNodeSkeletalMeshLOD> LODs;

	/** Default pin when there is no mesh. */
	UPROPERTY()
	FEdGraphPinReference DefaultPin;
	
	/** Morphs */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FString> UsedRealTimeMorphTargetNames;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
    bool bUseAllRealTimeMorphs = false; 
	
	/** The anim instance that will be gathered by a Generated instance if it contains this skeletal mesh part, 
		it will be grouped by component and AnimBlueprintSlot (the next UProperty). */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TSoftClassPtr<UAnimInstance> AnimInstance;

	/** The anim slot associated with the AnimInstance */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int32 AnimBlueprintSlot = 0;

	/** Animation tags that will be gathered by a Generated instance if it contains this skeletal mesh part,
		it will not be grouped by component or AnimBlueprintSlot */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FGameplayTagContainer AnimationGameplayTags;

	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Serialize(FArchive& Ar) override;

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual UCustomizableObjectNodeRemapPinsByName* CreateRemapPinsByName() const override;

	// UCustomizableObjectNodeMesh interface
	virtual UTexture2D* FindTextureForPin(const UEdGraphPin* Pin) const override;
	virtual void GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVIndex) const override;
	virtual TArray<UCustomizableObjectLayout*> GetLayouts(const UEdGraphPin* OutPin) const override;
	virtual UObject* GetMesh() const override;
	virtual UEdGraphPin* GetMeshPin(int32 LOD, int MaterialIndex) const override;
	virtual void GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const override;
	
	/** Returns the material associated to the given output pin. */
	UMaterialInterface* GetMaterialFor(const UEdGraphPin* Pin) const;
	FSkeletalMaterial* GetSkeletalMaterialFor(const UEdGraphPin* Pin) const;

	virtual bool ProvidesCustomPinRelevancyTest() const override { return true; }
	virtual bool IsPinRelevant(const UEdGraphPin* Pin) const override;

	virtual bool IsNodeOutDatedAndNeedsRefresh() override;
	virtual FString GetRefreshMessage() const override;

	// Creates the SGraph Node widget for the thumbnail
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

	// Check if previous UV channels pins of the same LOD and material are also linked to a layout node
	bool CheckIsValidLayout(const UEdGraphPin* Pin, int32& LayoutIndex, FString& MaterialName);

	// Determines if the Node is collapsed or not
	bool bCollapsed = true;

	// Pointer to the SGraphNode Skeletal Mesh
	TWeakPtr< SGraphNodeSkeletalMesh > GraphNodeSkeletalMesh;
private:
	UMaterialInterface* GetMaterialInterfaceFor(const int LODIndex, const int MaterialIndex, const FSkeletalMeshModel* ImportedModel = nullptr) const;
	FSkeletalMaterial* GetSkeletalMaterialFor(const int LODIndex, const int MaterialIndex, const FSkeletalMeshModel* ImportedModel = nullptr) const;
};
