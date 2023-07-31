// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RigLogicDeveloper : ModuleRules
	{
		public RigLogicDeveloper(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimGraphRuntime",
					"AnimationCore",
					"Core",
					"CoreUObject",
					"Engine",
					"PropertyPath",
					"Slate",
					"SlateCore",
					"InputCore",
					"TimeManagement",
					"EditorWidgets",
					"MessageLog",
					"RigLogicModule"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimationCore",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"EditorFramework",
						"UnrealEd",
						"AnimGraph",
						"BlueprintGraph",
						"PropertyEditor",
						"GraphEditor",
					}
				);

				PrivateIncludePathModuleNames.Add("RigLogicEditor");
				DynamicallyLoadedModuleNames.Add("RigLogicEditor");
			}
		}
	}
}
