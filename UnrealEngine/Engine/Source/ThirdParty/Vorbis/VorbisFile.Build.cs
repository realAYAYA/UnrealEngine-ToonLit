// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class VorbisFile : ModuleRules
{
	protected virtual string VorbisVersion { get { return "libvorbis-1.3.2"; } }
	protected virtual string IncRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }
	protected virtual string LibRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }

	protected virtual string VorbisFileIncPath { get { return Path.Combine(IncRootDirectory, "Vorbis", VorbisVersion, "include"); } }
	protected virtual string VorbisFileLibPath { get { return Path.Combine(LibRootDirectory, "Vorbis", VorbisVersion, "lib"); } }

	public VorbisFile(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(VorbisFileIncPath);
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "win64", "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), "libvorbisfile_64.lib"));
			PublicDelayLoadDLLs.Add("libvorbisfile_64.dll");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Vorbis/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libvorbisfile_64.dll");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "Android", "ARM64", "libvorbisfile.a"));
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "Android", "x64", "libvorbisfile.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "Unix", Target.Architecture, "libvorbisfile.a"));
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "Unix", Target.Architecture, "libvorbisenc.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "IOS", "libvorbis.a"));
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "IOS", "libvorbisfile.a"));
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "IOS", "libvorbisenc.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "TVOS", "libvorbis.a"));
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "TVOS", "libvorbisfile.a"));
			PublicAdditionalLibraries.Add(Path.Combine(VorbisFileLibPath, "TVOS", "libvorbisenc.a"));
		}
	}
}
