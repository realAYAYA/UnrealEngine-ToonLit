// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Documentation : ModuleRules
	{
		public Documentation(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"MainFrame"
					// ... add other public dependencies that you statically link with here ...
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "Slate",
					"SlateCore",
					"DeveloperSettings",
					"Projects",
					"EditorFramework",
                    "UnrealEd",
					"Analytics",
					"SourceCodeAccess",
					"SourceControl",
                    "ContentBrowser",
					"DesktopPlatform",
					"ToolWidgets",
				}
			);

			CircularlyReferencedDependentModules.Add("SourceControl");

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
                    "MessageLog"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"MessageLog"
				}
			);
		}
	}
}
