// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "AssetTypeActions_Base.h"

class FGLTFProxyAssetActions final : public IAssetTypeActions
{
public:

	FGLTFProxyAssetActions(const TSharedRef<IAssetTypeActions>& OriginalActions);
	
	virtual void GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section) override;

	virtual FText GetName() const override { return OriginalActions->GetName(); }
	virtual UClass* GetSupportedClass() const override { return OriginalActions->GetSupportedClass(); }
	virtual FColor GetTypeColor() const override { return OriginalActions->GetTypeColor(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override { OriginalActions->GetActions(InObjects, MenuBuilder); }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override { OriginalActions->OpenAssetEditor(InObjects, EditWithinLevelEditor); }
	virtual bool AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType) override { return OriginalActions->AssetsActivatedOverride(InObjects, ActivationType); }
	virtual bool CanRename(const FAssetData& InAsset, FText* OutErrorMsg) const override { return OriginalActions->CanRename(InAsset, OutErrorMsg); }
	virtual bool CanDuplicate(const FAssetData& InAsset, FText* OutErrorMsg) const override { return OriginalActions->CanDuplicate(InAsset, OutErrorMsg); }
	virtual TArray<FAssetData> GetValidAssetsForPreviewOrEdit(TArrayView<const FAssetData> InAssetDatas, bool bIsPreview) override { return OriginalActions->GetValidAssetsForPreviewOrEdit(InAssetDatas, bIsPreview); }
	virtual bool CanFilter() override { return false; }
	virtual FName GetFilterName() const override { return OriginalActions->GetFilterName(); }
	virtual bool CanLocalize() const override { return OriginalActions->CanLocalize(); }
	virtual bool CanMerge() const override { return OriginalActions->CanMerge(); }
	virtual void Merge( UObject* InObject) override { OriginalActions->Merge(InObject); }
	virtual void Merge(UObject* BaseAsset, UObject* RemoteAsset, UObject* LocalAsset, const FOnMergeResolved& ResolutionCallback) override { OriginalActions->Merge(BaseAsset, RemoteAsset, LocalAsset, ResolutionCallback); }
	virtual uint32 GetCategories() override { return OriginalActions->GetCategories(); }
	virtual FString GetObjectDisplayName(UObject* Object) const override { return OriginalActions->GetObjectDisplayName(Object); }
	virtual const TArray<FText>& GetSubMenus() const override { return OriginalActions->GetSubMenus(); }
	virtual bool ShouldForceWorldCentric() override { return OriginalActions->ShouldForceWorldCentric(); }
	virtual void PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const override { OriginalActions->PerformAssetDiff(OldAsset, NewAsset, OldRevision, NewRevision); }
	virtual UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override { return OriginalActions->GetThumbnailInfo(Asset); }
	virtual EThumbnailPrimType GetDefaultThumbnailPrimitiveType(UObject* Asset) const override { return OriginalActions->GetDefaultThumbnailPrimitiveType(Asset); }
	virtual TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override { return OriginalActions->GetThumbnailOverlay(AssetData); }
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override { return OriginalActions->GetAssetDescription(AssetData); }
	virtual bool IsImportedAsset() const override { return OriginalActions->IsImportedAsset(); }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override { OriginalActions->GetResolvedSourceFilePaths(TypeAssets, OutSourceFilePaths); }
	virtual void GetSourceFileLabels(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFileLabels) const override { OriginalActions->GetSourceFileLabels(TypeAssets, OutSourceFileLabels); }
	virtual void BuildBackendFilter(FARFilter& InFilter) override { OriginalActions->BuildBackendFilter(InFilter); }
	virtual FText GetDisplayNameFromAssetData(const FAssetData& AssetData) const override { return OriginalActions->GetDisplayNameFromAssetData(AssetData); }
	virtual void SetSupported(bool bInSupported) override { OriginalActions->SetSupported(bInSupported); }
	virtual bool IsSupported() const override { return OriginalActions->IsSupported(); }
	virtual FTopLevelAssetPath GetClassPathName() const override { return OriginalActions->GetClassPathName(); };
	virtual const FSlateBrush* GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const override { return OriginalActions->GetIconBrush(InAssetData, InClassName); }
	virtual const FSlateBrush* GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const override { return OriginalActions->GetThumbnailBrush(InAssetData, InClassName); }
private:

	void GetProxyActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section);

	void OnCreateProxy(TArray<FWeakObjectPtr> Objects) const;

	TSharedRef<IAssetTypeActions> OriginalActions;
};

#endif
