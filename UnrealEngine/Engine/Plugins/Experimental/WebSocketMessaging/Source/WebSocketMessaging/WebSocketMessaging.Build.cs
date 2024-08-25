// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebSocketMessaging : ModuleRules
{
	public WebSocketMessaging(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Messaging",
				"WebSockets",
				"WebSocketNetworking",
				"Json",
				"JsonUtilities",
				"Cbor",
				"Serialization"
			}
		);
	}
}
