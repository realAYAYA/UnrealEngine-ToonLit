// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNode.h"
#include "AssetRegistry/AssetData.h"
#include "EdGraph_ReferenceViewer.h"
#include "EdGraphNode_Reference.generated.h"

class UEdGraphPin;

UCLASS()
class ASSETMANAGEREDITOR_API UEdGraphNode_Reference : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	/** Returns first asset identifier */
	FAssetIdentifier GetIdentifier() const;
	
	/** Returns all identifiers on this node including virtual things */
	void GetAllIdentifiers(TArray<FAssetIdentifier>& OutIdentifiers) const;

	/** Returns only the packages in this node, skips searchable names */
	void GetAllPackageNames(TArray<FName>& OutPackageNames) const;

	/** Returns our owning graph */
	UEdGraph_ReferenceViewer* GetReferenceViewerGraph() const;

	// UEdGraphNode implementation
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void AllocateDefaultPins() override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool ShowPaletteIconOnNode() const override { return true; }
	// End UEdGraphNode implementation

	void SetAllowThumbnail(bool bInAllow) { bAllowThumbnail = bInAllow; }
	bool AllowsThumbnail() const;
	bool UsesThumbnail() const;
	bool IsPackage() const;
	bool IsCollapsed() const;
	bool IsADuplicate() const { return bIsADuplicate; }

	// Nodes that are filtered out still may still show because they
	// are between nodes that pass the filter and the root.  This "filtered"
	// bool allows us to render these in-between nodes differently
	void SetIsFiltered(bool bInFiltered);
	bool GetIsFiltered() const;

	bool IsOverflow() const { return bIsOverflow; }

	FAssetData GetAssetData() const;

	UEdGraphPin* GetDependencyPin();
	UEdGraphPin* GetReferencerPin();

private:
	void CacheAssetData(const FAssetData& AssetData);
	void SetupReferenceNode(const FIntPoint& NodeLoc, const TArray<FAssetIdentifier>& NewIdentifiers, const FAssetData& InAssetData, bool bInAllowThumbnail, bool bInIsADuplicate);
	void SetReferenceNodeCollapsed(const FIntPoint& NodeLoc, int32 InNumReferencesExceedingMax);
	void AddReferencer(class UEdGraphNode_Reference* ReferencerNode);

	TArray<FAssetIdentifier> Identifiers;
	FText NodeTitle;

	bool bAllowThumbnail;
	bool bUsesThumbnail;
	bool bIsPackage;
	bool bIsPrimaryAsset;
	bool bIsCollapsed;
	bool bIsADuplicate;
	bool bIsFiltered;
	bool bIsOverflow;

	FAssetData CachedAssetData;
	FLinearColor AssetTypeColor;
	FSlateIcon AssetBrush;

	UEdGraphPin* DependencyPin;
	UEdGraphPin* ReferencerPin;

	friend UEdGraph_ReferenceViewer;
};


