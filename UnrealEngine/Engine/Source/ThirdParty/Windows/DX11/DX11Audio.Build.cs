// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Audio : ModuleRules
{
	public DX11Audio(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDependencyModuleNames.Add("DirectX");

			string DirectXLibDir = DirectX.GetLibDir(Target);
			PublicAdditionalLibraries.AddRange(
				new string[] 
				{
					DirectXLibDir + "dxguid.lib",
					DirectXLibDir + "xapobase.lib"
				}
			);
		}
	}
}

