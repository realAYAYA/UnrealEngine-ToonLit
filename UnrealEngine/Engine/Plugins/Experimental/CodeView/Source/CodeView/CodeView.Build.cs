// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CodeView : ModuleRules
	{
		public CodeView(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Slate",
                    "InputCore",
					"Core",
					"CoreUObject",
					"Engine",
					"EditorFramework",
					"UnrealEd",
					"PropertyEditor",
					"DesktopPlatform",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DetailCustomizations",
					"SlateCore",
					"SourceCodeAccess"
				}
			);
		}
	}
}
