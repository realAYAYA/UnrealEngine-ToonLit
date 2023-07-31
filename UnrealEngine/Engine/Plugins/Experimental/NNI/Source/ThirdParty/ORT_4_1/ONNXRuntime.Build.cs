// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ONNXRuntime : ModuleRules
{
	public ONNXRuntime( ReadOnlyTargetRules Target ) : base( Target )
	{
		ShortName = "ORT"; // Shorten to avoid path-too-long errors

		if (Target.StaticAnalyzer == StaticAnalyzer.None)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
			PrivatePCHHeaderFile = "Private/ORTPrivatePCH.h";
		}
		else
		{
			PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		}

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "./Internal/core/session"),
			}
		);

		// ThirdParty includes
		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "../Deps"),
				System.IO.Path.Combine(ModuleDirectory, "../Deps/date/include"),
				System.IO.Path.Combine(ModuleDirectory, "../Deps/gsl"),
				System.IO.Path.Combine(ModuleDirectory, "../Deps/json/single_include"),
				System.IO.Path.Combine(ModuleDirectory, "../Deps/mp11/include"),
				System.IO.Path.Combine(ModuleDirectory, "../Deps/optional-lite/include"),
				System.IO.Path.Combine(ModuleDirectory, "../Deps/SafeInt"),
				System.IO.Path.Combine(ModuleDirectory, "../Deps/wil/include"),
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					System.IO.Path.Combine(ModuleDirectory, "./Private/Windows"),
				}
			);
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 || 
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrivateDependencyModuleNames.AddRange
				(
				new string[] {
					"Protobuf",
					"Re2" // ONNXRuntimeRE2
				}
			);
		}

		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"Eigen",
				"FlatBuffers",
				"ONNX_1_11_0",
				"ONNXRuntimeProto_1_11_0",
				"ONNXRuntimeMLAS_2022_4_1",
				"AbseilCpp",
				"ThirdPartyHelperAndDLLLoader"
			}
		);

		// Win64-only
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange
				(
				new string[] {
					"DirectML_1_8_0",
					"DX12"
				}
			);
		}
		// Linux
		else if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrivateDependencyModuleNames.AddRange
				(
				new string[] {
					"Nsync"
				}
			);
		}

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
			}
		);

		bUseRTTI = true;
		bEnableUndefinedIdentifierWarnings = false;
		bEnforceIWYU = false;

		// PublicDefinitions
		// Disable exceptions (needed by UE Game)
		if (!Target.bBuildEditor)
		{
			PublicDefinitions.Add("ORT_NO_EXCEPTIONS");
		}

		PublicDefinitions.Add("WITH_UE");
		PublicDefinitions.Add("NDEBUG");
		PublicDefinitions.Add("GSL_UNENFORCED_ON_CONTRACT_VIOLATION");

		PublicDefinitions.Add("EIGEN_MPL2_ONLY");
		PublicDefinitions.Add("EIGEN_USE_THREADS");
		PublicDefinitions.Add("EIGEN_HAS_C99_MATH");
		PublicDefinitions.Add("EIGEN_HAS_CONSTEXPR");
		PublicDefinitions.Add("EIGEN_HAS_VARIADIC_TEMPLATES");
		PublicDefinitions.Add("EIGEN_HAS_CXX11_MATH");
		PublicDefinitions.Add("EIGEN_HAS_CXX11_ATOMIC");
		PublicDefinitions.Add("EIGEN_STRONG_INLINE = inline");

		PublicDefinitions.Add("ENABLE_ORT_FORMAT_LOAD");

		PublicDefinitions.Add("ONNX_NAMESPACE = onnx");
		PublicDefinitions.Add("ONNX_ML = 1");
		PublicDefinitions.Add("__ONNX_NO_DOC_STRINGS");

		PublicDefinitions.Add("LOTUS_LOG_THRESHOLD = 2");
		PublicDefinitions.Add("LOTUS_ENABLE_STDERR_LOGGING");
		PublicDefinitions.Add("UNICODE");
		PublicDefinitions.Add("_UNICODE");
		PublicDefinitions.Add("_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING");

		// Win64-only
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("_USE_MATH_DEFINES");
			PublicDefinitions.Add("NOGDI");
			PublicDefinitions.Add("NOMINMAX");
			PublicDefinitions.Add("WIN32_LEAN_AND_MEAN");
			PublicDefinitions.Add("PLATFORM_WIN64");
			PublicDefinitions.Add("PLATFORM_NNI_MICROSOFT");
			PublicDefinitions.Add("DML_TARGET_VERSION_USE_LATEST");
			PublicDefinitions.Add("USE_DML = 1");
		}

		// Disable all static analysis checkers for this module
		bDisableStaticAnalysis = true;
	}
}
