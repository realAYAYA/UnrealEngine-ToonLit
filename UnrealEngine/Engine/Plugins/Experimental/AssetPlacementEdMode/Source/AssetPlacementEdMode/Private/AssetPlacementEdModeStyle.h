// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class FAssetPlacementEdModeStyle
	: public FSlateStyleSet
{
public:
	virtual const FName& GetStyleSetName() const override;

	static const FAssetPlacementEdModeStyle& Get();
	static void Shutdown();
	
	~FAssetPlacementEdModeStyle();

private:
	FAssetPlacementEdModeStyle();

	static FName StyleName;
	static TUniquePtr<FAssetPlacementEdModeStyle> Inst;
};
