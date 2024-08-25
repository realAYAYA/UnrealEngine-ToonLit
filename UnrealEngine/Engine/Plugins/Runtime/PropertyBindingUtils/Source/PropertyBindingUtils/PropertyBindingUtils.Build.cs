// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PropertyBindingUtils : ModuleRules
	{
		public PropertyBindingUtils(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"StructUtils",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"RenderCore",
				"InputCore"
			}
			);

			if (Target.bBuildEditor)
			{
				PublicDependencyModuleNames.AddRange(
					new string[] {
						"UnrealEd",
						"BlueprintGraph",
					}
				);
			}
		}
	}
}
