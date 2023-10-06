// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LauncherServices : ModuleRules
	{
		public LauncherServices(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				});

			PublicIncludePathModuleNames.AddRange(
				new string[] {
					"TargetDeviceServices",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"DesktopPlatform",
					"SessionMessages",
					"SourceCodeAccess",
					"TargetPlatform",
					"UnrealEdMessages",
					"Json",
					"TurnkeyIO",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"DerivedDataCache",
				});
		}
	}
}
