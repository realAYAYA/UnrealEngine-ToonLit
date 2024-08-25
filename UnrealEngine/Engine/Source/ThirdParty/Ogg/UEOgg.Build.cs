// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UEOgg : ModuleRules
{
    protected virtual string OggVersion { get { return "libogg-1.2.2"; } }
    // no longer needed, remove when subclasses remove overrides
	protected virtual string IncRootDirectory { get { return ""; } }
	protected virtual string LibRootDirectory { get { return ""; } }

	protected virtual string OggIncPath { get { return Path.Combine(ModuleDirectory, OggVersion, "include"); } }
	protected virtual string OggLibPath { get { return Path.Combine(PlatformModuleDirectory, OggVersion, "lib"); } }

    public UEOgg(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		PublicSystemIncludePaths.Add(OggIncPath);

		string LibDir;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string BinDir = "$(EngineDir)/Binaries/ThirdParty/Ogg/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			LibDir = Path.Combine(OggLibPath, "Win64", "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());

			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
			{
				LibDir = Path.Combine(LibDir, Target.Architecture.WindowsLibDir);
				BinDir = BinDir + "/" + Target.Architecture.WindowsLibDir;
			}

			PublicDelayLoadDLLs.Add("libogg_64.dll");
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libogg_64.lib"));
			RuntimeDependencies.Add(BinDir + "/libogg_64.dll");
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
			PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, "Unix", Target.Architecture.LinuxName, "libogg" + fPIC + ".a"));
		}
        else if (Target.Platform == UnrealTargetPlatform.TVOS)
        {
            PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, "tvos", "libogg.a"));
        }
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS))
        {
			string LibName = (Target.Architecture == UnrealArch.IOSSimulator) ? "libogg.sim.a" : "libogg.a";
            PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, PlatformSubdirectoryName, LibName));        
        }
    }
}
