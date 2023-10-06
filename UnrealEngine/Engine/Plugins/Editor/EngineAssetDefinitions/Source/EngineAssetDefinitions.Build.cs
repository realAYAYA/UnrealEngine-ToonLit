// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EngineAssetDefinitions : ModuleRules
{
	public EngineAssetDefinitions(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"SkeletalMeshEditor",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ContentBrowser",
				"ContentBrowserData",
				"AssetDefinition",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"RHI",
				"Engine",
				"AssetRegistry",
				"AssetTools",
				"SlateCore",
				"InputCore",
				"Slate",
				"Kismet",
				"DesktopPlatform",
				"Foliage",
				"Landscape",
				"PhysicsCore",
				"Kismet",
				"GameProjectGeneration",
				"AnimationBlueprintEditor",
				"ToolWidgets",
                "SourceControl",
				"Blutility",
				"UMGEditor",
				"ToolWidgets",
				"BlueprintGraph"
			}
		);
		
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"VirtualTexturingEditor",
				"CurveAssetEditor",
				"StaticMeshEditor",
				"TextureEditor",
				"MaterialEditor",
				"PhysicsAssetEditor",
				"FontEditor",
				"DataTableEditor",
				"CurveTableEditor",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"VirtualTexturingEditor",
				"CurveAssetEditor",
				"StaticMeshEditor",
				"TextureEditor",
				"MaterialEditor",
				"PhysicsAssetEditor",
				"FontEditor",
				"DataTableEditor",
				"CurveTableEditor",
			}
		);
	}
}
