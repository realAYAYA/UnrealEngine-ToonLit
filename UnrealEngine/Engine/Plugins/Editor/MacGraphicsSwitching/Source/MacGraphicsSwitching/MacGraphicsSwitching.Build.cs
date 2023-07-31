// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MacGraphicsSwitching : ModuleRules
	{
        public MacGraphicsSwitching(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
                    "InputCore",
					"LevelEditor",
					"Slate"
				}
			);

            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",
					"SlateCore",
					"EditorFramework",
					"UnrealEd"
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] 
				{
					"MainFrame",
					"Settings",
					"SettingsEditor"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] 
				{
					"MainFrame",
					"Settings",
					"SettingsEditor"
				}
			);
		}
	}
}
