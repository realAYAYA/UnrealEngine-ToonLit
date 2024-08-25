// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class SVGImporterEditor : ModuleRules
{
	public SVGImporterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Nanosvg");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetDefinition",
				"AssetTools",
				"ContentBrowser",
				"CoreUObject",
				"DetailCustomizations",
				"EditorFramework",
				"Engine",
				"InputCore",
				"LevelEditor",
				"MaterialX", // Used for pugi xml
				"Projects",
				"PropertyEditor",
				"RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
				"SVGImporter",
				"ToolMenus",
				"TypedElementRuntime",
				"UnrealEd",
				"XmlParser",
			}
		);
	}
}
