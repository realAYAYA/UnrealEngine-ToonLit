// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System.Collections.Generic;

public class CEF3 : ModuleRules
{
	public CEF3(ReadOnlyTargetRules Target) : base(Target)
	{
		/** Mark the current version of the library */
		string CEFVersion = "";
		string CEFPlatform = "";

		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			CEFVersion = "90.6.7+g19ba721+chromium-90.0.4430.212";
			CEFPlatform = "windows64";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			CEFVersion = "90.6.7+g19ba721+chromium-90.0.4430.212";
			// the wrapper.la that is in macosarm64 is universal, so we always point to this one for the lib
			CEFPlatform = "macosarm64";
		}
		else if(Target.Platform == UnrealTargetPlatform.Linux)
		{
			CEFVersion = "93.0.0+gf38ce34+chromium-93.0.4577.82";
			CEFPlatform = "linux64_ozone";
		}

		if (CEFPlatform.Length > 0 && CEFVersion.Length > 0 && Target.bCompileCEF3)
		{
			string PlatformPath = Path.Combine(Target.UEThirdPartySourceDirectory, "CEF3", "cef_binary_" + CEFVersion + "_" + CEFPlatform);

			PublicSystemIncludePaths.Add(PlatformPath);

			string LibraryPath = Path.Combine(PlatformPath, "Release");
            string RuntimePath = Path.Combine(Target.UEThirdPartyBinariesDirectory , "CEF3", Target.Platform.ToString());

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
                PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "libcef.lib"));

                // There are different versions of the C++ wrapper lib depending on the version of VS we're using
                string VSVersionFolderName = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
                string WrapperLibraryPath = Path.Combine(PlatformPath, VSVersionFolderName, "libcef_dll_wrapper");

                if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
                {
                    WrapperLibraryPath += "/Debug";
                }
                else
                {
                    WrapperLibraryPath += "/Release";
                }

                PublicAdditionalLibraries.Add(Path.Combine(WrapperLibraryPath, "libcef_dll_wrapper.lib"));

				List<string> Dlls = new List<string>();

				Dlls.Add("chrome_elf.dll");
				Dlls.Add("d3dcompiler_47.dll");
				Dlls.Add("libcef.dll");
				Dlls.Add("libEGL.dll");
				Dlls.Add("libGLESv2.dll");

				PublicDelayLoadDLLs.AddRange(Dlls);

                // Add the runtime dlls to the build receipt
                foreach (string Dll in Dlls)
                {
                    RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/" + Dll);
                }
                // We also need the icu translations table required by CEF
                RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/icudtl.dat");

				// Add the V8 binary data files as well
                RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/v8_context_snapshot.bin");
                RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/snapshot_blob.bin");

                // And the entire Resources folder. Enumerate the entire directory instead of mentioning each file manually here.
                foreach (string FileName in Directory.EnumerateFiles(Path.Combine(RuntimePath, "Resources"), "*", SearchOption.AllDirectories))
                {
                    string DependencyName = FileName.Substring(Target.UEThirdPartyBinariesDirectory.Length).Replace('\\', '/');
                    RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/" + DependencyName);
                }
            }
			// TODO: Ensure these are filled out correctly when adding other platforms
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				string WrapperPath = LibraryPath + "/libcef_dll_wrapper.a";
				
				// we have separate frameworks for x86 and arm64 because they are so large, some single-architecture builds don't want
				// to pay the cost of both of them (ideally we would make this universal, and then remove the unused architectures during staging)
				string FrameworkPathX86 = Target.UEThirdPartyBinariesDirectory + "CEF3/Mac/Chromium Embedded Framework x86.framework";
				string FrameworkPathArm64 = Target.UEThirdPartyBinariesDirectory + "CEF3/Mac/Chromium Embedded Framework arm64.framework";

				PublicAdditionalLibraries.Add(WrapperPath);

				if(Directory.Exists(LibraryPath + "/locale"))
				{
					var LocaleFolders = Directory.GetFileSystemEntries(LibraryPath + "/locale", "*.lproj");
					foreach (var FolderName in LocaleFolders)
					{
						AdditionalBundleResources.Add(new BundleResource(FolderName, bShouldLog: false));
					}
				}

				if (Target.Architecture.ToLower().Contains("arm64"))
				{
					// Add contents of framework directory as runtime dependencies
					foreach (string FilePath in Directory.EnumerateFiles(FrameworkPathArm64, "*", SearchOption.AllDirectories))
					{
						RuntimeDependencies.Add(FilePath);
					}
				}
				if (Target.Architecture.ToLower().Contains("x86"))
				{
					// Add contents of framework directory as runtime dependencies
					foreach (string FilePath in Directory.EnumerateFiles(FrameworkPathX86, "*", SearchOption.AllDirectories))
					{
						RuntimeDependencies.Add(FilePath);
					}
				}

			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				// link against runtime library since this produces correct RPATH
				string RuntimeLibCEFPath = Path.Combine(RuntimePath, "libcef.so");
				PublicAdditionalLibraries.Add(RuntimeLibCEFPath);

				string Configuration = "build_release";
				string WrapperLibraryPath =  Path.Combine(PlatformPath, Configuration, "libcef_dll");

				PublicAdditionalLibraries.Add(Path.Combine(WrapperLibraryPath, "libcef_dll_wrapper.a"));

				PrivateRuntimeLibraryPaths.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString());

				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/libcef.so");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/libEGL.so");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/libGLESv2.so");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/icudtl.dat");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/v8_context_snapshot.bin");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/snapshot_blob.bin");

				// Add the Resources and Swiftshader folders, enumerating the directory contents programmatically rather than listing each file manually here
				string[] AdditionalDirs = new string[]{"Resources", "swiftshader"};
				foreach (string DirName in AdditionalDirs)
				{
					foreach (string FileName in Directory.EnumerateFiles(Path.Combine(RuntimePath, DirName), "*", SearchOption.AllDirectories))
					{
						string DependencyName = FileName.Substring(Target.UEThirdPartyBinariesDirectory.Length).Replace('\\', '/');
						RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/" + DependencyName);
					}
				}
			}
		}
	}
}
