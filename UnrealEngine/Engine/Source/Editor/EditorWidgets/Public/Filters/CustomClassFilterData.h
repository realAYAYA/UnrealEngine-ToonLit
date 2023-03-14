// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AssetRegistry/ARFilter.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "UObject/TopLevelAssetPath.h"

class FFilterCategory;
class IAssetTypeActions;
class UClass;
struct FARFilter;

/** Helper Class that can be used to provide custom class filters to the Filter Widget
 *  Using this for Type Filters ensures that the type filters get OR'd together
 *  For example: filtering for StaticMeshActor and DirectionalLight will show assets that belong
 *  to either of those types.
 */
class EDITORWIDGETS_API FCustomClassFilterData
{
public:

	/* You can provide an IAssetTypeActions for your type to get all the information from that */
	FCustomClassFilterData(TWeakPtr<IAssetTypeActions> InAssetTypeAction)
	: Class(nullptr)
	, AssetTypeActions(InAssetTypeAction)
	{
		
	}

	/* Or you can provide a UClass, a Category and a Color to identify the filter */
	FCustomClassFilterData(UClass *InClass, TSharedPtr<FFilterCategory> InCategory, FLinearColor InColor)
	: Class(InClass)
	, Color(InColor)
	{
		Categories.Add(InCategory);
	}

	/* Unlike normal filters, Type Filters are allowed to belong to multiple categories */
	void AddCategory(TSharedPtr<FFilterCategory> InCategory);

	/* Get the AssetTypeActions associated with this filter if they exist */
	TSharedPtr<IAssetTypeActions> GetAssetTypeActions() const;

	/* Get the UClass associated with this filter */
	UClass *GetClass() const;

	/* Get the Catgories this filter belongs to */
	TArray<TSharedPtr<FFilterCategory>> GetCategories() const;

	/* Get the color of this filter */
	FLinearColor GetColor() const;

	/** Add this filter to the input BackendFilter */
	void BuildBackendFilter(FARFilter &Filter);

	/** Get the logical name of this filter class */
	FText GetName() const;

	/** Get the display name of this filter class */
	FString GetFilterName() const;

	/**
	 * Returns class path name as a package + class FName pair
	 */
	FTopLevelAssetPath GetClassPathName() const;

private:

	/** The Class the filter is associated with (if it does not have an AssetTypeAction) */
	UClass *Class;

	/** The Categories the filter will show up under */
	TArray<TSharedPtr<FFilterCategory>> Categories;

	/** The Color of the filter (if it does not have an AssetTypeAction to get the color from) */
	FLinearColor Color;

	/** An optional AssetTypeAction to get information about the filter class */
	TWeakPtr<IAssetTypeActions> AssetTypeActions;
};