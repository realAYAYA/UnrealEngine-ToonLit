// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StormSyncTransportServer : ModuleRules
	{
		public StormSyncTransportServer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
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
					"StormSyncTransportCore",
					"StormSyncTransportClient",
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"Slate",
					}
				);
			}
		}
	}
}