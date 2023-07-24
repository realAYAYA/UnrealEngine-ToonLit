// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CascadeToNiagaraConverter : ModuleRules
{
	public CascadeToNiagaraConverter(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateIncludePaths.AddRange(new string[] {
				"CascadeToNiagaraConverter/Private",
		});
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
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
