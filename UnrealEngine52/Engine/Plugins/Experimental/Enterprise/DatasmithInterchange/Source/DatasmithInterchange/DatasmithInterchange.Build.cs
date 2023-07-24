// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{	public class DatasmithInterchange : ModuleRules
	{
		public DatasmithInterchange(ReadOnlyTargetRules Target)
			: base(Target)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithContent",
					"DatasmithCore",
					"DatasmithTranslator",
					"Engine",
					"ExternalSource",
					"InterchangeEngine",
					"InterchangeNodes",
					"InterchangeCommonParser",
					"InterchangeFactoryNodes",
					"InterchangeImport",
					"InterchangePipelines",
					"MeshDescription"
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"DatasmithImporter",
						"DesktopPlatform",
						"InputCore",
						"InterchangeEditorPipelines",
						"Slate",
						"SlateCore",
						"ToolMenus",
						"UnrealEd",
					}
				);
			}

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"InterchangeCore",
					"InterchangeImport",
				}
			);
		}
	}
}