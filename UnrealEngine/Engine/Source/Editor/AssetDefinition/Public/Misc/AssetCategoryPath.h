// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The asset category path is how we know how to build menus around assets.  For example, Basic is generally the ones
 * we expose at the top level, where as everything else is a category with a pull out menu, and the subcategory would
 * be where it gets placed in a submenu inside of there.
 */
struct ASSETDEFINITION_API FAssetCategoryPath
{
	FAssetCategoryPath(const FText& InCategory);
	FAssetCategoryPath(const FText& InCategory, const FText& SubCategory);
	FAssetCategoryPath(const FAssetCategoryPath& InCategory, const FText& InSubCategory);
	FAssetCategoryPath(TConstArrayView<FText> InCategoryPath);

	FName GetCategory() const { return CategoryPath[0].Key; }
	FText GetCategoryText() const { return CategoryPath[0].Value; }
	
	bool HasSubCategory() const { return CategoryPath.Num() > 1; }
	FName GetSubCategory() const { return HasSubCategory() ? CategoryPath[1].Key : NAME_None; }
	FText GetSubCategoryText() const { return HasSubCategory() ? CategoryPath[1].Value : FText::GetEmpty(); }
	
	FAssetCategoryPath operator / (const FText& SubCategory) const { return FAssetCategoryPath(*this, SubCategory); }
	
private:
	TArray<TPair<FName, FText>> CategoryPath;
};