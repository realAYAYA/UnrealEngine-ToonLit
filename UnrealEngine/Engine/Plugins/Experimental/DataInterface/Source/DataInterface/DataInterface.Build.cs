// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataInterface : ModuleRules
	{
		public DataInterface(ReadOnlyTargetRules Target) : base(Target)
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