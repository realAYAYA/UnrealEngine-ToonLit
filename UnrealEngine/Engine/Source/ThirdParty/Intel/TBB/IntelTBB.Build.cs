// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class IntelTBB : ModuleRules
{
	protected virtual bool bUseWinTBB { get => Target.Platform.IsInGroup(UnrealPlatformGroup.Windows); }
	protected virtual bool bIncludeDLLRuntimeDependencies { get => true; }

	public IntelTBB(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		string IntelTBBPath = Path.Combine(Target.UEThirdPartySourceDirectory, "Intel/TBB/IntelTBB-2019u8");
		string IntelTBBIncludePath = Path.Combine(IntelTBBPath, "include");
		string IntelTBBLibPath = Path.Combine(IntelTBBPath, "lib");

		PublicSystemIncludePaths.Add(IntelTBBIncludePath);		

		if (bUseWinTBB)
		{
			string PlatformSubPath = "Win64";
			string IntelTBBBinaries = Path.Combine(Target.UEThirdPartyBinariesDirectory, "Intel", "TBB", "Win64");
			
			string LibDirTBB = Path.Combine(IntelTBBLibPath, PlatformSubPath, "vc14");
			string LibDirTBBMalloc = LibDirTBB;
			
			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
			{
				LibDirTBBMalloc = Path.Combine(LibDirTBBMalloc, Target.Architecture.WindowsLibDir);
			}

			PublicSystemLibraryPaths.Add(LibDirTBB);
			PublicSystemLibraryPaths.Add(LibDirTBBMalloc);

			// Disable the #pragma comment(lib, ...) used by default in TBB & MallocTBB...
			// We want to explicitly include the libraries.
			PublicDefinitions.Add("__TBBMALLOC_NO_IMPLICIT_LINKAGE=1");
			PublicDefinitions.Add("__TBB_NO_IMPLICIT_LINKAGE=1");

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicDefinitions.Add("TBB_USE_DEBUG=1");
				PublicAdditionalLibraries.Add(Path.Combine(LibDirTBB, "tbb_debug.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibDirTBBMalloc, "tbbmalloc_debug.lib"));
				if (bIncludeDLLRuntimeDependencies)
				{
					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "tbb_debug.dll"), Path.Combine(LibDirTBB, "tbb_debug.dll"));
					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "tbb_debug.pdb"), Path.Combine(LibDirTBB, "tbb_debug.pdb"), StagedFileType.DebugNonUFS);

					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "tbbmalloc_debug.dll"), Path.Combine(LibDirTBB, "tbbmalloc_debug.dll"));
					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "tbbmalloc_debug.pdb"), Path.Combine(LibDirTBB, "tbbmalloc_debug.pdb"), StagedFileType.DebugNonUFS);
				}
			}
			else
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibDirTBB, "tbb.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibDirTBBMalloc, "tbbmalloc.lib"));
				if (bIncludeDLLRuntimeDependencies)
				{
					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "tbb.dll"), Path.Combine(IntelTBBBinaries, "tbb.dll"));
					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "tbb.pdb"), Path.Combine(IntelTBBBinaries, "tbb.pdb"), StagedFileType.DebugNonUFS);

					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "tbbmalloc.dll"), Path.Combine(IntelTBBBinaries, "tbbmalloc.dll"));
					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "tbbmalloc.pdb"), Path.Combine(IntelTBBBinaries, "tbbmalloc.pdb"), StagedFileType.DebugNonUFS);
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDir = Path.Combine(IntelTBBLibPath, "Mac");
			string IntelTBBBinaries = Path.Combine(Target.UEThirdPartyBinariesDirectory, "Intel", "TBB", "Mac");
			
			PublicAdditionalLibraries.Add(Path.Combine(IntelTBBBinaries, "libtbb.dylib"));
			PublicAdditionalLibraries.Add(Path.Combine(IntelTBBBinaries, "libtbbmalloc.dylib"));

			RuntimeDependencies.Add(Path.Combine(IntelTBBBinaries, "libtbb.dylib"));
			RuntimeDependencies.Add(Path.Combine(IntelTBBBinaries, "libtbbmalloc.dylib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			bUseRTTI = false;
			bEnableExceptions = false;
			PublicDefinitions.Add("TBB_USE_EXCEPTIONS=0");

			string LibDir = Path.Combine(IntelTBBLibPath, "Linux");
			string IntelTBBBinaries = Path.Combine(Target.UEThirdPartyBinariesDirectory, "Intel", "TBB", "Linux");
			
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicDefinitions.Add("TBB_USE_DEBUG=1");
				PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libtbb_debug.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libtbbmalloc_debug.a"));

			}
			else
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libtbb.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libtbbmalloc.a"));
			}
		}
	}
}
