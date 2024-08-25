// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithRhino8 : DatasmithRhinoBase
	{
		public DatasmithRhino8(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetRhinoVersion()
		{
			return "8";
		}
	}
}
