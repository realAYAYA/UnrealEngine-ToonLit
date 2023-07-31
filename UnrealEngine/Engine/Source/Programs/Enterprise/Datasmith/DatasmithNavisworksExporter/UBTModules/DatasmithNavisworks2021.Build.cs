// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64")]
	public class DatasmithNavisworks2021 : DatasmithNavisworksBase
	{
		public DatasmithNavisworks2021(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetNavisworksVersion()
		{
			return "2021";
		}
	}
}
