// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DatasmithRevit2019Target : DatasmithRevitBaseTarget
{
	public DatasmithRevit2019Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersion() { return "2019"; }
}
