// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StylusInput : ModuleRules
	{
		public StylusInput(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"EditorSubsystem",
					"Engine",
					"EditorFramework",
					"UnrealEd"
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"MainFrame",
					"SlateCore",
					"Slate",
					"WorkspaceMenuStructure"
					// ... add private dependencies that you statically link with here ...
				}
				);
		}
	}
}
