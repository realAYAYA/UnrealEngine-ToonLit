// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DynamicMaterialEditor : ModuleRules
{
	public DynamicMaterialEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DynamicMaterial"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AppFramework",
				"ApplicationCore",
				"AssetDefinition",
				"ContentBrowser",
				"ContentBrowserData",
				"CustomDetailsView",
				"DeveloperSettings",
				"EditorWidgets",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"MaterialEditor",
				"Projects",
				"PropertyEditor",
				"RenderCore",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TypedElementRuntime",
				"UMG",
				"UnrealEd",
				"WorkspaceMenuStructure"
			}
		);

		ShortName = "DynMatEd";
	}
}
