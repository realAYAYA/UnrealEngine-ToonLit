// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using System;
using System.IO;
using UnrealBuildTool;

public class XInput : ModuleRules
{
	protected string DirectXSDKDir { get => DirectX.GetDir(Target); }

	public XInput(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Ensure correct include and link paths for xinput so the correct dll is loaded (xinput1_3.dll)
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			if (Target.Architecture.bIsX64)
			{
				PublicAdditionalLibraries.Add(DirectX.GetLibDir(Target) + "XInput.lib");
				PublicDependencyModuleNames.Add("DirectX");
			}
			else
			{
				Version WindowsSdkVersion;
				DirectoryReference WindowsSdkDir;
				if (!WindowsExports.TryGetWindowsSdkDir(null, out WindowsSdkVersion, out WindowsSdkDir))
				{
					throw new BuildException("Windows SDK must be installed in order to build this target.");
				}

				// add basic XInput library
				string WindowsSdkLibDir = Path.Combine(WindowsSdkDir.ToString(), "lib", WindowsSdkVersion.ToString(), "um", Target.Architecture.WindowsSystemLibDir);
				PublicAdditionalLibraries.Add(Path.Combine(WindowsSdkLibDir, "xinput.lib"));
			}
		}
	}
}

