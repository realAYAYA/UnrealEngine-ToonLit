// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelEditor : ModuleRules
{
	public LevelEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(GetModuleDirectory("SceneOutliner"), "Private"),
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"ClassViewer",
				"MainFrame",
                "PlacementMode",
				"SlateReflector",
                "PortalServices",
				"MergeActors",
				"Layers",
				"WorldBrowser",
				"NewLevelDialog",
				"LocalizationDashboard",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"CommonMenuExtensions",
				"Settings",
				"ToolWidgets",
				"UnrealEd",
				"VREditor",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"LevelSequence",
				"Analytics",
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"LauncherPlatform",
				"InputCore",
				"Slate",
				"SlateCore",
				"Engine",
				"MessageLog",
				"SourceControl",
				"SourceControlWindows",
				"StatsViewer",
				"EditorFramework",
				"UnrealEd", 
				"DeviceProfileServices",
				"ContentBrowser",
				"SceneOutliner",
				"ActorPickerMode",
				"RHI",
				"Projects",
				"TypedElementFramework",
				"TypedElementRuntime",
				"EngineSettings",
				"PropertyEditor",
				"KismetWidgets",
				"Foliage",
				"HierarchicalLODOutliner",
				"HierarchicalLODUtilities",
				"MaterialShaderQualitySettings",
				"PixelInspectorModule",
				"CommonMenuExtensions",
				"ToolMenus",
				"StatusBar",
				"AppFramework",
				"EditorSubsystem",
				"EnvironmentLightingViewer",
				"DesktopPlatform",
				"DataLayerEditor",
				"TranslationEditor",
				"SubobjectEditor",
				"SubobjectDataInterface",
				"DerivedDataEditor",
				"EditorWidgets",
				"ToolWidgets",
				"UnsavedAssetsTracker",
				"UncontrolledChangelists",
				"RenderCore",
				"DeveloperSettings",
				"ActionableMessage"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"ClassViewer",
				"DeviceManager",
				"SettingsEditor",
				"SlateReflector",
				"AutomationWindow",
				"Layers",
				"WorldBrowser",
				"WorldPartitionEditor",
				"AssetTools",
				"WorkspaceMenuStructure",
				"NewLevelDialog",
				"DeviceProfileEditor",
                "PlacementMode",
				"HeadMountedDisplay",
				"VREditor",
                "Persona",
				"MergeActors"
			}
		);

		if (Target.bBuildTargetDeveloperTools)
		{
			DynamicallyLoadedModuleNames.Add("SessionFrontend");
		}


		if (Target.bWithLiveCoding)
		{
			PrivateIncludePathModuleNames.Add("LiveCoding");
		}
	}
}
