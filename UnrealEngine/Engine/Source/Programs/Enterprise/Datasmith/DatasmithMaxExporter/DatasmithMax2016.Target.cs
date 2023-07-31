// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2016Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2016Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2016";
		ExeBinariesSubFolder = @"3DSMax\2016";

		AddCopyPostBuildStep(Target);
	}
}
