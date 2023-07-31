// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OnlineSubsystemGoogle : ModuleRules
{
	protected virtual bool bUsesRestfulImpl
	{
		get =>
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.IsInPlatformGroup(UnrealPlatformGroup.Unix);
	}

	public OnlineSubsystemGoogle(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.Add("ONLINESUBSYSTEMGOOGLE_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreOnline",
				"CoreUObject",
				"ApplicationCore",
				"HTTP",
				"ImageCore",
				"Json",
				"Sockets",
				"OnlineSubsystem", 
			}
			);

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicDefinitions.Add("WITH_GOOGLE=1");
			PrivateIncludePaths.Add("Private/IOS");

			// These are iOS system libraries that Google depends on
			PublicFrameworks.AddRange(
			new string[] {
				"SafariServices",
				"SystemConfiguration"
			});

			PublicAdditionalFrameworks.Add(
			new Framework(
				"GoogleSignIn",
				"ThirdParty/IOS/GoogleSignInSDK/GoogleSignIn.embeddedframework.zip",
				"GoogleSignIn.bundle"
			)
			);

			PublicAdditionalFrameworks.Add(
			new Framework(
				"GoogleSignInDependencies",
				"ThirdParty/IOS/GoogleSignInSDK/GoogleSignInDependencies.embeddedframework.zip"
			)
			);
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Launch",
			}
			);

			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "OnlineSubsystemGoogle_UPL.xml"));

			PrivateIncludePaths.Add("Private/Android");
		}
		else if (bUsesRestfulImpl)
		{
			PrivateIncludePaths.Add("Private/Rest");
		}
		else
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}

		PublicDefinitions.Add("USES_RESTFUL_GOOGLE=" + (bUsesRestfulImpl ? "1" : "0"));
	}
}
