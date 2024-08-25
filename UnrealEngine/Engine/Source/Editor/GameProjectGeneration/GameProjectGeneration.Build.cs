// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameProjectGeneration : ModuleRules
{
	public GameProjectGeneration(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"HardwareTargeting",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"ContentBrowser",
				"MainFrame",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
				"ApplicationCore",
				"AppFramework",
				"AssetTools",
				"ClassViewer",
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"InputCore",
				"Projects",
				"RenderCore",
				"Slate",
				"SlateCore",
				"EditorWidgets",
				"ToolWidgets",
				"SourceControl",
 				"TargetPlatform",
				"EditorFramework",
				"EditorSubsystem",
				"UnrealEd",
				"DesktopPlatform",
				"LauncherPlatform",
				"AddContentDialog",
				"AudioMixer",
				"AudioMixerCore",
				"ContentBrowserData"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"ContentBrowser",
				"Documentation",
				"MainFrame",
			}
		);

		if(Target.bWithLiveCoding)
		{
			PrivateIncludePathModuleNames.Add("LiveCoding");
		}
	}
}
