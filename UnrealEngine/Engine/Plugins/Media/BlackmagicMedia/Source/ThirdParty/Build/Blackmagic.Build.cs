// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Blackmagic : ModuleRules
{
	public Blackmagic(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		string MediaIOFrameworkBinaryPath = Path.Combine(EngineDir, "Plugins", "Media", "MediaIOFramework", "Binaries", Target.Platform.ToString());
		string GPUTextureTransferLibName = "GPUTextureTransferLib";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_PLATFORM=1");
			PublicDefinitions.Add("BLACKMAGICMEDIA_LINUX_PLATFORM=0");

			string SDKDir = ModuleDirectory;
			string LibPath = Path.Combine(ModuleDirectory, "../../../Binaries/ThirdParty/Win64");

			string LibraryName = "BlackmagicLib";

            bool bHaveDebugLib = File.Exists(Path.Combine(LibPath, "BlackmagicLibd.dll"));
            if (bHaveDebugLib && Target.Configuration == UnrealTargetConfiguration.Debug)
            {
                LibraryName = "BlackmagicLibd";
                PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_DEBUG=1");
			}
			else
			{
				PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_DEBUG=0");
			}

			PublicIncludePaths.Add(Path.Combine(SDKDir, "Include"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, LibraryName + ".lib"));

			PublicDelayLoadDLLs.Add(LibraryName + ".dll");
			RuntimeDependencies.Add(Path.Combine(LibPath, LibraryName + ".dll"));
			RuntimeDependencies.Add(Path.Combine(MediaIOFrameworkBinaryPath, GPUTextureTransferLibName + ".dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_PLATFORM=0");
			PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_DEBUG=0");
			PublicDefinitions.Add("BLACKMAGICMEDIA_LINUX_PLATFORM=1");

			string SDKDir = ModuleDirectory;
			string LibPath = Path.Combine(ModuleDirectory, "../../../Binaries/ThirdParty/Linux");

			string LibraryName = "libBlackmagicLib";

			PublicIncludePaths.Add(Path.Combine(SDKDir, "Include"));
			// uncomment this block to build using a static library
			// you will also need to add libDeckLinkAPI.so into Binaries/ThirdParty/Linux
			// PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libDeckLinkAPI.so"));
			// PublicAdditionalLibraries.Add(Path.Combine(LibPath, LibraryName + ".a"));

			// comment this block if you do not want to build using a dynamic library
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, LibraryName + ".so"));
			RuntimeDependencies.Add(Path.Combine(LibPath, LibraryName + ".so"));

		}
		else
		{
			PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_PLATFORM=0");
			PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_DEBUG=0");
			PublicDefinitions.Add("BLACKMAGICMEDIA_LINUX_PLATFORM=0");
			System.Console.WriteLine("BLACKMAGIC not supported on this platform");
		}
	}
}
