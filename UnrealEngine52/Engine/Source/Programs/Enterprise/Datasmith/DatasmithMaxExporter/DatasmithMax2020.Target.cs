// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2020Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2020Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2020";
		ExeBinariesSubFolder = @"3DSMax\2020";

		AddCopyPostBuildStep(Target);
	}
}
