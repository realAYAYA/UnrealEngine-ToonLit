// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.IO;
using System.Collections.Generic;

namespace UnrealBuildTool.Rules
{
	public class UnrealUSDWrapper : ModuleRules
	{
		public UnrealUSDWrapper(ReadOnlyTargetRules Target) : base(Target)
		{
			bUseRTTI = true;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Boost",
					"CinematicCamera",
					"Core",
					"CoreUObject",
					"Engine",
					"IntelTBB",
					"MaterialX", // Needed for the standard data libraries
					"Projects", // For plugin manager within UnrealUSDWrapper.cpp
					"USDClasses"
				}
			);

			// Temporarily disabled runtime USD support until Mac and Linux dynamic linking issues are resolved
			if (EnableUsdSdk(Target) && (Target.Type == TargetType.Editor || Target.Platform == UnrealTargetPlatform.Win64))
			{
				PublicDependencyModuleNames.Add("Python3");

				PublicDefinitions.Add("USE_USD_SDK=1");

				var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
				var PythonSourceTPSDir = Path.Combine(EngineDir, "Source", "ThirdParty", "Python3", Target.Platform.ToString());
				var PythonBinaryTPSDir = Path.Combine(EngineDir, "Binaries", "ThirdParty", "Python3", Target.Platform.ToString());
				string IntelTBBBinaries = Path.Combine(Target.UEThirdPartyBinariesDirectory, "Intel", "TBB", Target.Platform.ToString());
				string IntelTBBIncludes = Path.Combine(Target.UEThirdPartySourceDirectory, "Intel", "TBB", "IntelTBB-2019u8", "include");
				string USDLibsDir = Path.Combine(ModuleDirectory, "..", "ThirdParty", "USD", "lib");

				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicDefinitions.Add("USD_USES_SYSTEM_MALLOC=1");

					// TBB
					// Don't need to handle it for Windows or Mac as IntelTBB.Build.cs already does it

					// Python3
					PublicIncludePaths.Add(Path.Combine(PythonSourceTPSDir, "include"));
					PublicSystemLibraryPaths.Add(Path.Combine(PythonSourceTPSDir, "libs"));
					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "python39.dll"), Path.Combine(PythonBinaryTPSDir, "python39.dll"));

					// USD
					PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "USD", "include"));
					PublicSystemLibraryPaths.Add(USDLibsDir);
					foreach (string UsdLib in Directory.EnumerateFiles(USDLibsDir, "*.lib", SearchOption.AllDirectories))
					{

						PublicAdditionalLibraries.Add(UsdLib);
					}
					var USDBinDir = Path.Combine(ModuleDirectory, "..", "ThirdParty", "USD", "bin");
					foreach (string UsdDll in Directory.EnumerateFiles(USDBinDir, "*.dll", SearchOption.AllDirectories))
					{
						// We can't delay-load the USD dlls as they contain data and vtables: They need to be next to the executable and implicitly linked
						RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", Path.GetFileName(UsdDll)), UsdDll);
					}
				}
				else if (Target.Platform == UnrealTargetPlatform.Linux)
				{
					PublicDefinitions.Add("USD_USES_SYSTEM_MALLOC=0"); // USD uses tbb malloc on Linux

					// TBB
					PublicSystemIncludePaths.Add(IntelTBBIncludes);
					PrivateRuntimeLibraryPaths.Add(IntelTBBBinaries);
					PublicAdditionalLibraries.Add(Path.Combine(IntelTBBBinaries, "libtbb.so"));
					RuntimeDependencies.Add(Path.Combine(IntelTBBBinaries, "libtbb.so"));
					RuntimeDependencies.Add(Path.Combine(IntelTBBBinaries, "libtbb.so.2"));

					// Python3
					PublicIncludePaths.Add(Path.Combine(PythonSourceTPSDir, "include"));
					PublicSystemLibraryPaths.Add(Path.Combine(PythonBinaryTPSDir, "lib"));
					PrivateRuntimeLibraryPaths.Add(Path.Combine(PythonBinaryTPSDir, "bin"));
					RuntimeDependencies.Add(Path.Combine(PythonBinaryTPSDir, "bin", "python3.9"));

					// USD
					PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "USD", "include"));
					var USDBinDir = Path.Combine(ModuleDirectory, "..", "ThirdParty", "Linux", "bin", Target.Architecture);
					PrivateRuntimeLibraryPaths.Add(USDBinDir);
					foreach (string LibPath in Directory.EnumerateFiles(USDBinDir, "*.so", SearchOption.AllDirectories))
					{
						PublicAdditionalLibraries.Add(LibPath);
						RuntimeDependencies.Add(LibPath);
					}
					// Redirect plugInfo.json to Plugin/Binaries for the editor, but leave them pointing at the executable folder otherwise
					// (which is the default when USE_LIBRARIES_FROM_PLUGIN_FOLDER is not defined)
					if (Target.Type == TargetType.Editor && (Target.BuildEnvironment != TargetBuildEnvironment.Unique))
					{
						PublicDefinitions.Add("USE_LIBRARIES_FROM_PLUGIN_FOLDER=1");
					}
				}
				else if (Target.Platform == UnrealTargetPlatform.Mac)
				{
					PublicDefinitions.Add("USD_USES_SYSTEM_MALLOC=0");

					List<string> RuntimeModulePaths = new List<string>();

					// TBB
					// Don't need to handle it for Windows or Mac as IntelTBB.Build.cs already does it

					// Python3
					PublicIncludePaths.Add(Path.Combine(PythonSourceTPSDir, "include"));
					PublicSystemLibraryPaths.Add(Path.Combine(PythonBinaryTPSDir, "lib"));
					PrivateRuntimeLibraryPaths.Add(Path.Combine(PythonBinaryTPSDir, "bin"));
					RuntimeModulePaths.Add(Path.Combine(PythonBinaryTPSDir, "lib", "libpython3.9.dylib"));

					// USD
					PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "USD", "include"));
					var USDBinDir = Path.Combine(ModuleDirectory, "..", "ThirdParty", "Mac", "bin");
					foreach (string LibPath in Directory.EnumerateFiles(USDBinDir, "*.dylib", SearchOption.AllDirectories))
					{
						RuntimeModulePaths.Add(LibPath);
					}

					foreach (string RuntimeModulePath in RuntimeModulePaths)
					{
						if (!File.Exists(RuntimeModulePath))
						{
							string Err = string.Format("USD SDK module '{0}' not found.", RuntimeModulePath);
							System.Console.WriteLine(Err);
							throw new BuildException(Err);
						}

						PublicDelayLoadDLLs.Add(RuntimeModulePath);
						RuntimeDependencies.Add(RuntimeModulePath);
					}
					// Redirect plugInfo.json to Plugin/Binaries for the editor, but leave them pointing at the executable folder otherwise
					// (which is the default when USE_LIBRARIES_FROM_PLUGIN_FOLDER is not defined)
					if (Target.Type == TargetType.Editor && (Target.BuildEnvironment != TargetBuildEnvironment.Unique))
					{
						PublicDefinitions.Add("USE_LIBRARIES_FROM_PLUGIN_FOLDER=1");
					}
				}

				// Move UsdResources to <Target>/Binaries/ThirdParty/UsdResources. UnrealUSDWrapper.cpp will expect them to be there
				RuntimeDependencies.Add(
					Path.Combine("$(TargetOutputDir)", "..", "ThirdParty", "USD", "UsdResources", Target.Platform.ToString()),
					Path.Combine("$(PluginDir)", "Resources", "UsdResources", Target.Platform.ToString(), "...")
				);
			}
			else
			{
				PublicDefinitions.Add("USE_USD_SDK=0");
				PublicDefinitions.Add("USD_USES_SYSTEM_MALLOC=0");
			}
		}

		bool EnableUsdSdk(ReadOnlyTargetRules Target)
		{
			// USD SDK has been built against Python 3 and won't launch if the editor is using Python 2

			bool bEnableUsdSdk = (
				Target.WindowsPlatform.Compiler != WindowsCompiler.Clang &&
				Target.StaticAnalyzer == StaticAnalyzer.None
			);

			// Don't enable USD when running the include tool because it has issues parsing Boost headers
			if (Target.GlobalDefinitions.Contains("UE_INCLUDE_TOOL=1"))
			{
				bEnableUsdSdk = false;
			}

			// If you want to use USD in a monolithic target, you'll have to use the ANSI allocator.
			// USD always uses the ANSI C allocators directly. In a DLL UE build (so not monolithic) we can just override the operators new and delete
			// on each module with versions that use either the ANSI (so USD-compatible) allocators or the UE allocators (ModuleBoilerplate.h) when appropriate.
			// In a monolithic build we can't do that, as the primary game module will already define overrides for operator new and delete with
			// the standard UE allocators: Since we can only have one operator new/delete override on the entire monolithic executable, we can't define our own overrides.
			// Additionally, the ANSI allocator does not work properly with FMallocPoisonProxy. Consequently, FMallocPoisonProxy has to be disabled.
			// The only way around it is by forcing the ansi allocator and disabling FMallocPoisonProxy in your project's target file
			// (YourProject/Source/YourProject.Target.cs) file like this:
			//
			//		public class YourProject : TargetRules
			//		{
			//			public YourProject(TargetInfo Target) : base(Target)
			//			{
			//				...
			//				GlobalDefinitions.Add("FORCE_ANSI_ALLOCATOR=1");
			//				GlobalDefinitions.Add("UE_USE_MALLOC_FILL_BYTES=0");
			//				...
			//			}
			//		}
			//
			// This will force the entire built executable to use the ANSI C allocators for everything (by disabling the UE overrides in ModuleBoilerplate.h) while
			// FMallocPoisonProxy is disabled, and so UE and USD allocations will be compatible.
			// Note that by that point everything will be using the USD-compatible ANSI allocators anyway, so our overrides in USDMemory.h are also disabled, as they're unnecessary.
			// Also note that we're forced to use dynamic linking for monolithic targets mainly because static linking the USD libraries disables support for user USD plugins, and secondly
			// because those static libraries would need to be linked with the --whole-archive argument, and there is currently no standard way of doing that in UE.
			if (bEnableUsdSdk && Target.LinkType == TargetLinkType.Monolithic && !Target.GlobalDefinitions.Contains("FORCE_ANSI_ALLOCATOR=1") && !Target.GlobalDefinitions.Contains("UE_USE_MALLOC_FILL_BYTES=0"))
			{
				PublicDefinitions.Add("USD_FORCE_DISABLED=1");
				bEnableUsdSdk = false;
			}
			else
			{
				PublicDefinitions.Add("USD_FORCE_DISABLED=0");
			}

			return bEnableUsdSdk;
		}
	}
}
