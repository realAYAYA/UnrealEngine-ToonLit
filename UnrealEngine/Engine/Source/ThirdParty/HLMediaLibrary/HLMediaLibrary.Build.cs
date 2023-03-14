// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class HLMediaLibrary : ModuleRules
{
	public const string LibraryName = "HLMediaLibrary";
	
	protected virtual string Platform { get => "Windows"; }
	protected virtual bool bSupportedPlatform { get => Target.Platform == UnrealTargetPlatform.Win64; }

	public HLMediaLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string BaseDir = Target.UEThirdPartySourceDirectory + LibraryName;

		string BinariesDir = Target.UEThirdPartyBinariesDirectory + LibraryName;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		string Config = Target.Configuration == UnrealTargetConfiguration.Debug ? "Debug" : "Release";
		string Arch = Target.WindowsPlatform.GetArchitectureSubpath();
		string SubPath = Path.Combine(Platform, Config, Arch);

		string LibPath = Path.Combine(BaseDir, "lib", SubPath);
		string BinariesPath = Path.Combine(BinariesDir, SubPath);
		string dll = String.Format("{0}.dll", LibraryName);

		// windows desktop x64 target
		if (bSupportedPlatform)
		{
			// Add the import library
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, String.Format("{0}.lib", LibraryName)));

			// Delay-load the DLL, so we can load it from the right place first
			PublicDelayLoadDLLs.Add(dll);

			// Ensure that the DLL is staged along with the executable
			RuntimeDependencies.Add(Path.Combine(BinariesPath, dll));
		}
	}
}
