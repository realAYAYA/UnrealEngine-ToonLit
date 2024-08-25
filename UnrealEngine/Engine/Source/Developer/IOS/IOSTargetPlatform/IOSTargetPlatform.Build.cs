// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IOSTargetPlatform : ModuleRules
{
	public IOSTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
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
				"Projects",
				"Json",
				"AudioPlatformConfiguration",
				"Sockets",
				"Networking"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MessagingCommon",
				"TargetDeviceServices",
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add("/System/Library/PrivateFrameworks/MobileDevice.framework/Versions/Current/MobileDevice");
		}
	}
}
