// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MassAIBehavior : ModuleRules
	{
		public MassAIBehavior(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AIModule"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MassEntity",
					"Core",
					"CoreUObject",
					"Engine",
					"GameplayTags",
					"MassActors",
					"MassCommon",
					"MassLOD",
					"MassMovement",
					"MassNavigation",
					"MassZoneGraphNavigation",
					"MassRepresentation",
					"MassSignals",
					"MassSmartObjects",
					"MassSpawner",
					"MassSimulation",
					"NavigationSystem",
					"SmartObjectsModule",
					"StateTreeModule",
					"StructUtils",
					"ZoneGraph",
					"ZoneGraphAnnotations",
					"MassGameplayExternalTraits"
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
		}
	}
}
