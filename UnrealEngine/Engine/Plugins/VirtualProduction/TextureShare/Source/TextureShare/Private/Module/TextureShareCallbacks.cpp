// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/TextureShareCallbacks.h"

#include "ITextureShare.h"
#include "ITextureShareAPI.h"
#include "ITextureShareCallbacks.h"

ITextureShareCallbacks& ITextureShareCallbacks::Get()
{
	static ITextureShareCallbacks& TSCallbacksAPI = ITextureShare::Get().GetTextureShareAPI().GetCallbacks();

	return TSCallbacksAPI;
}
