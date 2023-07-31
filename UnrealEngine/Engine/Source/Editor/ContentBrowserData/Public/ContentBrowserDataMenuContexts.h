// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserItem.h"
#include "CollectionManagerTypes.h"
#include "ContentBrowserDataMenuContexts.generated.h"

class SWidget;

UENUM()
enum class EContentBrowserDataMenuContext_AddNewMenuDomain : uint8
{
	Toolbar,
	AssetView,
	PathView,
};

UCLASS()
class CONTENTBROWSERDATA_API UContentBrowserDataMenuContext_AddNewMenu : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_OneParam(FOnBeginItemCreation, const FContentBrowserItemDataTemporaryContext&);

	UPROPERTY()
	TArray<FName> SelectedPaths;

	// At least one of the selected paths maps to a mounted content root
	UPROPERTY()
	bool bContainsValidPackagePath = false;

	UPROPERTY()
	bool bCanBeModified = true;

	UPROPERTY()
	EContentBrowserDataMenuContext_AddNewMenuDomain OwnerDomain = EContentBrowserDataMenuContext_AddNewMenuDomain::Toolbar;

	FOnBeginItemCreation OnBeginItemCreation;
};

UCLASS()
class CONTENTBROWSERDATA_API UContentBrowserDataMenuContext_FolderMenu : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category=ContentBrowser)
	TArray<FContentBrowserItem> SelectedItems;

	UPROPERTY()
	bool bCanBeModified = true;

	TWeakPtr<SWidget> ParentWidget;
};

UCLASS()
class CONTENTBROWSERDATA_API UContentBrowserDataMenuContext_FileMenu : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_OneParam(FOnShowInPathsView, TArrayView<const FContentBrowserItem>);

	DECLARE_DELEGATE(FOnRefreshView);

	UPROPERTY()
	TArray<FContentBrowserItem> SelectedItems;

	TArray<FCollectionNameType> SelectedCollections;

	UPROPERTY()
	bool bCanBeModified = true;

	TWeakPtr<SWidget> ParentWidget;

	FOnShowInPathsView OnShowInPathsView;

	FOnRefreshView OnRefreshView;
};

UCLASS()
class CONTENTBROWSERDATA_API UContentBrowserDataMenuContext_DragDropMenu : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FContentBrowserItem DropTargetItem;

	UPROPERTY()
	TArray<FContentBrowserItem> DraggedItems;

	UPROPERTY()
	bool bCanMove = true;

	UPROPERTY()
	bool bCanCopy = true;

	TWeakPtr<SWidget> ParentWidget;
};
