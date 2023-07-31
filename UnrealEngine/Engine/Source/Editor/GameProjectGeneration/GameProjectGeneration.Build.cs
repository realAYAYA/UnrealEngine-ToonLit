// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameProjectGeneration : ModuleRules
{
	public GameProjectGeneration(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/AddContentDialog/Private",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"HardwareTargeting",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"ContentBrowser",
				"DesktopPlatform",
				"LauncherPlatform",
				"MainFrame",
				"AddContentDialog",
				"HardwareTargeting",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
				"ApplicationCore",
				"AppFramework",
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
				"HardwareTargeting",
				"AddContentDialog",
				"AudioMixer",
				"AudioMixerCore"
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
