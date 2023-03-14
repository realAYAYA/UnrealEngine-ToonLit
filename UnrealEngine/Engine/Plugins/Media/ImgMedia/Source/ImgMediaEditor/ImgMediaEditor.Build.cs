// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImgMediaEditor : ModuleRules
	{
		public ImgMediaEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AssetTools",
					"ContentBrowser",
					"Core",
					"CoreUObject",
					"DesktopWidgets",				
					"EditorFramework",
					"ImageCore",
					"ImageWrapper",
					"ImgMedia",
					"InputCore",
					"MediaAssets",
					"RenderCore",
					"Slate",
					"SlateCore",
					"ToolMenus",
					"UnrealEd",
					"WorkspaceMenuStructure",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"ImgMedia/Private",
					"ImgMediaEditor/Private",
					"ImgMediaEditor/Private/Customizations",
					"ImgMediaEditor/Private/Factories",
				});

			// Are we using the engine?
			if (Target.bCompileAgainstEngine)
			{
				PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Engine",
				});
			}

			bool bLinuxEnabled = Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64");

			// Add EXR support.
			if ((Target.Platform == UnrealTargetPlatform.Mac) ||
				Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
				bLinuxEnabled)
			{
				PrivateDependencyModuleNames.Add("OpenExrWrapper");
				PublicDefinitions.Add("IMGMEDIAEDITOR_EXR_SUPPORTED_PLATFORM=1");
			}
			else
			{
				PublicDefinitions.Add("IMGMEDIAEDITOR_EXR_SUPPORTED_PLATFORM=0");
			}
		}
	}
}
