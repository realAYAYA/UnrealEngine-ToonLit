// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTTP : ModuleRules
{
	protected virtual bool bPlatformSupportsWinHttp
	{
		get
		{
			return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows);
		}
	}

	protected virtual bool bPlatformSupportsLibCurl
	{
		get
		{
			return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Android);
		}
	}
	protected virtual bool bPlatformSupportsXCurl { get { return false; } }

	private bool bPlatformSupportsCurl { get { return bPlatformSupportsLibCurl || bPlatformSupportsXCurl; } }

	protected virtual bool bPlatformRequiresOpenSSL
	{
		get
		{
			return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Android);
		}
	}

	public HTTP(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("HTTP_PACKAGE=1");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SSL",
			}
			);

		if (bPlatformSupportsCurl)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Sockets",
				}
			);

			if (bPlatformSupportsLibCurl && !bPlatformSupportsXCurl)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");

				PublicDefinitions.Add("CURL_ENABLE_DEBUG_CALLBACK=1");
				if (Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PublicDefinitions.Add("CURL_ENABLE_NO_TIMEOUTS_OPTION=1");
				}
			}
		}

		PrivateDefinitions.Add("WITH_CURL_LIBCURL =" + (bPlatformSupportsLibCurl ? "1" : "0"));
		PrivateDefinitions.Add("WITH_CURL_XCURL=" + (bPlatformSupportsXCurl ? "1" : "0"));
		PrivateDefinitions.Add("WITH_CURL= " + ((bPlatformSupportsLibCurl || bPlatformSupportsXCurl) ? "1" : "0"));

		// Use Curl over WinHttp on platforms that support it (until WinHttp client security is in a good place at the least)
		if (bPlatformSupportsWinHttp)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "WinHttp");
			PublicDefinitions.Add("WITH_WINHTTP=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_WINHTTP=0");
		}

		if (bPlatformRequiresOpenSSL)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}

		if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS || Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("Security");
		}
	}
}
