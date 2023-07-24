// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemOculus : ModuleRules
{
	public OnlineSubsystemOculus(ReadOnlyTargetRules Target) : base(Target)
    {		
		PublicDefinitions.Add("ONLINESUBSYSTEMOCULUS_PACKAGE=1");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"OnlineSubsystemUtils"
			}
			);
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"NetCore",
				"CoreUObject",
				"Engine",
				"Sockets",
				"OnlineSubsystem",
				"Projects",
				"PacketHandler",
                "Voice",
            }
			);

		PublicDependencyModuleNames.AddRange(new string[] { "LibOVRPlatform" });

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDelayLoadDLLs.Add("LibOVRPlatform64_1.dll");
		}
		else if (Target.Platform != UnrealTargetPlatform.Android)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
