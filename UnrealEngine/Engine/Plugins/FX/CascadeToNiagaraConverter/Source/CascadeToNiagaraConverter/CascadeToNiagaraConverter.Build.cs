// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CascadeToNiagaraConverter : ModuleRules
{
	public CascadeToNiagaraConverter(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MessageLog",
			}
		);	
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
              	"NiagaraCore",
				"Engine",
				"AssetRegistry",
				"Niagara",
				"NiagaraEditor",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"PythonScriptPlugin",
			}
		);

        PublicIncludePathModuleNames.AddRange(
            new string[]
            {
				"Sequencer",
			}
		);

	}
}
