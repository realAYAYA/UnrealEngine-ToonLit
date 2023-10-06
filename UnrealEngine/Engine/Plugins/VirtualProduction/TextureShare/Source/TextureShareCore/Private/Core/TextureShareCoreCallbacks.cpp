// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/TextureShareCoreCallbacks.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreCallbacks.h"

ITextureShareCoreCallbacks& ITextureShareCoreCallbacks::Get()
{
	static ITextureShareCoreCallbacks& TSCallbacksAPI = ITextureShareCore::Get().GetTextureShareCoreAPI().GetCallbacks();

	return TSCallbacksAPI;
}
