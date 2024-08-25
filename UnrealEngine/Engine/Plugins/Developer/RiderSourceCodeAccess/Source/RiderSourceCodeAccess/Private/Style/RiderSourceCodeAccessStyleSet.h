// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FRiderSourceCodeAccessStyleSet : public FSlateStyleSet
{
public:

	static const FRiderSourceCodeAccessStyleSet& Get();

	static void Initialize();

	static void Shutdown();

	static FName RiderIconName;
	static FName RiderRefreshIconName;
	static FName StyleName;

private:
	FRiderSourceCodeAccessStyleSet();

	virtual const FName& GetStyleSetName() const override;

	static TUniquePtr<FRiderSourceCodeAccessStyleSet> Inst;
	
};
