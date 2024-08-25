// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MassSmartObjects : ModuleRules
	{
		public MassSmartObjects(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MassEntity",
					"Core",
					"CoreUObject",
					"Engine",
					"GameplayTags",
					"MassCommon",
					"MassLOD",
					"MassMovement",
					"MassSignals",
					"MassSimulation",
					"MassSpawner",
					"SmartObjectsModule",
					"StructUtils",
					"ZoneGraph",
					"ZoneGraphAnnotations",
					"MassGameplayExternalTraits"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MassActors"
				}
			);
		}
	}
}
