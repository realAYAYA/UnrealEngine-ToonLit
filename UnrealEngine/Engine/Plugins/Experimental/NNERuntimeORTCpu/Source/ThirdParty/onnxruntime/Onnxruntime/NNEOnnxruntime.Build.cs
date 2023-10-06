// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNEOnnxruntime : ModuleRules
{
	public NNEOnnxruntime( ReadOnlyTargetRules Target ) : base( Target )
	{
		CppStandard = CppStandardVersion.Cpp17;

		// Disable all static analysis checkers for this module
		bDisableStaticAnalysis = true;

		ShortName = "NNEORT"; // Shorten to avoid path-too-long errors

		if (Target.StaticAnalyzer == StaticAnalyzer.None || Target.Platform.IsInGroup("Microsoft"))
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
		string DependenciesDirectory = System.IO.Path.Combine(ModuleDirectory, "../Dependencies");
		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(DependenciesDirectory, "dlpack/include"),
				System.IO.Path.Combine(DependenciesDirectory, "date/include"),
				System.IO.Path.Combine(DependenciesDirectory, "gsl/include"),
				System.IO.Path.Combine(DependenciesDirectory, "json/single_include"),
				System.IO.Path.Combine(DependenciesDirectory, "mp11/include"),
				System.IO.Path.Combine(DependenciesDirectory, "SafeInt"),
				System.IO.Path.Combine(DependenciesDirectory, "wil/include"),
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Eigen",
				"NNEFlatBuffers",
				"NNEProtobuf",
				"NNEOnnx",
				"NNEOnnxruntimeMlas",
				"NNEAbseilCpp",
				"ORTHelper"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrivateDependencyModuleNames.Add("NNENsync");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrivateDependencyModuleNames.Add("Re2");
		}

		bEnableUndefinedIdentifierWarnings = false;
		IWYUSupport = IWYUSupport.None;

		// PublicDefinitions
		// Disable exceptions (needed by UE Game)
		if (!Target.bBuildEditor)
		{
			// Note: to disable exceptions, ONNX Runtime requires the ORT_MINIMAL_BUILD directive.
			// We can not enable these flags because we can not filter files for compilation here.
			// https://onnxruntime.ai/docs/build/custom.html
			
			// PublicDefinitions.Add("ORT_MINIMAL_BUILD"); // Required by ORT_NO_EXCEPTIONS
			PublicDefinitions.Add("ORT_NO_RTTI");		// Required/implied by ORT_MINIMAL_BUILD

			PublicDefinitions.Add("ORT_NO_EXCEPTIONS");
			PublicDefinitions.Add("MLAS_NO_EXCEPTION");
			PublicDefinitions.Add("ONNX_NO_EXCEPTIONS");
			PublicDefinitions.Add("JSON_NOEXCEPTION");
		}
		else
		{
			bUseRTTI = true;
			bEnableExceptions = true;
		}

		PublicDefinitions.Add("WITH_UE");
		PublicDefinitions.Add("NDEBUG");

		PublicDefinitions.Add("ONNX_NAMESPACE = onnx");
		PublicDefinitions.Add("ONNX_ML");
		PublicDefinitions.Add("__ONNX_NO_DOC_STRINGS");

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
		}
	}
}
