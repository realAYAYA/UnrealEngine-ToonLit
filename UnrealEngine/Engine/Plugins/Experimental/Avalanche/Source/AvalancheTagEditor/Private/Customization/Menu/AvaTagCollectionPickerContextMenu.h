// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class IPropertyHandle;
class SWidget;
class UToolMenu;
struct FAssetData;

class FAvaTagCollectionPickerContextMenu : public TSharedFromThis<FAvaTagCollectionPickerContextMenu>
{
	struct FPrivateToken { explicit FPrivateToken() = default; };

public:
	explicit FAvaTagCollectionPickerContextMenu(FPrivateToken) {}

	static FAvaTagCollectionPickerContextMenu& Get();

	TSharedRef<SWidget> GenerateContextMenuWidget(const TSharedPtr<IPropertyHandle>& InTagCollectionHandle);

private:
	void PopulateContextMenu(UToolMenu* InToolMenu);

	bool TryGetAsset(const TWeakPtr<IPropertyHandle>& InTagCollectionHandleWeak, FAssetData& OutAssetData) const;

	void EditAsset(TWeakPtr<IPropertyHandle> InTagCollectionHandleWeak);

	void FindInContentBrowser(TWeakPtr<IPropertyHandle> InTagCollectionHandleWeak);
};
