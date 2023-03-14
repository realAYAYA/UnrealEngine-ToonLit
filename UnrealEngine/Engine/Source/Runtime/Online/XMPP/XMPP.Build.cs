// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XMPP : ModuleRules
{
	protected virtual bool bTargetPlatformSupportsJingle { get { return false; } }

	protected virtual bool bTargetPlatformSupportsStrophe
	{
		get =>
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Android ||
			Target.Platform == UnrealTargetPlatform.IOS ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.IsInPlatformGroup(UnrealPlatformGroup.Unix);
	}

	protected virtual bool bRequireOpenSSL { get { return false; } }

	public XMPP(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("XMPP_PACKAGE=1");

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"Json"
			}
		);

		bool TargetPlatformSupportsJingle = bTargetPlatformSupportsJingle;
		bool TargetPlatformSupportsStrophe = bTargetPlatformSupportsStrophe;

		if (TargetPlatformSupportsJingle)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "WebRTC");
			PrivateDefinitions.Add("WITH_XMPP_JINGLE=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_XMPP_JINGLE=0");
		}

		if (TargetPlatformSupportsStrophe)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libstrophe");
			PrivateDependencyModuleNames.Add("WebSockets");
			PrivateDefinitions.Add("WITH_XMPP_STROPHE=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_XMPP_STROPHE=0");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			bRequireOpenSSL)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
		}
	}
}
