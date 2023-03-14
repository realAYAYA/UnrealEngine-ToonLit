// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UEOgg_HoloLens : UEOgg
	{
		public UEOgg_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			string LibDir;

			string LibFileName = "libogg";
			string PlatformSubpath = Target.Platform.ToString();
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64 || Target.WindowsPlatform.Architecture == WindowsArchitecture.x64)
			{
				LibFileName += "_64";
			}

			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64)
			{
				LibDir = System.String.Format("{0}/{1}/VS{2}/{3}/", OggLibPath, PlatformSubpath, Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), Target.WindowsPlatform.GetArchitectureSubpath());
				RuntimeDependencies.Add(
					System.String.Format("$(EngineDir)/Binaries/ThirdParty/Ogg/{0}/VS{1}/{2}/{3}.dll",
						Target.Platform,
						Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
						Target.WindowsPlatform.GetArchitectureSubpath(),
						LibFileName));
			}
			else
			{
				LibDir = System.String.Format("{0}/{1}/VS{2}/", OggLibPath, PlatformSubpath, Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
				RuntimeDependencies.Add(
					System.String.Format("$(EngineDir)/Binaries/ThirdParty/Ogg/{0}/VS{1}/{2}.dll",
						Target.Platform,
						Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
						LibFileName));
			}

			PublicAdditionalLibraries.Add(LibDir + LibFileName + ".lib");
			PublicDelayLoadDLLs.Add(LibFileName + ".dll");

		}
	}
}
