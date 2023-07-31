// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class XInput : ModuleRules
{
	protected string DirectXSDKDir { get => Target.UEThirdPartySourceDirectory + "Windows/DirectX"; }

	public XInput(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Ensure correct include and link paths for xinput so the correct dll is loaded (xinput1_3.dll)

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(DirectXSDKDir + "/Lib/x64/XInput.lib");
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");
		}
	}
}

