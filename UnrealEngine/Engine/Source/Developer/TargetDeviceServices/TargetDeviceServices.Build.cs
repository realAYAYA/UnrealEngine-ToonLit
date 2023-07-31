// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TargetDeviceServices : ModuleRules
	{
		public TargetDeviceServices(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"TargetPlatform",
					"DesktopPlatform",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"MessagingCommon",
				});
		}
	}
}
