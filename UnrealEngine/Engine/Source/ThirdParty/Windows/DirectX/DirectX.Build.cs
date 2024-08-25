// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class DirectX : ModuleRules
{
	[Obsolete("Replace with Target.WindowsPlatform.DirectXDir")]
	public static string GetDir(ReadOnlyTargetRules Target)
	{
		return Path.Combine(Target.UEThirdPartySourceDirectory, "Windows", "DirectX");
	}

	[Obsolete("Add DirectX to PublicDependencyModuleNames")]
	public static string GetIncludeDir(ReadOnlyTargetRules Target)
	{
		return Path.Combine(GetDir(Target), "include");
	}

	[Obsolete("Replace with Target.WindowsPlatform.DirectXLibDir")]
	public static string GetLibDir(ReadOnlyTargetRules Target)
	{
		return Path.Combine(GetDir(Target), "Lib", Target.Architecture.WindowsLibDir) + "/";
	}

	[Obsolete("Replace with Target.WindowsPlatform.DirectXDllDir")]
	public static string GetDllDir(ReadOnlyTargetRules Target)
	{
		return Path.Combine(Target.RelativeEnginePath, "Binaries", "ThirdParty", "Windows", "DirectX", Target.Architecture.WindowsLibDir) + "/";
	}

	public DirectX(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
	}
}
