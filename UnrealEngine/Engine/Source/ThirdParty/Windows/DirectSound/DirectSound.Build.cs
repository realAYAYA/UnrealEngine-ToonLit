// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DirectSound : ModuleRules
{
	public DirectSound(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicDependencyModuleNames.Add("DirectX");

			string DirectXLibDir = DirectX.GetLibDir(Target);
			PublicAdditionalLibraries.AddRange(
				new string[] {
					 DirectXLibDir + "dxguid.lib",
					 DirectXLibDir + "dsound.lib"
				}
			);
		}
	}
}
