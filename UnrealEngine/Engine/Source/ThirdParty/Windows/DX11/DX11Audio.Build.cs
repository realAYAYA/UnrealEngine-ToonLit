// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class DX11Audio : ModuleRules
{
	public DX11Audio(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDependencyModuleNames.Add("DirectX");

			string DirectXLibDir = Target.WindowsPlatform.DirectXLibDir;
			PublicAdditionalLibraries.AddRange(
				new string[] 
				{
					Path.Combine(DirectXLibDir, "dxguid.lib"),
					Path.Combine(DirectXLibDir, "xapobase.lib")
				}
			);
		}
	}
}

