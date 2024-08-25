// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class OnlineSubsystemApple : ModuleRules
{
	bool bSignInWithAppleSupported = false;

	public OnlineSubsystemApple(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS))
		{
			ConfigHierarchy PlatformGameConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Target.ProjectFile), UnrealTargetPlatform.IOS);
			PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableSignInWithAppleSupport", out bSignInWithAppleSupported);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// TODO: Mark.Fitt enable Mac support
		}

		PublicDefinitions.Add("ONLINESUBSYSTEMAPPLE_PACKAGE=1");
		PublicDefinitions.Add("ONLINESUBSYSTEMAPPLE_IDENTITY_ENABLE_SIWA=" + (bSignInWithAppleSupported ? "1" : "0"));

		PrivateDependencyModuleNames.AddRange(
		new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"OnlineSubsystem",
        });

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Apple))
		{
			if (bSignInWithAppleSupported)
			{
				PublicWeakFrameworks.Add("AuthenticationServices");
			}
		}
	}
}
