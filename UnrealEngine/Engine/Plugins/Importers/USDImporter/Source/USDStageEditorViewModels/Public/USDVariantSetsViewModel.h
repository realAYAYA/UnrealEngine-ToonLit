// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

class FUsdVariantSetsViewModel;

class USDSTAGEEDITORVIEWMODELS_API FUsdVariantSetViewModel : public TSharedFromThis< FUsdVariantSetViewModel >
{
public:
	explicit FUsdVariantSetViewModel( FUsdVariantSetsViewModel* InOwner );

	void SetVariantSelection( const TSharedPtr< FString >& InVariantSelection );

public:
	FString SetName;
	TSharedPtr< FString > VariantSelection;
	TArray< TSharedPtr< FString > > Variants;

private:
	FUsdVariantSetsViewModel* Owner;
};

class USDSTAGEEDITORVIEWMODELS_API FUsdVariantSetsViewModel
{
public:
	void UpdateVariantSets( const UE::FUsdStageWeak& InUsdStage, const TCHAR* PrimPath );

public:
	UE::FUsdStageWeak UsdStage;
	FString PrimPath;

	TArray< TSharedPtr< FUsdVariantSetViewModel > > VariantSets;
};
