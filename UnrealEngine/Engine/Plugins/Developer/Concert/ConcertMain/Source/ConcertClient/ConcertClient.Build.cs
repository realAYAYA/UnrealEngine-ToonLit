// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertClient : ModuleRules
	{
		public ConcertClient(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",  // FH: Can we do without at some point?
					"ConcertTransport",
					"Serialization",
					"Concert"
				}
			);

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"ConcertTransport",
					"Concert",
					"GameplayTags",
				}
			);
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"GameplayTags",
					"Engine"
				}
			);
		}
	}
}
