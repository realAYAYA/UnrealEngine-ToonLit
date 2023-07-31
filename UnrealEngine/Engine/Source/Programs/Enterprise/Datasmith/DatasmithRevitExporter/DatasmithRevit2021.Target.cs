// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DatasmithRevit2021Target : DatasmithRevitBaseTarget
{
	public DatasmithRevit2021Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersion() { return "2021"; }
}
