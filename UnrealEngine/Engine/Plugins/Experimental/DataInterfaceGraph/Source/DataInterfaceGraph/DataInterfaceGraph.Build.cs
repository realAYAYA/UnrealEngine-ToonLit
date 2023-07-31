// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataInterfaceGraph : ModuleRules
	{
		public DataInterfaceGraph(ReadOnlyTargetRules Target) : base(Target)
		{
		
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"RigVM",
					"ControlRig",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DataInterface",
				}
			);
		}
	}
}