// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UEOgg : ModuleRules
{
    protected virtual string OggVersion { get { return "libogg-1.2.2"; } }
	protected virtual string IncRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }
	protected virtual string LibRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }

	protected virtual string OggIncPath { get { return Path.Combine(IncRootDirectory, "Ogg", OggVersion, "include"); } }
	protected virtual string OggLibPath { get { return Path.Combine(LibRootDirectory, "Ogg", OggVersion, "lib"); } }

    public UEOgg(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		PublicSystemIncludePaths.Add(OggIncPath);

		string LibDir;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibDir = Path.Combine(OggLibPath, "Win64", "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());

			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libogg_64.lib"));

			PublicDelayLoadDLLs.Add("libogg_64.dll");

			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Ogg/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libogg_64.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string DylibPath = Path.Combine(Target.UEThirdPartyBinariesDirectory, "Ogg", "Mac", "libogg.dylib");
			PublicDelayLoadDLLs.Add(DylibPath);
			RuntimeDependencies.Add(DylibPath);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, "Android", "ARM64", "libogg.a"));
			PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, "Android", "x64", "libogg.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string fPIC = (Target.LinkType == TargetLinkType.Monolithic)
				? ""
				: "_fPIC";
			PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, "Unix", Target.Architecture, "libogg" + fPIC + ".a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, "ios", "libogg.a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.TVOS)
        {
            PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, "tvos", "libogg.a"));
        }
    }
}
