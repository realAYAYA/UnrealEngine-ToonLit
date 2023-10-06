// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2024Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2024Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2024";
		ExeBinariesSubFolder = @"3DSMax\2024";

		AddCopyPostBuildStep(Target);
	}
}
