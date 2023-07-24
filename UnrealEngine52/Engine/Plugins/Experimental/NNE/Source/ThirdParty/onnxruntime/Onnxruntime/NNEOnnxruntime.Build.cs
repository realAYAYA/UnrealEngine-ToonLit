// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNEOnnxruntime : ModuleRules
{
	public NNEOnnxruntime( ReadOnlyTargetRules Target ) : base( Target )
	{
		ShortName = "NNEORT"; // Shorten to avoid path-too-long errors

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
				System.IO.Path.Combine(ModuleDirectory, "../Dependencies"),
				System.IO.Path.Combine(ModuleDirectory, "../Dependencies/dlpack/include"),
				System.IO.Path.Combine(ModuleDirectory, "../Dependencies/date/include"),
				System.IO.Path.Combine(ModuleDirectory, "../Dependencies/gsl"),
				System.IO.Path.Combine(ModuleDirectory, "../Dependencies/json/single_include"),
				System.IO.Path.Combine(ModuleDirectory, "../Dependencies/mp11/include"),
				System.IO.Path.Combine(ModuleDirectory, "../Dependencies/SafeInt"),
				System.IO.Path.Combine(ModuleDirectory, "../Dependencies/wil/include"),
			}
		);

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
				"NNEFlatBuffers",
				"NNEOnnx",
				"NNEOnnxruntimeMlas",
				"NNEAbseilCpp",
				"ORTHelper"
			}
		);

		// Win64-only
		//if (Target.Platform == UnrealTargetPlatform.Win64)
		//{
		//	PrivateDependencyModuleNames.AddRange
		//		(
		//		new string[] {
		//			"DirectML_1_8_0",
		//			"DX12"
		//		}
		//	);
		//}
		// Linux
		if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac)
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
		IWYUSupport = IWYUSupport.None;

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

		// PublicDefinitions.Add("DISABLE_ABSEIL");

		// Win64-only
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("_USE_MATH_DEFINES");
			PublicDefinitions.Add("NOGDI");
			PublicDefinitions.Add("NOMINMAX");
			PublicDefinitions.Add("WIN32_LEAN_AND_MEAN");
			PublicDefinitions.Add("PLATFORM_WIN64");
			PublicDefinitions.Add("PLATFORM_NNE_MICROSOFT");
			//PublicDefinitions.Add("DML_TARGET_VERSION_USE_LATEST");
			//PublicDefinitions.Add("USE_DML = 1");
		}

		// Disable all static analysis checkers for this module
		bDisableStaticAnalysis = true;
	}
}
