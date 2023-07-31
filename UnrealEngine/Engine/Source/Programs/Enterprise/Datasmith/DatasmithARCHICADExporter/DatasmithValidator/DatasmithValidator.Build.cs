// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithValidator : ModuleRules
	{
		public DatasmithValidator(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
                    "CoreUObject",
                    "MeshDescription",
					"DatasmithCore",
					"DatasmithExporter",
                    //"DatasmithImporter",
				}
			);
		}
	}
}
