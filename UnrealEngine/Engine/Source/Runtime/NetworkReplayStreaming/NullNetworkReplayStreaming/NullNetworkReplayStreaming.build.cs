// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NullNetworkReplayStreaming : ModuleRules
	{
		public NullNetworkReplayStreaming( ReadOnlyTargetRules Target ) : base(Target)
		{
            ShortName = "NullReplayStreaming";

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Engine",
					"NetworkReplayStreaming",
                    "Json",
				}
			);

			UnsafeTypeCastWarningLevel = WarningLevel.Error;
		}
	}
}
