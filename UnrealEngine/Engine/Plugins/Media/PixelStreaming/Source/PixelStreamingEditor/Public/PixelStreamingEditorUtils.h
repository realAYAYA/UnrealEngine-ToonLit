// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::EditorPixelStreaming
{
	enum class PIXELSTREAMINGEDITOR_API EStreamTypes : uint8
	{
		LevelEditorViewport = 0,
		Editor = 1,
		VCam = 2
	};

	inline FString ToString(EStreamTypes StreamType)
	{
		switch(StreamType)
		{
			case EStreamTypes::LevelEditorViewport:
				return TEXT("the Level Editor");
			case EStreamTypes::Editor:
				return TEXT("the Full Editor");
			case EStreamTypes::VCam:
				return TEXT("a Virtual Camera");
			default:
				return TEXT("Unknown stream type!");
		}
	}
}