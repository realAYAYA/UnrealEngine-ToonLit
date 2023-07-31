// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2021Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2021Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2021";
		ExeBinariesSubFolder = @"3DSMax\2021";

		AddCopyPostBuildStep(Target);
	}
}
