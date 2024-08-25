// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraBlueprintNodes : ModuleRules
{
	public NiagaraBlueprintNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
				"CoreUObject",
				"UnrealEd",
				"Engine",
				"NiagaraCore",
				"Niagara",
				"Kismet",
				"KismetCompiler",
			}
		);
	}
}
