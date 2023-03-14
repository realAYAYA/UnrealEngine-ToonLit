// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ElectraHTTPStream : ModuleRules
{
	// Returns true when this platform uses WinHttp
	protected virtual bool bPlatformUsesWinHttp
	{
		get
		{
			return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows);
		}
	}

	// Returns whether or not this platform uses libCurl
	protected virtual bool bPlatformUsesLibCurl
	{
		get
		{
			return Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) || Target.IsInPlatformGroup(UnrealPlatformGroup.Android);
		}
	}


	// Returns true if this platform supports xCurl (from Microsoft Game Core)
	protected virtual bool bPlatformUsesXCurl
	{
		get
		{
			return false;
		}
	}


	// Returns true if the platform requires all connections to be secure https:// connections.
	protected virtual bool bPlatformRequiresSecureConnections
	{
		get
		{
			return false;
		}
	}

	// Returns whether or not sockets are required for whatever HTTP library is being used.
	protected virtual bool bPlatformRequiresSockets
	{
		get
		{
			return bPlatformUsesLibCurl;
		}
	}

	// Returns whether or not this platform needs OpenSSL in combination with whatever HTTP library it is using.
	protected virtual bool bPlatformRequiresOpenSSL
	{
		get
		{
			// WinHttp does not need OpenSSL.
			if (bPlatformUsesWinHttp)
			{
				return false;
			}
			// libCurl is compiled to rely on OpenSSL
			if (bPlatformUsesLibCurl)
			{
				return true;
			}
			return false;
		}
	}

	public ElectraHTTPStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("ELECTRA_HTTPSTREAM_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"ElectraHTTPStream/Private",
			});

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ElectraBase",
				"SSL",
			});

		// When Curl is used we need to make sure there is no conflict with its use in the engine's HTTP module
		// as far as global initialization is concerned.
		if (bPlatformUsesLibCurl || bPlatformUsesXCurl)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"HTTP",
				});
		}

		if (bPlatformRequiresSockets)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Sockets",
				}
			);
		}

		if (bPlatformUsesWinHttp)
		{
			PublicDefinitions.Add("ELECTRA_HTTPSTREAM_WINHTTP=1");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "WinHttp");
		}
		else
		{
			PublicDefinitions.Add("ELECTRA_HTTPSTREAM_WINHTTP=0");
		}

		if (bPlatformUsesLibCurl || bPlatformUsesXCurl)
		{
			if (bPlatformUsesLibCurl)
			{
				PublicDefinitions.Add("ELECTRA_HTTPSTREAM_LIBCURL=1");
				PublicDefinitions.Add("ELECTRA_HTTPSTREAM_XCURL=0");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
			}
			else
			{
				PublicDefinitions.Add("ELECTRA_HTTPSTREAM_LIBCURL=1");
				PublicDefinitions.Add("ELECTRA_HTTPSTREAM_XCURL=1");
			}
		}
		else
		{
			PublicDefinitions.Add("ELECTRA_HTTPSTREAM_LIBCURL=0");
			PublicDefinitions.Add("ELECTRA_HTTPSTREAM_XCURL=0");
		}

		PublicDefinitions.Add("ELECTRA_HTTPSTREAM_REQUIRES_SECURE_CONNECTIONS=" + (bPlatformRequiresSecureConnections ? "1" : "0"));

		if (bPlatformRequiresOpenSSL)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}

		// Apple
		if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS || Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDefinitions.Add("ELECTRA_HTTPSTREAM_APPLE=1");
			PublicFrameworks.Add("Security");
		}

	}
}
