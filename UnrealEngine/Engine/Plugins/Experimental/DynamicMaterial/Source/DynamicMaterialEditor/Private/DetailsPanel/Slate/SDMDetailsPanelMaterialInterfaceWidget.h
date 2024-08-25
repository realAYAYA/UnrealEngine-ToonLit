// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Templates/SharedPointer.h"

class FAssetThumbnailPool;
class IPropertyHandle;
class UDynamicMaterialInstance;

class SDMDetailsPanelMaterialInterfaceWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMDetailsPanelMaterialInterfaceWidget) {}
	SLATE_ARGUMENT(TSharedPtr<FAssetThumbnailPool>, ThumbnailPool)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InPropertyHandle);

protected:
	TSharedPtr<IPropertyHandle> PropertyHandle;

	UObject* GetAsset() const;
	void SetAsset(UObject* NewAsset);

	UDynamicMaterialInstance* GetDynamicMaterialInstance() const;
	void SetDynamicMaterialInstance(UDynamicMaterialInstance* NewInstance);

	FReply OnButtonClicked();
	FReply CreateDynamicMaterialInstance();
	FReply ClearDynamicMaterialInstance();
	FReply OpenDynamicMaterialInstanceTab();

	FText GetButtonText() const;
};
