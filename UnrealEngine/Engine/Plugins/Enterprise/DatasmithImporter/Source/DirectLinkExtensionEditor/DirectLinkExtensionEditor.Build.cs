// Copyright Epic Games, Inc. All Rights Reserved.
using System;

namespace UnrealBuildTool.Rules
{
	public class DirectLinkExtensionEditor : ModuleRules
	{
		public DirectLinkExtensionEditor(ReadOnlyTargetRules Target)
			: base(Target)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"InputCore",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"ContentBrowser",
					"Core",
					"CoreUObject",
					"DirectLink",
					"DirectLinkExtension",
					"ExternalSource",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ContentBrowserData",
					"EditorStyle",
					"EditorWidgets",
					"MainFrame",
					"Projects",
					"PropertyEditor",
					"Slate",
					"SlateCore",
					"ToolMenus",
					"UnrealEd",
				}
			);
		}
	}
}