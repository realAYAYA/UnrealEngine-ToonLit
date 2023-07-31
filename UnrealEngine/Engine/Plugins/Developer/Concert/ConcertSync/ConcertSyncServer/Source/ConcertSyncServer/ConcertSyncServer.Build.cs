// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertSyncServer : ModuleRules
	{
		public ConcertSyncServer(ReadOnlyTargetRules Target) : base(Target)
		{
			// Make the linking path shorter (to prevent busting the Windows limit) when linking ConcertSyncServer.lib against an executable that have a long name.
			ShortName = "CncrtSyncSvr";

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Concert",
					"ConcertSyncCore",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ConcertTransport",
					"Serialization",
				}
			);
		}
	}
}
