// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DatasmithCloTranslator : ModuleRules
{
	public DatasmithCloTranslator(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DatasmithContent",
				"DatasmithCore",
				"DatasmithTranslator",
				"Json",
				"MeshDescription",
			}
		);
	}
}
