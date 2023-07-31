// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

#include "UsdWrappers/UsdStage.h"
#include "Widgets/IUSDTreeViewItem.h"

class USDSTAGEEDITORVIEWMODELS_API FUsdLayerModel : public TSharedFromThis< FUsdLayerModel >
{
public:
	FString DisplayName;
	bool bIsEditTarget = false;
	bool bIsMuted = false;
	bool bIsDirty = false;
	bool bIsInIsolatedStage = true;
};

class USDSTAGEEDITORVIEWMODELS_API FUsdLayerViewModel : public IUsdTreeViewItem
{
public:
	explicit FUsdLayerViewModel(
		FUsdLayerViewModel* InParentItem,
		const UE::FUsdStageWeak& InUsdStage,
		const UE::FUsdStageWeak& InIsolatedStage,
		const FString& InLayerIdentifier
	);

	bool IsValid() const;

	TArray< TSharedRef< FUsdLayerViewModel > > GetChildren();

	void FillChildren();

	void RefreshData();

	UE::FSdfLayer GetLayer() const;
	FText GetDisplayName() const;

	bool IsLayerMuted() const;
	bool CanMuteLayer() const;
	void ToggleMuteLayer();

	bool IsEditTarget() const;
	bool CanEditLayer() const;
	bool EditLayer();

	bool IsInIsolatedStage() const;

	void AddSubLayer( const TCHAR* SubLayerIdentifier );
	void NewSubLayer( const TCHAR* SubLayerIdentifier );
	bool RemoveSubLayer( int32 SubLayerIndex );

	bool IsLayerDirty() const;

public:
	TSharedRef< FUsdLayerModel > LayerModel;
	FUsdLayerViewModel* ParentItem;
	TArray< TSharedRef< FUsdLayerViewModel > > Children;

	UE::FUsdStageWeak UsdStage;
	UE::FUsdStageWeak IsolatedStage;
	FString LayerIdentifier;
};
