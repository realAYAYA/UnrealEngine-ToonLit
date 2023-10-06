// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "MuCOE/CustomizableObjectEditor_Deprecated.h"
#include "SGraphNode.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByNameDefaultPin.h"

#include "CustomizableObjectNodeSkeletalMesh.generated.h"

namespace ENodeTitleType { enum Type : int; }

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


/** Remap pins by pin PinData. */
UCLASS()
class UCustomizableObjectNodeSkeletalMeshRemapPinsBySection : public UCustomizableObjectNodeRemapPinsByNameDefaultPin
{
	GENERATED_BODY()
public:
	virtual bool Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const override;
};


/** PinData of a pin that belongs to a Skeletal Mesh Section. */
UCLASS()
class UCustomizableObjectNodeSkeletalMeshPinDataSection : public UCustomizableObjectNodePinData
{
	GENERATED_BODY()

public:
	void Init(int32 InLODIndex, int32 InSectionIndex);

	int32 GetLODIndex() const;

	int32 GetSectionIndex() const;

protected:
	virtual bool Equals(const UCustomizableObjectNodePinData& Other) const override;

private:
	UPROPERTY()
	int32 LODIndex = -1;

	UPROPERTY()
	int32 SectionIndex = -1;
};


/** PinData of a Mesh pin. */
UCLASS()
class UCustomizableObjectNodeSkeletalMeshPinDataMesh : public UCustomizableObjectNodeSkeletalMeshPinDataSection
{
	GENERATED_BODY()
};


/** PinData of a Image pin. */
UCLASS()
class UCustomizableObjectNodeSkeletalMeshPinDataImage : public UCustomizableObjectNodeSkeletalMeshPinDataSection
{
	GENERATED_BODY()

public:
	void Init(int32 InLODIndex, int32 InSectionIndex, FGuid InTextureParameterId);

	FGuid GetTextureParameterId() const;

protected:
	virtual bool Equals(const UCustomizableObjectNodePinData& Other) const override;

private:
	UPROPERTY()
	FGuid TextureParameterId;
};


/** PinData of a Layout pin. */
UCLASS()
class UCustomizableObjectNodeSkeletalMeshPinDataLayout : public UCustomizableObjectNodeSkeletalMeshPinDataSection
{
	GENERATED_BODY()

public:
	void Init(int32 InLODIndex, int32 InSectionIndex, int32 InUVIndex);

	int32 GetUVIndex() const;

protected:
	virtual bool Equals(const UCustomizableObjectNodePinData& Other) const override;

private:
	UPROPERTY()
	int32 UVIndex = -1;
};



UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeSkeletalMesh : public UCustomizableObjectNodeMesh
{
public:
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TObjectPtr<USkeletalMesh> SkeletalMesh;

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

	UPROPERTY()
	int32 AnimBlueprintSlot_DEPRECATED;
	
	/** The anim slot associated with the AnimInstance */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FName AnimBlueprintSlotName;

	/** Animation tags that will be gathered by a Generated instance if it contains this skeletal mesh part,
		it will not be grouped by component or AnimBlueprintSlot */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FGameplayTagContainer AnimationGameplayTags;

	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual UCustomizableObjectNodeRemapPins* CreateRemapPinsDefault() const override;

	// UCustomizableObjectNodeMesh interface
	virtual UTexture2D* FindTextureForPin(const UEdGraphPin* Pin) const override;
	virtual void GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVIndex) const override;
	virtual TArray<UCustomizableObjectLayout*> GetLayouts(const UEdGraphPin& MeshPin) const override;
	virtual UObject* GetMesh() const override;
	virtual UEdGraphPin* GetMeshPin(int32 LOD, int32 SectionIndex) const override;
	virtual UEdGraphPin* GetLayoutPin(int32 LODIndex, int32 SectionIndex, int32 LayoutIndex) const override;
	virtual void GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const override;

	// Own interface
	
	/** Returns the material associated to the given output pin. */
	UMaterialInterface* GetMaterialFor(const UEdGraphPin* Pin) const;
	FSkeletalMaterial* GetSkeletalMaterialFor(const UEdGraphPin& Pin) const;

	virtual bool ProvidesCustomPinRelevancyTest() const override { return true; }
	virtual bool IsPinRelevant(const UEdGraphPin* Pin) const override;

	virtual bool IsNodeOutDatedAndNeedsRefresh() override;
	virtual FString GetRefreshMessage() const override;

	// Creates the SGraph Node widget for the thumbnail
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

	/** Check if lower UV channels pins of the same LOD and Section are also linked to a Layout Node. */
	bool CheckIsValidLayout(const UEdGraphPin* Pin, int32& LayoutIndex, FString& MaterialName);

	// Determines if the Node is collapsed or not
	bool bCollapsed = true;

	// Pointer to the SGraphNode Skeletal Mesh
	TWeakPtr< SGraphNodeSkeletalMesh > GraphNodeSkeletalMesh;

private:
	UMaterialInterface* GetMaterialInterfaceFor(const int32 LODIndex, const int32 MaterialIndex, const FSkeletalMeshModel* ImportedModel = nullptr) const;
	FSkeletalMaterial* GetSkeletalMaterialFor(const int32 LODIndex, const int32 MaterialIndex, const FSkeletalMeshModel* ImportedModel = nullptr) const;

	// Deprecated
	
	UPROPERTY()
	TArray<FCustomizableObjectNodeSkeletalMeshLOD> LODs_DEPRECATED;
};
