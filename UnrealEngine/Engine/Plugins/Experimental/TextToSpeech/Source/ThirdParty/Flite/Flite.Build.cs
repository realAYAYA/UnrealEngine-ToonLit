
// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Flite : ModuleRules
{
	protected virtual string FliteVersion
	{
		get
		{
			return "Flite-e0a3d25";
		}
	}
	protected virtual string FliteLibRootPath
    {
        get
        {
			return ModuleDirectory;
        }
    }

	protected virtual string FliteIncludePath
	{
		get 
		{ 
			return Path.Combine(ModuleDirectory, FliteVersion, "include"); 
		}
	}

	protected virtual string PlatformName
	{
		get
		{
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				return "Unix";
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
			{
				return "Android";
			}
			else
			{
				return Target.Platform.ToString();
			}
		}
	}

	protected virtual string FliteLibPath
	{
		get
		{
			// We check for null as child classes could override platform name to be null 
			return Path.Combine(FliteLibRootPath, "lib", PlatformName ?? ".");
		}
	}

	protected virtual bool bUseDebugLibs
	{
		get { return Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT; }
	}
	// Override in child platforms that require the use of Windows headers 
	protected virtual bool bRequiresWindowsHeaders
	{
		get { return Target.Platform == UnrealTargetPlatform.Win64; }
	}

	public Flite(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(FliteIncludePath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Only VS2019 supported as of now
			string VSVersion = "VS2019";
			PublicAdditionalLibraries.Add(Path.Combine(FliteLibPath, VSVersion, bUseDebugLibs ? "Debug" : "Release", "libFlite.lib"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
        {
			PublicAdditionalLibraries.Add(Path.Combine(FliteLibPath, "ARM64", bUseDebugLibs ? "Debug" : "Release", "libFlite.a"));
			PublicAdditionalLibraries.Add(Path.Combine(FliteLibPath, "x64", bUseDebugLibs ? "Debug" : "Release", "libFlite.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(FliteLibPath , Target.Architecture, bUseDebugLibs ? "Debug" : "Release", "libFlite.a"));
		}
		PublicDefinitions.Add("UE_FLITE_REQUIRES_WINDOWS_HEADERS=" + (bRequiresWindowsHeaders ? "1" : "0"));
	}
}
