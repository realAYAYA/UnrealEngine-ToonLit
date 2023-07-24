// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX9 : ModuleRules
{
	protected string DirectXSDKDir { get => DirectX.GetDir(Target); }

	protected virtual string LibDir { get => (Target.Platform == UnrealTargetPlatform.Win64) ? DirectX.GetLibDir(Target) : null; }

	public DX9(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

			PublicAdditionalLibraries.AddRange(
				new string[] {
					LibDir + "d3d9.lib",
					LibDir + "dxguid.lib",
					LibDir + "dinput8.lib",
					LibDir + "xapobase.lib",
					LibDir + "XAPOFX.lib"
				}
			);
		}
	}
}

