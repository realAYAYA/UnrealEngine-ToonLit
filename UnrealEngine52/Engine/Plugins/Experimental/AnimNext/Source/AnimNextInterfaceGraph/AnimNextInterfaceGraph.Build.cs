// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextInterfaceGraph : ModuleRules
	{
		public AnimNextInterfaceGraph(ReadOnlyTargetRules Target) : base(Target)
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
					"AnimNextInterface",
					"Engine"
				}
			);
		}
	}
}