// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class ICU : ModuleRules
{
	enum EICULinkType
	{
		None,
		Static,
		Dynamic
	}

	protected const string ICU53VersionString = "icu4c-53_1";
	protected const string ICU64VersionString = "icu4c-64_1";

	protected virtual string ICUVersion
	{
		get
		{
			if (Target.Platform == UnrealTargetPlatform.IOS ||
				Target.Platform == UnrealTargetPlatform.TVOS ||
				Target.Platform == UnrealTargetPlatform.Mac ||
				Target.Platform == UnrealTargetPlatform.Win64 ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Android) ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				return ICU64VersionString;
			}

			return ICU53VersionString;
		}
	}

	protected virtual string ICULibRootPath
	{
		get { return ModuleDirectory; }
	}
	protected virtual string ICUIncRootPath
	{
		get { return ModuleDirectory; }
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

	protected virtual string ICULibPath
	{
		get
		{
			switch (ICUVersion)
			{
				case ICU53VersionString: return Path.Combine(ICULibRootPath, ICUVersion, PlatformName ?? ".");
				case ICU64VersionString: return Path.Combine(ICULibRootPath, ICUVersion, "lib", PlatformName ?? ".");

				default: throw new ArgumentException("Invalid ICU version");
			}
		}
	}

	protected virtual bool UseDebugLibs
	{
		get { return Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT; }
	}

	public ICU(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Note: If you change the version of ICU used for your platform, you may also need to update the ICU data staging code inside CopyBuildToStagingDirectory.Automation.cs

		bool bNeedsDlls = false;
		
		if (ICUVersion == ICU64VersionString)
		{
			PublicDefinitions.Add("WITH_ICU_V64=1"); // TODO: Remove this once everything is using ICU 64
		}

		// Includes
		PublicSystemIncludePaths.Add(Path.Combine(ICUIncRootPath, ICUVersion, "include"));

		// Libs
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string VSVersionFolderName = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicAdditionalLibraries.Add(Path.Combine(ICULibPath, VSVersionFolderName, UseDebugLibs ? "Debug" : "Release", "icu.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ICULibPath, UseDebugLibs ? "libicud.a" : "libicu.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ICULibPath, UseDebugLibs ? "Debug" : "Release", "libicu.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			string ICULibName = "libicu.a";
			string ICULibFolder = UseDebugLibs ? "Debug" : "Release";

			// filtered out in the toolchain
			PublicAdditionalLibraries.Add(Path.Combine(ICULibPath, "ARM64", ICULibFolder, ICULibName));
			PublicAdditionalLibraries.Add(Path.Combine(ICULibPath, "x64", ICULibFolder, ICULibName));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string ICULibName = UseDebugLibs ? "libicud_fPIC.a" : "libicu_fPIC.a";
			PublicAdditionalLibraries.Add(Path.Combine(ICULibPath, Target.Architecture, ICULibName));
		}

		// DLL Definitions
		if (bNeedsDlls)
		{
			PublicDefinitions.Add("NEEDS_ICU_DLLS=1");
		}
		else
		{
			PublicDefinitions.Add("NEEDS_ICU_DLLS=0");
			PublicDefinitions.Add("U_STATIC_IMPLEMENTATION"); // Necessary for linking to ICU statically.
		}

		// Common Definitions
		PublicDefinitions.Add("U_USING_ICU_NAMESPACE=0"); // Disables a using declaration for namespace "icu".
		PublicDefinitions.Add("UNISTR_FROM_CHAR_EXPLICIT=explicit"); // Makes UnicodeString constructors for ICU character types explicit.
		PublicDefinitions.Add("UNISTR_FROM_STRING_EXPLICIT=explicit"); // Makes UnicodeString constructors for "char"/ICU string types explicit.
		PublicDefinitions.Add("UCONFIG_NO_TRANSLITERATION=1"); // Disables declarations and compilation of unused ICU transliteration functionality.
	}
}
