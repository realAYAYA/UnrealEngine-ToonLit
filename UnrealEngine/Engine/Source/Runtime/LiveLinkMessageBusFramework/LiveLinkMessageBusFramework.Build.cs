// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkMessageBusFramework : ModuleRules
{
    public LiveLinkMessageBusFramework(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePathModuleNames.Add("Messaging");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"LiveLinkInterface",
				"MessagingCommon"
			}
		);
	}
}
