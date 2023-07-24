// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"


namespace WaveTable
{
	namespace Editor
	{
		namespace FileUtilities
		{
			void WAVETABLEEDITOR_API LoadPCMChannel(const FString& InFilePath, int32 InChannelIndex, TArray<float>& OutPCMData);
		} // namespace FileUtilities
	} // namespace Editor
} // namespace WaveTable

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Containers/UnrealString.h"
#endif
