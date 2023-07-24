// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/TextureShareCoreLogDefines.h"

#if TEXTURESHARECORE_SDK && TEXTURESHARECORE_DEBUGLOG

#include <windows.h>
#include "Debugapi.h"

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreLogHelpers::WriteLine(const FString& In)
	{
		OutputDebugStringW(FTCHARToWChar(*FString::Printf(TEXT("%s\n\t"), *In)).Get());
	}

#endif
