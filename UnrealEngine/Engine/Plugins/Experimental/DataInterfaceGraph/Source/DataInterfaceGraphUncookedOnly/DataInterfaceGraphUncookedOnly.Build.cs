// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataInterfaceGraphUncookedOnly : ModuleRules
	{
		public DataInterfaceGraphUncookedOnly(ReadOnlyTargetRules Target) : base(Target)
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
					"DataInterface",
					"DataInterfaceGraph",
					"BlueprintGraph",	// For K2 schema
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
			}
		}
	}
}