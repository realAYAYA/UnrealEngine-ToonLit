// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class DirectX : ModuleRules
{
	public static string GetDir(ReadOnlyTargetRules Target)
	{
		return Target.UEThirdPartySourceDirectory + "Windows/DirectX";
	}

	public static string GetIncludeDir(ReadOnlyTargetRules Target)
	{
		return GetDir(Target) + "/include";
	}

	public static string GetLibDir(ReadOnlyTargetRules Target)
	{
		return Path.Combine(GetDir(Target), "Lib", Target.Architecture.WindowsName) + "/";
	}

	public static string GetDllDir(ReadOnlyTargetRules Target)
	{
		return Path.Combine(Target.RelativeEnginePath, "Binaries/ThirdParty/Windows/DirectX", Target.Architecture.WindowsName) + "/";
	}

	public DirectX(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
	}
}

