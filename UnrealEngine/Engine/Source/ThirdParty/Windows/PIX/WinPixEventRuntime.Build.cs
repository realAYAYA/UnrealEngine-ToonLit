// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WinPixEventRuntime : ModuleRules
{
	public WinPixEventRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64 || Target.WindowsPlatform.Architecture == WindowsArchitecture.x64)
		{
			string WinPixDir = Path.Combine(Target.UEThirdPartySourceDirectory, "Windows/PIX");

			PublicSystemIncludePaths.Add(Path.Combine( WinPixDir, "include") );

			string WinPixBaseName = "WinPixEventRuntime";
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64)
            {
				WinPixBaseName += "_UAP";
			}
			string WinPixDll = WinPixBaseName + ".dll";
			string WinPixLib = WinPixBaseName + ".lib";

			PublicDefinitions.Add("WITH_PIX_EVENT_RUNTIME=1");
            PublicDelayLoadDLLs.Add(WinPixDll);
            PublicAdditionalLibraries.Add( Path.Combine( WinPixDir, "Lib/" + Target.WindowsPlatform.Architecture.ToString() + "/" + WinPixLib) );
            RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Windows/WinPixEventRuntime/" + Target.WindowsPlatform.Architecture.ToString() + "/" + WinPixDll);

			// see pixeventscommon.h - MSVC has no support for __has_feature(address_sanitizer) so need to define this manually
			if (Target.WindowsPlatform.Compiler != WindowsCompiler.Clang && Target.WindowsPlatform.bEnableAddressSanitizer)
			{
				PublicDefinitions.Add("PIX_ENABLE_BLOCK_ARGUMENT_COPY=0");
			}
        }
	}
}

