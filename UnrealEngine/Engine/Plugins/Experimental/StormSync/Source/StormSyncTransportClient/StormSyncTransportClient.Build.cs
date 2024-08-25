// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StormSyncTransportClient : ModuleRules
	{
		public StormSyncTransportClient(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"StormSyncTransportCore"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"Json",
					"JsonUtilities",
					"Networking",
					"Sockets",
					"StormSyncCore",
				}
			);
		}
	}
}
