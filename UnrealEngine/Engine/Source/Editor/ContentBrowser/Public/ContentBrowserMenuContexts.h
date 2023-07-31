// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "Containers/Array.h"
#include "ContentBrowserDelegates.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "ContentBrowserMenuContexts.generated.h"

class FAssetContextMenu;
class IAssetTypeActions;
class SAssetView;
class SContentBrowser;
class SFilterList;
class UClass;
struct FFrame;
struct FToolMenuSection;
struct FToolMenuContext;

UCLASS()
class CONTENTBROWSER_API UContentBrowserAssetContextMenuContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<FAssetContextMenu> AssetContextMenu;

	TWeakPtr<IAssetTypeActions> CommonAssetTypeActions;

	UE_DEPRECATED(5.1, "Use SelectedAssets now, this field will not contain any objects.")
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;

	UPROPERTY()
	TArray<FAssetData> SelectedAssets;

	UPROPERTY()
	TObjectPtr<UClass> CommonClass;

	UPROPERTY()
	bool bCanBeModified;

	UFUNCTION(BlueprintCallable, Category="Tool Menus", meta=(DeprecatedFunction, DeprecationMessage = "GetSelectedObjects has been deprecated.  We no longer implictly load assets upon request.  If you can work without loading the assets, please use SelectedAssets.  Otherwise call LoadSelectedObjects"))
	TArray<UObject*> GetSelectedObjects() const
	{
		return LoadSelectedObjects();
	}

	UFUNCTION(BlueprintCallable, Category="Tool Menus")
	TArray<UObject*> LoadSelectedObjects() const
	{
		return LoadSelectedObjects<UObject>();
	}

	template<typename ExpectedAssetType>
	TArray<ExpectedAssetType*> LoadSelectedObjects() const
	{
		return LoadSelectedObjectsIf<ExpectedAssetType>([](const FAssetData& AssetData){ return true; });
	}

	template<typename ExpectedAssetType>
	TArray<ExpectedAssetType*> LoadSelectedObjectsIf(TFunctionRef<bool(const FAssetData& AssetData)> PredicateFilter) const
	{
		TArray<ExpectedAssetType*> Result;
		Result.Reserve(SelectedAssets.Num());
		for (const FAssetData& Asset : SelectedAssets)
		{
			if (PredicateFilter(Asset))
			{
				if (UObject* AssetObject = Asset.GetAsset())
				{
					if (ExpectedAssetType* AssetObjectTyped = Cast<ExpectedAssetType>(AssetObject))
					{
						Result.Add(AssetObjectTyped);
					}
				}
			}
		}
		return Result;
	}

	template<typename MenuOrSectionType>
	static const UContentBrowserAssetContextMenuContext* FindContextWithAssets(const MenuOrSectionType& MenuOrSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = MenuOrSection.template FindContext<UContentBrowserAssetContextMenuContext>();
		if (!Context || Context->SelectedAssets.IsEmpty())
		{
			return nullptr;
		}

		return Context;
	}
};

UCLASS()
class CONTENTBROWSER_API UContentBrowserAssetViewContextMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SContentBrowser> OwningContentBrowser;
	TWeakPtr<SAssetView> AssetView;
};

UCLASS()
class CONTENTBROWSER_API UContentBrowserMenuContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<SContentBrowser> ContentBrowser;
};

UCLASS()
class CONTENTBROWSER_API UContentBrowserFolderContext : public UContentBrowserMenuContext
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly, Category = "ContentBrowser")
	bool bCanBeModified;

	UPROPERTY(BlueprintReadOnly, Category = "ContentBrowser")
	bool bNoFolderOnDisk;

	UPROPERTY(BlueprintReadOnly, Category = "ContentBrowser")
	int32 NumAssetPaths;

	UPROPERTY(BlueprintReadOnly, Category = "ContentBrowser")
	int32 NumClassPaths;

	UPROPERTY(BlueprintReadOnly, Category = "ContentBrowser")
	TArray<FString> SelectedPackagePaths;

	FOnCreateNewFolder OnCreateNewFolder;

	const TArray<FString>& GetSelectedPackagePaths() const { return SelectedPackagePaths; }
};

UCLASS()
class CONTENTBROWSER_API UContentBrowserFilterListContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<SFilterList> FilterList;

	EAssetTypeCategories::Type MenuExpansion;
};

UCLASS()
class CONTENTBROWSER_API UContentBrowserAddNewContextMenuContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<SContentBrowser> ContentBrowser;
};

UCLASS()
class CONTENTBROWSER_API UContentBrowserToolbarMenuContext : public UObject
{
	GENERATED_BODY()

public:
	FName GetCurrentPath() const;

	bool CanWriteToCurrentPath() const;

	TWeakPtr<SContentBrowser> ContentBrowser;
};
