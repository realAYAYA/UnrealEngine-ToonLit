// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextInterface : ModuleRules
	{
		public AnimNextInterface(ReadOnlyTargetRules Target) : base(Target)
		{	
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);

		}
	}
}