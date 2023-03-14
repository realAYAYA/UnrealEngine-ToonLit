// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64")]
	public class DatasmithMax2021 : DatasmithMaxBase
	{
		public DatasmithMax2021(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetMaxVersion() { return "2021"; }
	}
}