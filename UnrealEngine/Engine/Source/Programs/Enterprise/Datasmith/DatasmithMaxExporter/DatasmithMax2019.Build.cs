// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64")]
	public class DatasmithMax2019 : DatasmithMaxBase
	{
		public DatasmithMax2019(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetMaxVersion() { return "2019"; }
	}
}