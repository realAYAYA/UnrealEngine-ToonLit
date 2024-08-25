// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextWorkspace.generated.h"

struct FEditedDocumentInfo;
class UAnimNextWorkspace;

namespace UE::AnimNext::Editor
{
	class FWorkspaceEditor;
	class SWorkspaceView;
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	// A delegate for subscribing / reacting to workspace modifications.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorkspaceModified, UAnimNextWorkspace* /* InWorkspace */);
}

// Workspace entry used to export to asset registry
USTRUCT()
struct FAnimNextWorkspaceAssetRegistryExportEntry
{
	GENERATED_BODY()

	FAnimNextWorkspaceAssetRegistryExportEntry() = default;
	
	FAnimNextWorkspaceAssetRegistryExportEntry(const FSoftObjectPath& InAsset)
		: Asset(InAsset)
	{}

	UPROPERTY()
	FSoftObjectPath Asset;
};

// Workspace used to export to asset registry
USTRUCT()
struct FAnimNextWorkspaceAssetRegistryExports
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAnimNextWorkspaceAssetRegistryExportEntry> Assets;
};

UCLASS()
class UAnimNextWorkspace : public UObject
{
	GENERATED_BODY()

	friend class UE::AnimNext::Editor::FWorkspaceEditor;
	friend class UE::AnimNext::Editor::SWorkspaceView;
	friend struct UE::AnimNext::Editor::FUtils;

	ANIMNEXTEDITOR_API static const FName ExportsAssetRegistryTag;

	// Adds an asset to the workspace
	// @return true if the asset was added
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Workspace")
	bool AddAsset(UObject* InAsset, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
	bool AddAsset(const FAssetData& InAsset, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// Adds assets to the workspace
	// @return true if an asset was added
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Workspace")
	bool AddAssets(const TArray<UObject*>& InAssets, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
	bool AddAssets(TConstArrayView<FAssetData> InAssets, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// Removes an asset from the workspace
	// @return true if the asset was removed
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Workspace")
	bool RemoveAsset(UObject* InAsset, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
	bool RemoveAsset(const FAssetData& InAsset, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// Removes assets from the workspace
	// @return true if the asset was removed
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Workspace")
	bool RemoveAssets(TArray<UObject*> InAssets, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
	bool RemoveAssets(TConstArrayView<FAssetData> InAssets, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	static const TArray<FTopLevelAssetPath>& GetSupportedAssetClassPaths();

	static bool IsAssetSupported(const FAssetData& InAsset);

	void ReportError(const TCHAR* InMessage) const;

	void BroadcastModified();

	// UObject interface
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual bool IsEditorOnly() const override { return true; }

	// All of the assets referenced by this workspace
	UPROPERTY()
	TArray<TSoftObjectPtr<UObject>> Assets;

	// Documents this workspace was editing
	UPROPERTY()
	TArray<FEditedDocumentInfo> LastEditedDocuments;

	// Delegate to subscribe to modifications
	UE::AnimNext::Editor::FOnWorkspaceModified ModifiedDelegate;

	bool bSuspendNotifications = false;
};