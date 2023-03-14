// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DatasmithRevit2023Target : DatasmithRevitBaseTarget
{
	public DatasmithRevit2023Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersion() { return "2023"; }
}
