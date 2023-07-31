// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UATHelper : ModuleRules
	{
		public UATHelper(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"Slate",
					"SlateCore",
					"GameProjectGeneration",
					"EditorFramework",
					"UnrealEd",
					"Analytics",
					"OutputLog",
			    }
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"MessageLog",
				}
			);
		}
	}
}
