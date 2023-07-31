// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Audio : ModuleRules
{
	public DX11Audio(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibDir = null;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";

			LibDir = DirectXSDKDir + "/Lib/x64/";

			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

			PublicAdditionalLibraries.AddRange(
			new string[] 
			{
				LibDir + "dxguid.lib",
				LibDir + "xapobase.lib",
				LibDir + "XAPOFX.lib"
			}
			);
		}
	}
}

