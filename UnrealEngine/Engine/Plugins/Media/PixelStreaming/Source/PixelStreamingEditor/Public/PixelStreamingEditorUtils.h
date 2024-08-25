// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericWindowDefinition.h"
#include "RHIResources.h"

class SWindow;

namespace UE::EditorPixelStreaming
{
	enum class PIXELSTREAMINGEDITOR_API EStreamTypes : uint8
	{
		LevelEditorViewport = 0,
		Editor = 1
	};

	FString ToString(EStreamTypes StreamType);
	const TCHAR* ToString(EWindowType Type);
	const FString HashWindow(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer);
}