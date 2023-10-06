// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibreSSL : ModuleRules
{
	public LibreSSL(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (!IsVcPackageSupported)
		{
			return;
		}

		AddVcPackage("libressl", true, new string [] { "crypto", "ssl", "tls" });
	}
}
