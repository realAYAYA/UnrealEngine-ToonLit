// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64")]
	public class DatasmithMax2020 : DatasmithMaxBase
	{
		public DatasmithMax2020(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetMaxVersion() { return "2020"; }
	}
}