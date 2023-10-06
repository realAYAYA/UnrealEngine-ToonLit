// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayTasks : ModuleRules
	{
        public GameplayTasks(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"NetCore"
                    //"GameplayTags",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
				CircularlyReferencedDependentModules.Add("UnrealEd");
                //PrivateDependencyModuleNames.Add("GameplayTagsEditor");
			}

			SetupIrisSupport(Target);
		}
	}
}
