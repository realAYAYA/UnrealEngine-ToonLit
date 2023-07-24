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
			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64 || Target.WindowsPlatform.Architecture == UnrealArch.X64)
			{
				LibFileName += "_64";
			}

			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
			{
				LibDir = System.String.Format("{0}/{1}/VS{2}/{3}/", OggLibPath, PlatformSubpath, Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), Target.Architecture.WindowsName);
				RuntimeDependencies.Add(
					System.String.Format("$(EngineDir)/Binaries/ThirdParty/Ogg/{0}/VS{1}/{2}/{3}.dll",
						Target.Platform,
						Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
						Target.Architecture.WindowsName,
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
