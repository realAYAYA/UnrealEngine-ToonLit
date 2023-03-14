// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ModelProtoFileReader : ModuleRules
{
	public ModelProtoFileReader( ReadOnlyTargetRules Target ) : base( Target )
	{
		// Define when ModelProtoFileReader is available
		// Disabled to avoid linking error when compiled in-game with UEAndORT back end, Schema*.txt/*.cpp files also modified
		bool bIsModelProtoFileReaderSupported = false; //(Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac);
		if (bIsModelProtoFileReaderSupported)
		{
			PublicDefinitions.Add("WITH_MODEL_PROTO_CONVERTER_SUPPORT");
		}

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "..")
			}
		);

		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Generated"));

		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"ModelProto"
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				// "ONNXRuntimeProto",
				"ThirdPartyHelperAndDLLLoader"
			}
		);

		if (bIsModelProtoFileReaderSupported)
		{
			PrivateDependencyModuleNames.AddRange
				(
				new string[] {
					"Protobuf"
				}
			);
		}
	}
}