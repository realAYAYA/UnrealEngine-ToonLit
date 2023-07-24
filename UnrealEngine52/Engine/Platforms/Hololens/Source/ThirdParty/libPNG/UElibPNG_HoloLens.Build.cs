// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class UElibPNG_HoloLens : UElibPNG
	{
		public UElibPNG_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			string LibDir;

			string PlatformSubpath = Target.Platform.ToString();
			LibDir = Path.Combine(LibPNGPath, PlatformSubpath, "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
			{
				LibDir = Path.Combine(LibDir, Target.Architecture.WindowsName);
			}

			string LibFileName = "libpng";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				LibFileName += "d";
			}
			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64 || Target.WindowsPlatform.Architecture == UnrealArch.X64)
			{
				LibFileName += "_64";
			}
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, LibFileName + ".lib"));

		}
	}
}
