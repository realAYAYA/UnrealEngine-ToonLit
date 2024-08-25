// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Input : ModuleRules
{
	public DX11Input(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDependencyModuleNames.Add("DirectX");

		string LibDir = null;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibDir = Target.WindowsPlatform.DirectXLibDir;
		}

		PublicAdditionalLibraries.AddRange(
			new string[] {
				LibDir + "dxguid.lib",
				LibDir + "dinput8.lib"
			}
			);
	}
}

