// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DatasmithRevit2022Target : DatasmithRevitBaseTarget
{
	public DatasmithRevit2022Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersion() { return "2022"; }
}
