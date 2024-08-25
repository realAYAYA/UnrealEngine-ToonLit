// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DMXPixelMappingEditor : ModuleRules
{
	public DMXPixelMappingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

		PrivateDependencyModuleNames.AddRange( new string[] {
			"AppFramework",
			"ApplicationCore",
			"Core",
			"CoreUObject",
			"DMXEditor",
			"DMXPixelMappingCore",
			"DMXPixelMappingEditorWidgets",
			"DMXPixelMappingRuntime",
			"DMXPixelMappingRenderer",
			"DMXPixelMappingBlueprintGraph",
			"DMXRuntime",
			"EditorStyle",
			"Engine",
			"InputCore",
			"Projects",
			"PropertyEditor",
			"RenderCore",
			"RHI",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"ToolWidgets",
			"UMG",
			"UnrealEd",
		});
	}
}
