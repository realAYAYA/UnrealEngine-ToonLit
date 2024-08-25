// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class AvalancheTextEditor : ModuleRules
{
	public AvalancheTextEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"AvalancheShapesEditor",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Avalanche",
				"AvalancheComponentVisualizers",
				"AvalancheInteractiveTools",
				"AvalancheLevelViewport",
				"AvalancheText",
				"ComponentVisualizers",
				"Core",
				"CoreUObject",
				"DynamicMaterialEditor",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Engine",
				"FreeType2",
				"InputCore",
				"InteractiveToolsFramework",
				"Projects",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"Text3D",
				"UnrealEd",
			}
		);

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PrivateDependencyModuleNames.Add("DirectX");

			PublicAdditionalLibraries.AddRange(new string[] {
				Path.Combine(Target.WindowsPlatform.DirectXLibDir, "dwrite.lib")
			});
		}

		// Needed for FreeType/Font functionality
		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"zlib",
			"UElibPNG"
		);

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AvalancheEditorCore"
			});
		}
	}
}
