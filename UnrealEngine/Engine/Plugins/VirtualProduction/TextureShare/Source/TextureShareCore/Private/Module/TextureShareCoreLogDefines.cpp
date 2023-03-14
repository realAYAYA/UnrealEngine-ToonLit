// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/TextureShareCoreLogDefines.h"

#if TEXTURESHARECORE_SDK && TEXTURESHARECORE_DEBUGLOG

#include "Debugapi.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace LogHelpers
{
	void WriteLine(const FString& In)
	{
		OutputDebugStringW(FTCHARToWChar(*FString::Printf(TEXT("%s\n\t"), *In)).Get());
	}

	void WriteLineOnce(const FString& In)
	{
		static FString _PreLogText = TEXT("");

		if (_PreLogText != In)
		{
			_PreLogText = In;

			OutputDebugStringW(FTCHARToWChar(*FString::Printf(TEXT("%s\n\t"), *In)).Get());
		}
	}
};

#endif
