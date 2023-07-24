// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Input : ModuleRules
{
	public DX11Input(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(DirectX.GetIncludeDir(Target));

		string LibDir = null;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibDir = DirectX.GetLibDir(Target);
		}

		PublicAdditionalLibraries.AddRange(
			new string[] {
				LibDir + "dxguid.lib",
				LibDir + "dinput8.lib"
			}
			);
	}
}

