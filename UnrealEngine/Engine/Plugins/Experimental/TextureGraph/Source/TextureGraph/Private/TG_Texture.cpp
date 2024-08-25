// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_Texture.h"
#include "TG_Var.h"

template <> FString TG_Var_LogValue(FTG_Texture& Value)
{
	if (!Value.RasterBlob)
		return TEXT("FTG_Texture nullptr");

	return FString::Printf(TEXT("FTG_Texture <0x%0*x> %dx%d"), 8, Value.RasterBlob.get(), Value.RasterBlob->GetWidth(), Value.RasterBlob->GetHeight());

}