// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64")]
	public class DatasmithRevit2023 : DatasmithRevitBase
	{
		public DatasmithRevit2023(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}
		
		public override string GetRevitVersion()
		{
			return "2023";
		}
	}
}
