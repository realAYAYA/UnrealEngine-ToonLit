// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ExampleCustomDataInterface : ModuleRules
{
	public ExampleCustomDataInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core"
			});
			
		
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Projects",
				
				// Data interface dependencies
				"Niagara", "NiagaraCore", "VectorVM", "RenderCore", "RHI"
			});
		
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
