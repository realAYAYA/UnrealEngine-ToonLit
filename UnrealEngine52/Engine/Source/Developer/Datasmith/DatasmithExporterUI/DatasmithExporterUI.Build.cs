// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithExporterUI : ModuleRules
	{
		public DatasmithExporterUI(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DatasmithCore",
					"DatasmithExporter",
					"DesktopPlatform",
					"DirectLink",
					"InputCore",
					"Slate",
					"SlateCore",
				});
		}
	}
}