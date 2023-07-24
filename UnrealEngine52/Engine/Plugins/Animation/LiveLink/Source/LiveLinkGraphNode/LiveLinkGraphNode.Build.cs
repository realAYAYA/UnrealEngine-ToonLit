// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkGraphNode : ModuleRules
	{
		public LiveLinkGraphNode(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{

				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimationCore",
					"AnimGraphRuntime",
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"KismetCompiler",
					"LiveLink",
					"LiveLinkAnimationCore",
					"LiveLinkInterface",
					"Persona",
					"SlateCore",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"AnimGraph",
					}
				);

				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"EditorFramework",
						"UnrealEd",
						"Kismet",
						"BlueprintGraph",
					}
				);
			}
		}
	}
}
