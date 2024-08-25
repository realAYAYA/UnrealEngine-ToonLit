// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClonerEffector : ModuleRules
{
	public ClonerEffector(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Niagara",
				"NiagaraCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",
				"DynamicMesh",
				"ProceduralMeshComponent",
				"StaticMeshDescription"
			}
		);

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"Slate",
				"SlateCore"
			});
		}

		ShortName = "CE";
	}
}
