// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OnlineSubsystemFacebook : ModuleRules
{
	protected virtual bool bUsesRestfulImpl
	{
		get =>
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.IsInPlatformGroup(UnrealPlatformGroup.Unix);
	}

	public OnlineSubsystemFacebook(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.Add("ONLINESUBSYSTEMFACEBOOK_PACKAGE=1");
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
				"OnlineSubsystem", 
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Facebook");

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicDefinitions.Add("WITH_FACEBOOK=1");
			PrivateIncludePaths.Add("Private/IOS");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			bool bHasFacebookSDK = false;
			string FacebookNFLDir = "";
			try
			{
				FacebookNFLDir = System.IO.Path.Combine(EngineDirectory, "Restricted/NotForLicensees/Plugins/Online/OnlineSubsystemFacebook/Source/ThirdParty/Android/FacebookSDK");
				bHasFacebookSDK = System.IO.Directory.Exists(FacebookNFLDir);
			}
			catch (System.Exception)
			{
			}

			PrivateIncludePaths.Add("Private/Android");

			if (bHasFacebookSDK)
			{
				string Err = string.Format("Facebook SDK found in {0}", FacebookNFLDir);
				System.Console.WriteLine(Err);

				PublicDefinitions.Add("WITH_FACEBOOK=1");

				PrivateDependencyModuleNames.AddRange(
				new string[] {
				"Launch",
				}
				);

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "OnlineSubsystemFacebook_UPL.xml"));
			}
			else
			{
				string Err = string.Format("Facebook SDK not found in {0}", FacebookNFLDir);
				System.Console.WriteLine(Err);
				PublicDefinitions.Add("WITH_FACEBOOK=0");
			}
		}
		else if (bUsesRestfulImpl)
		{
			PublicDefinitions.Add("WITH_FACEBOOK=1");
			PrivateIncludePaths.Add("Private/Rest");
		}
        else
        {
			PublicDefinitions.Add("WITH_FACEBOOK=0");

			PrecompileForTargets = PrecompileTargetsType.None;
		}

		PublicDefinitions.Add("USES_RESTFUL_FACEBOOK=" + (bUsesRestfulImpl ? "1" : "0"));
	}
}
