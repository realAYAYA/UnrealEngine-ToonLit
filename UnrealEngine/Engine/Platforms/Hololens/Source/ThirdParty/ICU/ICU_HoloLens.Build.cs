// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ICU_HoloLens : ICU
	{
		public ICU_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			string VSVersionFolderName = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			string PlatformICULibPath = Path.Combine(ICULibPath, VSVersionFolderName, Target.WindowsPlatform.GetArchitectureSubpath(), "lib");

			string[] LibraryNameStems =
			{
				"dt",   // Data
				"uc",   // Unicode Common
				"in",   // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = UseDebugLibs ? "d" : string.Empty;

			// Library Paths
			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = "sicu" + Stem + LibraryNamePostfix + "." + "lib";
				PublicAdditionalLibraries.Add(Path.Combine(PlatformICULibPath, LibraryName));
			}

		}
	}
}
