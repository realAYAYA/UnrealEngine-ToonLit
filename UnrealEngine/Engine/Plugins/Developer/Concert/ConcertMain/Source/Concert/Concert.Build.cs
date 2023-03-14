// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Concert : ModuleRules
	{
		public Concert(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",  // FH: Can we do without at some point?
					"ConcertTransport",
					"Serialization",
				}
			);

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"ConcertTransport",
				}
			);

			//PrivateDependencyModuleNames.AddRange(
			//	new string[]
			//	{
			//		"ConcertTransport",
			//	}
			//);
		}
	}
}
