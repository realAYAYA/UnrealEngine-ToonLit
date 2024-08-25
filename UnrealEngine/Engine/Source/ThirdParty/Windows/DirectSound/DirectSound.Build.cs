// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class DirectSound : ModuleRules
{
	public DirectSound(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicDependencyModuleNames.Add("DirectX");

			string DirectXLibDir = Target.WindowsPlatform.DirectXLibDir;
			PublicAdditionalLibraries.AddRange(
				new string[] {
					 Path.Combine(DirectXLibDir, "dxguid.lib"),
					 Path.Combine(DirectXLibDir, "dsound.lib")
				}
			);
		}
	}
}
