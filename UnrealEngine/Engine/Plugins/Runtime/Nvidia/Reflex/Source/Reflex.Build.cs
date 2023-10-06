// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class Reflex : ModuleRules
	{
		public Reflex(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			// Reevaulaute if ThirdParty\reflexstats.h is updated to compile with c++20
			// error C4002: too many arguments for function-like macro invocation '_TLG_FOR_imp0'
			CppStandard = CppStandardVersion.Cpp17;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Engine",
					"RHI",
					"CoreUObject",
					"SlateCore",
					"Slate"
				}
			);

			// Grab NVAPI
			AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");

			// Grab ReflexStat
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty"));
		}
	}
}
