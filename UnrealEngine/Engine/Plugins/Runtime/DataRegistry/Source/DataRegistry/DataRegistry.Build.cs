// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataRegistry : ModuleRules
	{
		public DataRegistry(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"GameplayTags",
					"DeveloperSettings"
				}
			);

			// Needed for PIE callbacks, which should really be somewhere better
			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd"
					}
				);
			}
		}
	}
}
