// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AppleVisionBlueprintSupport : ModuleRules
	{
		public AppleVisionBlueprintSupport(ReadOnlyTargetRules Target) : base(Target)
		{

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Engine",
					"BlueprintGraph",
					"AppleVision"
					// ... add other public dependencies that you statically link with here ...
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject"
					// ... add private dependencies that you statically link with here ...
				}
			);
		}
	}
}
