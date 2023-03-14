// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class Vorbis_HoloLens : Vorbis
	{
		public Vorbis_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			string PlatformSubpath = Target.Platform.ToString();
			string LibFileName = "libvorbis_64";

			string LibDir;
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64)
			{
				LibDir = System.String.Format("{0}/{1}/VS{2}/{3}/", VorbisLibPath, PlatformSubpath, Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), Target.WindowsPlatform.GetArchitectureSubpath() + "/");
				RuntimeDependencies.Add(
					System.String.Format("$(EngineDir)/Binaries/ThirdParty/Vorbis/{0}/VS{1}/{2}/{3}.dll",
						Target.Platform,
						Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
						Target.WindowsPlatform.GetArchitectureSubpath(),
						LibFileName));
			}
			else
			{
				LibDir = System.String.Format("{0}/{1}/VS{2}/", VorbisLibPath, PlatformSubpath, Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/");
				RuntimeDependencies.Add(
					System.String.Format("$(EngineDir)/Binaries/ThirdParty/Vorbis/{0}/VS{1}/{2}.dll",
						Target.Platform,
						Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
						LibFileName));
			}

			PublicAdditionalLibraries.Add(Path.Combine(LibDir, LibFileName + ".lib"));
			PublicDelayLoadDLLs.Add(LibFileName + ".dll");

		}
	}
}
