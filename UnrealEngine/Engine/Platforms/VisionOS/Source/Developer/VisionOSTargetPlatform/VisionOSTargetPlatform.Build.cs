// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VisionOSTargetPlatform : ModuleRules
{
	public VisionOSTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "IOS";
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"LaunchDaemonMessages",
				"IOSTargetPlatform",
				"Projects"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			"Messaging",
			"TargetDeviceServices",
		}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		PrivateIncludePaths.AddRange(
			new string[] {
			"Developer/IOS/IOSTargetPlatform/Private"
			}
		);
	}
}
