// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64")]
	public class DatasmithRevit2019 : DatasmithRevitBase
	{
		public DatasmithRevit2019(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}
		
		public override string GetRevitVersion()
		{
			return "2019";
		}
	}
}
