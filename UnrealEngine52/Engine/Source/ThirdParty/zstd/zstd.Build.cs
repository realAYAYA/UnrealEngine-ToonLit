// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class zstd : ModuleRules
{
	public zstd(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (!IsVcPackageSupported)
		{
			return;
		}

		AddVcPackage("zstd", true, "zstd");
	}
}
