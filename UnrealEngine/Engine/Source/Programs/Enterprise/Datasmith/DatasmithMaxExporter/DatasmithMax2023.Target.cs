// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2023Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2023Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2023";
		ExeBinariesSubFolder = @"3DSMax\2023";

		AddCopyPostBuildStep(Target);
	}
}
