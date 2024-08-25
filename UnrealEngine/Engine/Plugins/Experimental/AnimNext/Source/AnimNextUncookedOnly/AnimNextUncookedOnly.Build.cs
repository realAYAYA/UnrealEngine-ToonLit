// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextUncookedOnly : ModuleRules
	{
		public AnimNextUncookedOnly(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"RigVM",
					"RigVMDeveloper",
					"ControlRig",
					"ControlRigDeveloper",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AnimNext",
					"BlueprintGraph",	// For K2 schema
					"AnimationCore",
					"AnimGraph",
					"Kismet",
					"Slate",
					"SlateCore",
					"StructUtils",
					"StructUtilsEditor",
					"ToolMenus",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
					}
				);

				PrivateIncludePathModuleNames.AddRange(
					new string[]
					{
						"AnimNextEditor",
					}
				);

				DynamicallyLoadedModuleNames.AddRange(
					new string[]
					{
						"AnimNextEditor",
					}
				);
			}
		}
	}
}