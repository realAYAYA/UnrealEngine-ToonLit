// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Networking : ModuleRules
	{
		public Networking(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Sockets"
				});

			UnsafeTypeCastWarningLevel = WarningLevel.Error;
		}
	}
}
