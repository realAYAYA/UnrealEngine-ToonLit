// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

#include "UsdWrappers/ForwardDeclarations.h"

class USDSTAGEEDITORVIEWMODELS_API FUsdReference : public TSharedFromThis< FUsdReference >
{
public:
	FString AssetPath;
};

class USDSTAGEEDITORVIEWMODELS_API FUsdReferencesViewModel
{
public:
	void UpdateReferences( const UE::FUsdStageWeak& UsdStage, const TCHAR* PrimPath );

public:
	TArray< TSharedPtr< FUsdReference > > References;
};