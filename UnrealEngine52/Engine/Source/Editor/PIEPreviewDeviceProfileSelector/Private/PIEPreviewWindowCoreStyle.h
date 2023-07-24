// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Styling/SlateStyle.h"

class PIEPREVIEWDEVICEPROFILESELECTOR_API FPIEPreviewWindowCoreStyle
{

public:

	static TSharedRef<class ISlateStyle> Create(const FName& InStyleSetName = "FPIECoreStyle");
	
	static void InitializePIECoreStyle();
	
	/** @return the singleton instance. */
	static const ISlateStyle& Get()
	{
		return *(Instance.Get());
	}

private:
		
	static TSharedPtr< class ISlateStyle > Instance;
};
#endif