// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class Vorbis : ModuleRules
{
	protected virtual string VorbisVersion { get { return "libvorbis-1.3.2"; } }
	// no longer needed, remove when sunclasses remove overrides
	protected virtual string IncRootDirectory { get { return ModuleDirectory; } }
	protected virtual string LibRootDirectory { get { return PlatformModuleDirectory; } }

	protected virtual string VorbisIncPath { get { return Path.Combine(ModuleDirectory, VorbisVersion, "include"); } }
	protected virtual string VorbisLibPath { get { return Path.Combine(PlatformModuleDirectory, VorbisVersion, "lib"); } }

	public Vorbis(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(VorbisIncPath);
		PublicDefinitions.Add("WITH_OGGVORBIS=1");

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicAdditionalLibraries.Add(Path.Combine(VorbisLibPath, "win64", "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), "libvorbis_64.lib"));
			PublicDelayLoadDLLs.Add("libvorbis_64.dll");

			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Vorbis/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libvorbis_64.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string DylibPath = Target.UEThirdPartyBinariesDirectory + "Vorbis/Mac/libvorbis.dylib";
			PublicDelayLoadDLLs.Add(DylibPath);
			RuntimeDependencies.Add(DylibPath);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			// toolchain will filter
			string[] Architectures = new string[] {
				"ARM64",
				"x64",
			};

			foreach(string Architecture in Architectures)
			{
				PublicAdditionalLibraries.Add(Path.Combine(VorbisLibPath, "Android", Architecture, "libvorbis.a"));
			}
        }
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS))
		{
			string LibExt = (Target.Architecture == UnrealArch.IOSSimulator) ? ".sim.a" : ".a";
			PublicAdditionalLibraries.Add(Path.Combine(VorbisLibPath, PlatformSubdirectoryName, "libvorbis" + LibExt));
			PublicAdditionalLibraries.Add(Path.Combine(VorbisLibPath, PlatformSubdirectoryName, "libvorbisenc" + LibExt));
			PublicAdditionalLibraries.Add(Path.Combine(VorbisLibPath, PlatformSubdirectoryName, "libvorbisfile" + LibExt));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(VorbisLibPath, "Unix", Target.Architecture.LinuxName, "libvorbis.a"));
		}
	}
}
