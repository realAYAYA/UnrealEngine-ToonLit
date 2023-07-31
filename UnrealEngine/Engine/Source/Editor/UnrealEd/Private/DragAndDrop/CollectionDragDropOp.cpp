// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragAndDrop/CollectionDragDropOp.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CollectionManagerModule.h"
#include "HAL/Platform.h"
#include "ICollectionManager.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"
#include "SAssetTagItem.h"
#include "Styling/AppStyle.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"

class SWidget;

TArray<FAssetData> FCollectionDragDropOp::GetAssets() const
{
	ICollectionManager& CollectionManager = FModuleManager::LoadModuleChecked<FCollectionManagerModule>("CollectionManager").Get();
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FSoftObjectPath> AssetPaths;
	for (const FCollectionNameType& CollectionNameType : Collections)
	{
		TArray<FName> CollectionAssetPaths;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		CollectionManager.GetAssetsInCollection(CollectionNameType.Name, CollectionNameType.Type, CollectionAssetPaths);
		AssetPaths.Append(UE::SoftObjectPath::Private::ConvertObjectPathNames(CollectionAssetPaths));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	TArray<FAssetData> AssetDatas;
	AssetDatas.Reserve(AssetPaths.Num());
	for (const FSoftObjectPath& AssetPath : AssetPaths)
	{
		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(AssetPath);
		if (AssetData.IsValid())
		{
			AssetDatas.AddUnique(AssetData);
		}
	}
	
	return AssetDatas;
}

TSharedPtr<SWidget> FCollectionDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Padding(0)
		.BorderImage(FAppStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
		//.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SAssetTagItem)
			.ViewMode(AssetTagViewMode)
			.DisplayName(this, &FCollectionDragDropOp::GetDecoratorText)
		];
}

FText FCollectionDragDropOp::GetDecoratorText() const
{
	if (CurrentHoverText.IsEmpty() && Collections.Num() > 0)
	{
		return (Collections.Num() == 1)
			? FText::FromName(Collections[0].Name)
			: FText::Format(NSLOCTEXT("ContentBrowser", "CollectionDragDropDescription", "{0} and {1} {1}|plural(one=other,other=others)"), FText::FromName(Collections[0].Name), Collections.Num() - 1);
	}

	return CurrentHoverText;
}
