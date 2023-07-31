// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Core_HoloLens : Core
	{
		public Core_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.Add("../Platforms/HoloLens/Source/Runtime/Core/Public");
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib");

			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"IntelTBB",
				"XInput"
				);

			PublicDefinitions.Add("WITH_VS_PERF_PROFILER=0");
			PublicDefinitions.Add("IS_RUNNING_GAMETHREAD_ON_EXTERNAL_THREAD=1");

		}
	}
}
