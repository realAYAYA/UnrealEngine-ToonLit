// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InstancedActors : ModuleRules
	{
		public InstancedActors(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"GameplayTags",
					"MassEntity",
					"MassCommon",
					"MassActors",
					"MassRepresentation",
					"MassSpawner",
					"MassLOD",
					"MassSmartObjects",
					"MassSignals",
					"StructUtils",
					"DataRegistry",
					"DeveloperSettings",
					"NetCore",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[] {
						"InputCore",
						"MassGameplayDebug"
					}
				);
			}

			SetupGameplayDebuggerSupport(Target);
		}
	}
}
