// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;

namespace UnrealBuildTool.Rules
{
	public class ChaosFleshEngine : ModuleRules
	{
        public ChaosFleshEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			SetupModulePhysicsSupport(Target);

			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"ProceduralMeshComponent",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ComputeFramework",
					"CoreUObject",
					"Chaos",
					"ChaosCaching",
					"ChaosFlesh",
					"DataflowCore",
					"DataflowEngine",
					"Engine",
					"FieldSystemEngine",
					"NetCore",
					"OptimusCore",
					"Projects",
					"RenderCore",
                    "RHI",
					"Renderer",
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"ChaosCachingUSD",
						"UnrealUSDWrapper",
						"USDClasses",
						"USDUtilities",
					});
			}
			else
			{
				PrivateDefinitions.Add("USE_USD_SDK=0");
			}

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);

			PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");

			// Build flavor selection copied from UnrealUSDWrapper, then modified.
			// Currently only Win64 is supported.
			if (EnableUsdSdk(Target) && (Target.Type == TargetType.Editor && Target.Platform == UnrealTargetPlatform.Win64))
			{
				bUseRTTI = true;
				PublicDefinitions.Add("DO_USD_CACHING=1");
			}
			else
			{
				PublicDefinitions.Add("DO_USD_CACHING=0");
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
