// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InMemoryNetworkReplayStreaming : ModuleRules
	{
		public InMemoryNetworkReplayStreaming( ReadOnlyTargetRules Target ) : base(Target)
		{
			PublicIncludePathModuleNames.Add("Engine");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"NetworkReplayStreaming",
                    "Json"
				}
			);
		}
	}
}
