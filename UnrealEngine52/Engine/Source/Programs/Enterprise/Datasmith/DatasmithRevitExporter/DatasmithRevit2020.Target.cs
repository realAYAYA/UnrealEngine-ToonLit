// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DatasmithRevit2020Target : DatasmithRevitBaseTarget
{
	public DatasmithRevit2020Target(TargetInfo Target)
		: base(Target)
	{
		// Make sure the C# facade is up to date
		// Commented by default as it require write access on the facade generated files, which is not usually the case...
		// PreBuildTargets.Add(new TargetInfo("DatasmithFacadeCSharp", Target.Platform, Target.Configuration, Target.Architecture, null, Target.Arguments));
	}

	public override string GetVersion() { return "2020"; }
}
