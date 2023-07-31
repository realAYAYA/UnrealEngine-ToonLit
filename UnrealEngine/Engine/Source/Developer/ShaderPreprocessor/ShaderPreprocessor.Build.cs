// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShaderPreprocessor : ModuleRules
{
	public ShaderPreprocessor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"RenderCore",
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "MCPP");
	}
}
