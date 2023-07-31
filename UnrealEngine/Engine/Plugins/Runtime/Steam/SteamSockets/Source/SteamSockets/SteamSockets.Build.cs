// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SteamSockets : ModuleRules
{
    public SteamSockets(ReadOnlyTargetRules Target) : base(Target)
    {
		PublicDefinitions.Add("STEAMSOCKETS_MODULE=1");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject",
				"NetCore",
				"Engine", 
				"Sockets",
				"OnlineSubsystem",
				"OnlineSubsystemSteam",
				"PacketHandler",
                "SteamShared"
            }
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
