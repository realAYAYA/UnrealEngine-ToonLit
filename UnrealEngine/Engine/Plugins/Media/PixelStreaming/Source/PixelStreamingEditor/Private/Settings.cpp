// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings.h"
#include "Utils.h"

namespace UE::EditorPixelStreaming::Settings
{
    TAutoConsoleVariable<bool> CVarEditorPixelStreamingStartOnLaunch(
		TEXT("PixelStreaming.Editor.StartOnLaunch"),
		false,
		TEXT("Start streaming the Editor as soon as it launches. Default: false"),
		ECVF_Default);

    void InitialiseSettings()
    {
		using namespace UE::PixelStreaming;
        // Options parse (if these exist they are set to true)
		CommandLineParseOption(TEXT("EditorPixelStreamingStartOnLaunch"), CVarEditorPixelStreamingStartOnLaunch);
    }
}