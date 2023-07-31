// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Input : ModuleRules
{
	public DX11Input(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";

		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		string LibDir = null;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibDir = DirectXSDKDir + "/Lib/x64/";
		}

		PublicAdditionalLibraries.AddRange(
			new string[] {
				LibDir + "dxguid.lib",
				LibDir + "dinput8.lib"
			}
			);
	}
}

