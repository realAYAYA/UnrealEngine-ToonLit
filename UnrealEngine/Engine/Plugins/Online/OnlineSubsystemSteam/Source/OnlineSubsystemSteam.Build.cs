// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemSteam : ModuleRules
{
	public OnlineSubsystemSteam(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.Add("ONLINESUBSYSTEMSTEAM_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"OnlineSubsystemUtils"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject",
				"NetCore",
				"Engine", 
				"Sockets", 
				"Voice",
                "AudioMixer",
				"OnlineBase",
				"OnlineSubsystem",
				"Json",
				"PacketHandler",
				"Projects",
                "SteamShared"
            }
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
