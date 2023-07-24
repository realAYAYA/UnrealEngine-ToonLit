// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WinPixEventRuntime : ModuleRules
{
	public WinPixEventRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64 || Target.WindowsPlatform.Architecture == UnrealArch.X64)
		{
			string ThirdPartyDir = Path.Combine(Target.UEThirdPartySourceDirectory, "Windows", "PIX");
			string IncludeDir = Path.Combine(ThirdPartyDir, "include");
			string LibrariesDir = Path.Combine(ThirdPartyDir, "Lib", Target.WindowsPlatform.Architecture.ToString());
			string BinariesDir = Path.Combine("$(EngineDir)", "Binaries", "ThirdParty", "Windows", "WinPixEventRuntime", Target.WindowsPlatform.Architecture.ToString());

			string BinaryBaseName = Target.WindowsPlatform.Architecture == UnrealArch.Arm64 ? "WinPixEventRuntime_UAP" : "WinPixEventRuntime";
			string WinPixDll = BinaryBaseName + ".dll";
			string WinPixLib = BinaryBaseName + ".lib";

			PublicDefinitions.Add("WITH_PIX_EVENT_RUNTIME=1");

			PublicSystemIncludePaths.Add(IncludeDir);
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesDir, WinPixLib));
			RuntimeDependencies.Add(Path.Combine(BinariesDir, WinPixDll));
			PublicDelayLoadDLLs.Add(WinPixDll);

			// see pixeventscommon.h - MSVC has no support for __has_feature(address_sanitizer) so need to define this manually
			if (Target.WindowsPlatform.Compiler != WindowsCompiler.Clang && Target.WindowsPlatform.bEnableAddressSanitizer)
			{
				PublicDefinitions.Add("PIX_ENABLE_BLOCK_ARGUMENT_COPY=0");
			}
        }
	}
}

