// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelUnitTester.h"
#include "NeuralTimer.h"
#include "NeuralNetworkInferenceQAUtils.h"
#include "Misc/Paths.h"

#ifndef SKIP_ModelLoadAccuracyAndSpeedTests
#define SKIP_ModelLoadAccuracyAndSpeedTests 0
#endif

/* FModelUnitTester axuiliary functions
 *****************************************************************************/

struct FNNIUnitTesterTimeData
{
	FNeuralStatistics ComputeTimeData;
	FNeuralStatistics InputCopyTimeData;
	FNeuralStatistics OutputCopyTimeData;

	FNNIUnitTesterTimeData(const FNeuralStatistics& InComputeTimeData, const FNeuralStatistics& InInputCopyTimeData,
		const FNeuralStatistics& InOutputCopyTimeData)
		: ComputeTimeData(InComputeTimeData)
		, InputCopyTimeData(InInputCopyTimeData)
		, OutputCopyTimeData(InOutputCopyTimeData)
	{}
};

FNNIUnitTesterTimeData FModelUnitTester_GetTimeInformation(UNeuralNetwork* InOutNetwork, TArray<float>& OutCPUGPUCPUOutput,
	const TArray<float>& InInputArray, const int32 InRepetitions)
{
	/* Input/output copy speed */
	FNeuralStatisticsEstimator OutCopyingStats;
	FNeuralTimer Timer;
	for (int32 TimerIndex = 0; TimerIndex < InRepetitions; ++TimerIndex)
	{
		InOutNetwork->SetInputFromArrayCopy(InInputArray);
		Timer.Tic();
		OutCPUGPUCPUOutput = InOutNetwork->GetOutputTensor().GetArrayCopy<float>();
		const float CurrentCopyingTime = Timer.Toc();
		OutCopyingStats.StoreSample(CurrentCopyingTime);

	}

	/* Forward() speed */
	if (InRepetitions > 1)
	{
		for (int32 TimerIndex = 0; TimerIndex < 5; ++TimerIndex)
		{
			InOutNetwork->Run();
		}
	}

	if (InRepetitions > 0)
	{
		for (int32 TimerIndex = 0; TimerIndex < InRepetitions; ++TimerIndex)
		{
			InOutNetwork->SetInputFromArrayCopy(InInputArray);
			InOutNetwork->Run();
			OutCPUGPUCPUOutput = InOutNetwork->GetOutputTensor().GetArrayCopy<float>();
		}
	}
	// Return NetworkTimeData
	return FNNIUnitTesterTimeData(InOutNetwork->GetRunStatistics(), InOutNetwork->GetInputMemoryTransferStats(), OutCopyingStats.GetStats());
}



/* FModelUnitTester static public functions
 *****************************************************************************/

bool FModelUnitTester::GlobalTest(const FString& InProjectContentDir, const FString& InModelZooRelativeDirectory)
{
	// NOTE: models are separated into multiple lines to enable/disable particular network for faster testing

	// Model names, input values, and number of repetitions for profiling
	const TArray<FString> ModelNames({
		TEXT("cloth_network"),
		TEXT("HS"),
		TEXT("RL")
	});

	// Ground truths
	const TArray<TArray<double>> CPUGroundTruths({
		{0.042571, 0.023693, 0.015783, 13.100505, 8.050994, 0.028807, 0.016387},
		{138.372906, 126.753839, 127.287254, 130.316062, 127.303424, 124.800896, 126.546051},
		{0.488662, 0.472437, 0.478862, 0.522685, 0.038322, 0.480848, 0.483821}
	});
	
	const TArray<TArray<double>> GPUGroundTruths({
		{0.042571, 0.023693, 0.015783, 13.100504, 8.050994, 0.028807, 0.016387},
		{138.373184, 126.754100, 127.287398, 130.316194, 127.303495, 124.801134, 126.5462530},
		{0.488662, 0.472437, 0.478862, 0.522685, 0.038322, 0.480848, 0.483821}
	});

	const TArray<float> InputArrayValues({ // This one can be shorter than CPU/GPUGroundTruths
		1.f,
		0.f,
		-1.f,
		100.f,
		-100.f,
		0.5f,
		-0.5f
	});
	
	// Speed profiling test - 0 repetitions means that test will not be run
#ifdef WITH_UE_AND_ORT_SUPPORT
	const TArray<int32> CPURepetitionsForUEAndORTBackEnd({1000,  50, 1000 });
#ifdef PLATFORM_WIN64
	const TArray<int32> GPURepetitionsForUEAndORTBackEnd({1000, 100, 1000 });
#else //PLATFORM_WIN64
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("FModelUnitTester::GlobalTest(): GPU tests disabled for non-Windows platforms."));
	const TArray<int32> GPURepetitionsForUEAndORTBackEnd({ 0, 0, 0 });
#endif //PLATFORM_WIN64
#else //WITH_UE_AND_ORT_SUPPORT
	const TArray<int32> CPURepetitionsForUEAndORTBackEnd({ 0, 0, 0 });
	const TArray<int32> GPURepetitionsForUEAndORTBackEnd({ 0, 0, 0 });
#endif //WITH_UE_AND_ORT_SUPPORT
#if WITH_EDITOR
	const TArray<int32> CPURepetitionsForUEOnlyBackEnd({ 0, 0, 0 });
	const TArray<int32> GPURepetitionsForUEOnlyBackEnd({ 10, 0, 0 });
#else //WITH_EDITOR
	const TArray<int32> CPURepetitionsForUEOnlyBackEnd({ 0, 0, 0 });
	const TArray<int32> GPURepetitionsForUEOnlyBackEnd({ 0, 0, 0 });
#endif //WITH_EDITOR
	// Run tests
	UE_LOG(LogNeuralNetworkInferenceQA, Display,
		TEXT("ENeuralBackEnd::UEOnly test for FModelUnitTester::GlobalTest disabled due to not being compatible with ORT (linking error issues)."));
#if !SKIP_ModelLoadAccuracyAndSpeedTests
	return ModelLoadAccuracyAndSpeedTests(InProjectContentDir, InModelZooRelativeDirectory, ModelNames, InputArrayValues, CPUGroundTruths,
		GPUGroundTruths, CPURepetitionsForUEAndORTBackEnd, GPURepetitionsForUEAndORTBackEnd, CPURepetitionsForUEOnlyBackEnd,
		GPURepetitionsForUEOnlyBackEnd);
#else
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("ENeuralBackEnd::FModelUnitTester::GlobalTest disabled due to not being compatible with cooked platform build"));
	return true; 
#endif
}



/* FModelUnitTester static private functions
 *****************************************************************************/

bool FModelUnitTester::ModelLoadAccuracyAndSpeedTests(const FString& InProjectContentDir, const FString& InModelZooRelativeDirectory,
	const TArray<FString>& InModelNames, const TArray<float>& InInputArrayValues, const TArray<TArray<double>>& InCPUGroundTruths,
	const TArray<TArray<double>>& InGPUGroundTruths, const TArray<int32>& InCPURepetitionsForUEAndORTBackEnd,
	const TArray<int32>& InGPURepetitionsForUEAndORTBackEnd, const TArray<int32>& InCPURepetitionsForUEOnlyBackEnd,
	const TArray<int32>& InGPURepetitionsForUEOnlyBackEnd)
{
	bool bDidGlobalTestPassed = true;

	const FString ModelZooDirectory = InProjectContentDir / InModelZooRelativeDirectory;

	// Test OTXT/UAsset accuracy
	for (int32 ModelIndex = 0; ModelIndex < InModelNames.Num(); ++ModelIndex)
	{
		const FString& ModelName = InModelNames[ModelIndex];
		const TArray<double>& CPUGroundTruths = InCPUGroundTruths[ModelIndex];
		const TArray<double>& GPUGroundTruths = InGPUGroundTruths[ModelIndex];

		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %s - Network Uasset Load and Run"), *ModelName);
		const FString UAssetModelFilePath = GetUAssetModelFilePath(ModelName, InModelZooRelativeDirectory);
		UNeuralNetwork* Network = NetworkUassetLoadTest(UAssetModelFilePath);
		if (!Network)
		{
			UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("UNeuralNetwork could not be loaded from UAssetModelFilePath %s."),
				*UAssetModelFilePath);
			return false;
		}
		// Input debugging
		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %s - Input/Output"), *ModelName);
		for (int32 TensorIndex = 0; TensorIndex < Network->GetInputTensorNumber(); ++TensorIndex)
		{
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("InputTensor[%d] = %s."), TensorIndex,
				*Network->GetInputTensor(TensorIndex).GetName());
		}
		// Output debugging
		for (int32 TensorIndex = 0; TensorIndex < Network->GetOutputTensorNumber(); ++TensorIndex)
		{
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("OutputTensor[%d] = %s."), TensorIndex,
				*Network->GetOutputTensor(TensorIndex).GetName());
		}
		const bool bShouldRunUEAndORTBackEnd = (InCPURepetitionsForUEAndORTBackEnd[ModelIndex] + InGPURepetitionsForUEAndORTBackEnd[ModelIndex] > 0);
		if (bShouldRunUEAndORTBackEnd)
		{
			bDidGlobalTestPassed &= ModelAccuracyTest(Network, ENeuralSynchronousMode::Synchronous, UNeuralNetwork::ENeuralBackEnd::UEAndORT,
				InInputArrayValues, CPUGroundTruths, GPUGroundTruths);
			bDidGlobalTestPassed &= ModelAccuracyTest(Network, ENeuralSynchronousMode::Asynchronous, UNeuralNetwork::ENeuralBackEnd::UEAndORT,
				InInputArrayValues, CPUGroundTruths, GPUGroundTruths);
		}
		const bool bShouldRunUEOnlyBackEnd = false; // (InCPURepetitionsForUEOnlyBackEnd[ModelIndex] * InCPURepetitionsForUEOnlyBackEnd[ModelIndex] > 0);
		if (bShouldRunUEOnlyBackEnd)
		{
			bDidGlobalTestPassed &= ModelAccuracyTest(Network, ENeuralSynchronousMode::Synchronous, UNeuralNetwork::ENeuralBackEnd::UEOnly,
				InInputArrayValues, CPUGroundTruths, GPUGroundTruths);
		}

		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("---------------------------------------------------------------------------------------------------------------------------------"));

		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %s - Network ONNX/ORT Load and Run"), *ModelName);
#if WITH_EDITOR
		const int32 ONNXOrORTIndex = 0; //for (int32 ONNXOrORTIndex = 0; ONNXOrORTIndex < 2; ++ONNXOrORTIndex)
		{
			const FString ModelFilePath = (ONNXOrORTIndex % 2 == 0 ? GetONNXModelFilePath(ModelZooDirectory, ModelName)
				: GetORTModelFilePath(ModelZooDirectory, ModelName));
			const FString ModelType = (ONNXOrORTIndex % 2 == 0 ? TEXT("ONNX") : TEXT("ORT"));
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- %s - Network %s Load and Run - %s"), *ModelName, *ModelType,
				*ModelFilePath);
			if (bShouldRunUEAndORTBackEnd)
			{
				bDidGlobalTestPassed &= ModelAccuracyTest(NetworkONNXOrORTLoadTest(ModelFilePath), ENeuralSynchronousMode::Synchronous,
					UNeuralNetwork::ENeuralBackEnd::UEAndORT, InInputArrayValues, CPUGroundTruths, GPUGroundTruths);
				bDidGlobalTestPassed &= ModelAccuracyTest(NetworkONNXOrORTLoadTest(ModelFilePath), ENeuralSynchronousMode::Asynchronous,
					UNeuralNetwork::ENeuralBackEnd::UEAndORT, InInputArrayValues, CPUGroundTruths, GPUGroundTruths);
			}
			if (bShouldRunUEOnlyBackEnd)
			{
				bDidGlobalTestPassed &= ModelAccuracyTest(NetworkONNXOrORTLoadTest(ModelFilePath), ENeuralSynchronousMode::Synchronous,
					UNeuralNetwork::ENeuralBackEnd::UEOnly, InInputArrayValues, CPUGroundTruths, GPUGroundTruths);
			}
		}
#else //WITH_EDITOR
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- Skipped (only if WITH_EDITOR enabled)."));
#endif //WITH_EDITOR
	}

	// Profile speed
	for (int32 ModelIndex = 0; ModelIndex < InModelNames.Num(); ++ModelIndex)
	{
		const FString& ModelName = InModelNames[ModelIndex];
		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %s - Network UAsset Speed Profiling"), *ModelName);
		const FString UAssetModelFilePath = GetUAssetModelFilePath(ModelName, InModelZooRelativeDirectory);

		// UEAndORT (if WITH_UE_AND_ORT_SUPPORT)
#ifdef WITH_UE_AND_ORT_SUPPORT
		bDidGlobalTestPassed &= ModelSpeedTest(UAssetModelFilePath, ENeuralDeviceType::CPU, UNeuralNetwork::ENeuralBackEnd::UEAndORT,
			InCPURepetitionsForUEAndORTBackEnd[ModelIndex]);
		UNeuralNetwork* Network = NetworkUassetLoadTest(UAssetModelFilePath);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (Network->GetBackEndForCurrentPlatform() != UNeuralNetwork::ENeuralBackEnd::UEAndORT)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		{
			UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("-------------------- Default UAsset BackEnd should be UEAndORT."));
			return false;
		}
		else if (InGPURepetitionsForUEAndORTBackEnd[ModelIndex] > 0 && Network->IsGPUSupported())
		{
			bDidGlobalTestPassed &= ModelSpeedTest(UAssetModelFilePath, ENeuralDeviceType::GPU, UNeuralNetwork::ENeuralBackEnd::UEAndORT,
				InGPURepetitionsForUEAndORTBackEnd[ModelIndex]);
		}
		else
		{
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- ModelSpeedTest-UEAndORT-GPU skipped (DX12 not enabled or"
				" InGPURepetitionsForUEAndORTBackEnd[ModelIndex] = %d is 0)."), InGPURepetitionsForUEAndORTBackEnd[ModelIndex]);
		}
#endif //WITH_UE_AND_ORT_SUPPORT

		// UEOnly
		const bool bShouldRunUEOnlyBackEnd = false;
		if (bShouldRunUEOnlyBackEnd)
		{
			bDidGlobalTestPassed &= ModelSpeedTest(UAssetModelFilePath, ENeuralDeviceType::CPU, UNeuralNetwork::ENeuralBackEnd::UEOnly,
				InCPURepetitionsForUEOnlyBackEnd[ModelIndex]);
			bDidGlobalTestPassed &= ModelSpeedTest(UAssetModelFilePath, ENeuralDeviceType::GPU, UNeuralNetwork::ENeuralBackEnd::UEOnly,
				InGPURepetitionsForUEOnlyBackEnd[ModelIndex]);
		}
	}

	return bDidGlobalTestPassed;
}

FString FModelUnitTester::GetONNXModelFilePath(const FString& ModelZooDirectory, const FString& InModelName)
{
	// E.g., ModelZooDirectory / TEXT("ExampleNetworkReadable/ExampleNetworkReadable.onnx")
	return /*FPaths::ConvertRelativePathToFull*/(ModelZooDirectory / InModelName + TEXT("/") + InModelName + TEXT(".onnx"));
}

FString FModelUnitTester::GetORTModelFilePath(const FString& ModelZooDirectory, const FString& InModelName)
{
	// E.g., ModelZooDirectory / TEXT("ExampleNetworkReadable/ExampleNetworkReadable.ort")
	return /*FPaths::ConvertRelativePathToFull*/(ModelZooDirectory / InModelName + TEXT("/") + InModelName + TEXT(".ort"));
}

FString FModelUnitTester::GetUAssetModelFilePath(const FString& InModelName, const FString& InModelZooRelativeDirectory)
{
	// E.g., '/Game/[MODEL_ZOO_DIR]/ExampleNetworkReadable/ExampleNetworkReadable.ExampleNetworkReadable'
	return InModelName + TEXT("'/Game/") + InModelZooRelativeDirectory / InModelName + TEXT("/") + InModelName + TEXT(".") + InModelName + TEXT("'");
}

UNeuralNetwork* FModelUnitTester::NetworkUassetLoadTest(const FString& InUAssetPath)
{
	UNeuralNetwork* Network = LoadObject<UNeuralNetwork>((UObject*)GetTransientPackage(), *InUAssetPath);
	if (!Network)
	{
		ensureMsgf(false, TEXT("UNeuralNetwork is a nullptr. Path: \"%s\"."), *InUAssetPath);
		return nullptr;
	}
	if (!Network->IsLoaded())
	{
		ensureMsgf(false, TEXT("UNeuralNetwork could not be loaded from uasset disk location. Path: \"%s\"."), *InUAssetPath);
		return nullptr;
	}
	return Network;
}

UNeuralNetwork* FModelUnitTester::NetworkONNXOrORTLoadTest(const FString& InModelFilePath)
{
	// Load network architecture and weights from file
	UNeuralNetwork* Network = NewObject<UNeuralNetwork>((UObject*)GetTransientPackage(), UNeuralNetwork::StaticClass());
	if (!Network)
	{
		ensureMsgf(false, TEXT("UNeuralNetwork is a nullptr. Path: \"%s\"."), *InModelFilePath);
		return nullptr;
	}
	if (!Network->Load(InModelFilePath))
	{
		ensureMsgf(false, TEXT("UNeuralNetwork could not be loaded from ONNX file disk location. Path: \"%s\"."), *InModelFilePath);
		return nullptr;
	}
	return Network;
}

bool FModelUnitTester::ModelAccuracyTest(UNeuralNetwork* InOutNetwork, const ENeuralSynchronousMode InSynchronousMode,
	const UNeuralNetwork::ENeuralBackEnd InBackEnd, const TArray<float>& InInputArrayValues, const TArray<double>& InCPUGroundTruths,
	const TArray<double>& InGPUGroundTruths)
{
	// Sanity check
	if (!InOutNetwork)
	{
		return false;
	}
	// Find NetworkSize
	const int64 NetworkSize = InOutNetwork->GetInputTensor().Num();
	// Initialize input data
	TArray<TArray<float>> InputArrays;
	{
		InputArrays.Reserve(InInputArrayValues.Num());
		for (const float InputArrayValue : InInputArrayValues)
		{
			InputArrays.Emplace(TArray<float>({}));
			InputArrays.Last().Init(InputArrayValue, NetworkSize);
		}
		if (InputArrays.Num() > InCPUGroundTruths.Num() || InputArrays.Num() > InGPUGroundTruths.Num())
		{
			UE_LOG(LogNeuralNetworkInferenceQA, Warning,
				TEXT("InputArrays.Num() <= InCPUGroundTruths.Num() && InputArrays.Num() <= InGPUGroundTruths.Num() failed: %d vs. %d vs. %d."),
				InputArrays.Num(), InCPUGroundTruths.Num(), InGPUGroundTruths.Num());
			return false;
		}
	}
	
	// Save original network state
	const ENeuralDeviceType OriginalDeviceType = InOutNetwork->GetDeviceType();
	const ENeuralDeviceType OriginalInputDeviceType = InOutNetwork->GetInputDeviceType();
	const ENeuralDeviceType OriginalOutputDeviceType = InOutNetwork->GetOutputDeviceType();
	const ENeuralSynchronousMode OriginalSynchronousMode = InOutNetwork->GetSynchronousMode();
	const ENeuralThreadMode OriginalThreadModeDelegateForAsyncRunCompleted = InOutNetwork->GetThreadModeDelegateForAsyncRunCompleted();
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const UNeuralNetwork::ENeuralBackEnd OriginalBackEnd = InOutNetwork->GetBackEnd();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	// Set (a)synchronous Mode
	InOutNetwork->SetSynchronousMode(InSynchronousMode);
	InOutNetwork->SetThreadModeDelegateForAsyncRunCompleted(ENeuralThreadMode::AnyThread);

	// Set back end
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!InOutNetwork->SetBackEnd(InBackEnd))
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("Backend %s is disabled."), *GetBackEndString(InBackEnd));
		return false;
	}
	const FString BackEndString = GetBackEndString(InBackEnd);
	
	// Run each input with CPU/GPU and compare with each other and with the ground truth
	// Multiple for loops to make sure that running on the CPU does not affect GPU results or vice versa
	bool bDidGlobalTestPassed = true;
	TArray<TArray<float>> CPUOutputs, CPUGPUCPUOutputs, CPUGPUGPUOutputs, GPUGPUCPUOutputs;
	
	// Input CPU + Network CPU + Output CPU
	ModelAccuracyTestRun(CPUOutputs, InOutNetwork, InputArrays, /*DeviceType*/ENeuralDeviceType::CPU, /*InputDeviceType*/ENeuralDeviceType::CPU,
		/*OutputDeviceType*/ENeuralDeviceType::CPU);

	if (InOutNetwork->IsGPUSupported())
	{
		// Input CPU + Network GPU + Output CPU
		ModelAccuracyTestRun(CPUGPUCPUOutputs, InOutNetwork, InputArrays, /*DeviceType*/ENeuralDeviceType::GPU,
			/*InputDeviceType*/ENeuralDeviceType::CPU, /*OutputDeviceType*/ENeuralDeviceType::CPU);
		// Input CPU + Network GPU + Output GPU
		ModelAccuracyTestRun(CPUGPUGPUOutputs, InOutNetwork, InputArrays, /*DeviceType*/ENeuralDeviceType::GPU,
			/*InputDeviceType*/ENeuralDeviceType::CPU, /*OutputDeviceType*/ENeuralDeviceType::GPU);
		// Input GPU + Network GPU + Output CPU
		ModelAccuracyTestRun(GPUGPUCPUOutputs, InOutNetwork, InputArrays, /*DeviceType*/ENeuralDeviceType::GPU,
			/*InputDeviceType*/ENeuralDeviceType::GPU, /*OutputDeviceType*/ENeuralDeviceType::CPU);
	}
	else
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- ModelAccuracyTest-UEAndORT-GPU skipped (DX12 not enabled)."));
	}

	for (int32 Index = 0; Index < InputArrays.Num(); ++Index)
	{
		const TArray<float>& GPUGPUCPUOutput = (InOutNetwork->IsGPUSupported() ? GPUGPUCPUOutputs[Index] : CPUOutputs[Index]);
		const TArray<float>& CPUGPUCPUOutput = (InOutNetwork->IsGPUSupported() ? CPUGPUCPUOutputs[Index] : CPUOutputs[Index]);
		const TArray<float>& CPUGPUGPUOutput = (InOutNetwork->IsGPUSupported() ? CPUGPUGPUOutputs[Index] : CPUOutputs[Index]);
		// Prepare verbose
		const double CPUAvgL1Norm = GetAveragedL1Norm(CPUOutputs[Index]);
		const double CPUGPUCPUAvgL1Norm = GetAveragedL1Norm(CPUGPUCPUOutput);
		const double GPUGPUCPUAvgL1Norm = GetAveragedL1Norm(GPUGPUCPUOutput);
		const double CPUGPUGPUAvgL1Norm = GetAveragedL1Norm(CPUGPUGPUOutput);
		const double RelativeCoefficient = 1. / FMath::Max(1., FMath::Min(CPUAvgL1Norm, CPUGPUCPUAvgL1Norm)); // Max(1, X) to avoid 0s
		const double CPUGPUAvgL1NormDiff = GetAveragedL1NormDiff(CPUOutputs[Index], CPUGPUCPUOutput) * RelativeCoefficient * 1e6;
		const double GPUGPUInputAvgL1NormDiff = GetAveragedL1NormDiff(CPUGPUCPUOutput, GPUGPUCPUOutput)
			/ FMath::Max(1., FMath::Min(GPUGPUCPUAvgL1Norm, CPUGPUCPUAvgL1Norm)) * 1e12;
		const double GPUGPUOutputAvgL1NormDiff = GetAveragedL1NormDiff(CPUGPUCPUOutput, CPUGPUGPUOutput)
			/ FMath::Max(1., FMath::Min(CPUGPUGPUAvgL1Norm, CPUGPUCPUAvgL1Norm)) * 1e12;
		const double FastCPUGPUAvgL1NormDiff = FMath::Abs((CPUAvgL1Norm - CPUGPUCPUAvgL1Norm)) * RelativeCoefficient * 1e6;
		const double FastCPUAvgL1NormDiff = FMath::Abs(CPUAvgL1Norm - InCPUGroundTruths[Index])
			/ FMath::Max(1., FMath::Min(CPUAvgL1Norm, InCPUGroundTruths[Index])) * 1e7;
		const double FastGPUAvgL1NormDiff = FMath::Abs(CPUGPUCPUAvgL1Norm - InGPUGroundTruths[Index])
			/ FMath::Max(1., FMath::Min(CPUGPUCPUAvgL1Norm, InGPUGroundTruths[Index])) * 1e7;
		// Print verbose
		UE_LOG(LogNeuralNetworkInferenceQA, Display,
			TEXT("%s: InputNorm = %f, OutputNormCPU = %f, OutputNormGPU = %f, OutputNormCPUGPUGPU = %f, OutputNormGT = %f, CPUAvgL1Norm = %f, CPUGPUCPUAvgL1Norm = %f,"),
			*BackEndString, GetAveragedL1Norm(InputArrays[Index]), CPUAvgL1Norm, CPUGPUCPUAvgL1Norm, CPUGPUGPUAvgL1Norm, InCPUGroundTruths[Index], CPUAvgL1Norm, CPUGPUCPUAvgL1Norm);
		UE_LOG(LogNeuralNetworkInferenceQA, Display,
			TEXT("\tCPUGPUAvgL1NormDiff = %fe-6, GPUGPUInputAvgL1NormDiff = %fe-7, GPUGPUOutputAvgL1NormDiff = %fe-7, FastCPUGPUAvgL1NormDiff = %fe-6, FastCPUAvgL1NormDiff = %fe-7, FastGPUAvgL1NormDiff = %fe-7 (1e-7 is roughly the precision for float)."),
			CPUGPUAvgL1NormDiff, GPUGPUInputAvgL1NormDiff, GPUGPUOutputAvgL1NormDiff, FastCPUGPUAvgL1NormDiff, FastCPUAvgL1NormDiff, FastGPUAvgL1NormDiff);
		// Check if test failed and (if so) display information
		const bool bDidSomeTestFailed = (!FMath::IsFinite(FastCPUGPUAvgL1NormDiff) || FastCPUGPUAvgL1NormDiff > 5)
			|| (!FMath::IsFinite(CPUGPUAvgL1NormDiff) || CPUGPUAvgL1NormDiff > 5)
			|| (!FMath::IsFinite(GPUGPUInputAvgL1NormDiff) || GPUGPUInputAvgL1NormDiff > 1)
			|| (!FMath::IsFinite(GPUGPUOutputAvgL1NormDiff) || GPUGPUOutputAvgL1NormDiff > 1)
			|| (!FMath::IsFinite(FastCPUAvgL1NormDiff) || FastCPUAvgL1NormDiff > 30)
			|| (!FMath::IsFinite(FastGPUAvgL1NormDiff) || FastGPUAvgL1NormDiff > 30);
		if (bDidSomeTestFailed)
		{
			const int64 MaxNumberElementsToDisplay = 100;
			const TArray<int64>& InputSizes = InOutNetwork->GetInputTensor().GetSizes();
			const TArray<int64>& OutputSizes = InOutNetwork->GetOutputTensor().GetSizes();
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("FastCPUGPUAvgL1NormDiff (%fe-6) < 5e-6 might have failed."), FastCPUGPUAvgL1NormDiff);
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("CPUGPUAvgL1NormDiff (%fe-6) < 5e-6 might have failed."), CPUGPUAvgL1NormDiff);
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("GPUGPUInputAvgL1NormDiff (%fe-12) < 1e-12 might have failed."),
				GPUGPUInputAvgL1NormDiff);
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("GPUGPUOutputAvgL1NormDiff (%fe-12) < 1e-12 might have failed."),
				GPUGPUOutputAvgL1NormDiff);
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("FastCPUAvgL1NormDiff (%fe-7) < 30e-7 might have failed (~30 times the float precision).\nCPUOutput = %s."),
				FastCPUAvgL1NormDiff, *FNeuralTensor(CPUOutputs[Index], OutputSizes).ToString(MaxNumberElementsToDisplay));
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("FastGPUAvgL1NormDiff (%fe-7) < 30e-7 might have failed (~30 times the float precision).\nCPUGPUCPUOutput = %s."),
				FastGPUAvgL1NormDiff, *FNeuralTensor(CPUGPUCPUOutput, OutputSizes).ToString(MaxNumberElementsToDisplay));
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("Input = %s"),
				*FNeuralTensor(InOutNetwork->GetInputTensor().GetArrayCopy<float>(), InputSizes).ToString(MaxNumberElementsToDisplay));
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("CPUOutput = %s"),
				*FNeuralTensor(CPUOutputs[Index], OutputSizes).ToString(MaxNumberElementsToDisplay));
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("CPUGPUCPUOutput = %s"),
				*FNeuralTensor(CPUGPUCPUOutput, OutputSizes).ToString(MaxNumberElementsToDisplay));
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("GPUGPUCPUOutput = %s"),
				*FNeuralTensor(GPUGPUCPUOutput, OutputSizes).ToString(MaxNumberElementsToDisplay));
			UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("CPUGPUGPUOutput = %s"),
				*FNeuralTensor(CPUGPUGPUOutput, OutputSizes).ToString(MaxNumberElementsToDisplay));
			UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("At least 1 of the 6 CPU/GPU tests failed."));
			return false;
		}
		bDidGlobalTestPassed &= !bDidSomeTestFailed;
	}
	// Reset to original network state
	InOutNetwork->SetDeviceType(/*DeviceType*/OriginalDeviceType, /*InputDeviceType*/OriginalInputDeviceType,
		/*OutputDeviceType*/OriginalOutputDeviceType);
	InOutNetwork->SetSynchronousMode(OriginalSynchronousMode);
	InOutNetwork->SetThreadModeDelegateForAsyncRunCompleted(OriginalThreadModeDelegateForAsyncRunCompleted);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InOutNetwork->SetBackEnd(OriginalBackEnd);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	// Test successful
	return bDidGlobalTestPassed;
}

void FModelUnitTester::ModelAccuracyTestRun(TArray<TArray<float>>& OutOutputs, UNeuralNetwork* InOutNetwork,
	const TArray<TArray<float>> InInputArrays, const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType,
	const ENeuralDeviceType InOutputDeviceType)
{
	std::atomic<bool> bDidAsyncNetworkFinished(false);
	const ENeuralSynchronousMode SynchronousMode = InOutNetwork->GetSynchronousMode();
	if (SynchronousMode == ENeuralSynchronousMode::Asynchronous)
	{
		InOutNetwork->GetOnAsyncRunCompletedDelegate().BindLambda([&bDidAsyncNetworkFinished]()
		{
			bDidAsyncNetworkFinished = true;
		});
	}
	for (int32 Index = 0; Index < InInputArrays.Num(); ++Index)
	{
		bDidAsyncNetworkFinished = false;
		InOutNetwork->SetDeviceType(InDeviceType, InInputDeviceType, InOutputDeviceType);
		InOutNetwork->SetInputFromArrayCopy(InInputArrays[Index]);
		if (InInputDeviceType == ENeuralDeviceType::GPU)
		{
			InOutNetwork->InputTensorsToGPU();
		}
		InOutNetwork->Run();
		// If async test, wait until inference is completed
		if (SynchronousMode == ENeuralSynchronousMode::Asynchronous)
		{
			while (!bDidAsyncNetworkFinished)
			{
				FPlatformProcess::Sleep(0.1e-3);
			}
		}
		if (InOutputDeviceType == ENeuralDeviceType::GPU)
		{
			InOutNetwork->OutputTensorsToCPU();
		}
		OutOutputs.Emplace(InOutNetwork->GetOutputTensor().GetArrayCopy<float>());
	}
	if (SynchronousMode == ENeuralSynchronousMode::Asynchronous)
	{
		InOutNetwork->GetOnAsyncRunCompletedDelegate().Unbind();
	}
}

bool FModelUnitTester::ModelSpeedTest(const FString& InUAssetPath, const ENeuralDeviceType InDeviceType,
	const UNeuralNetwork::ENeuralBackEnd InBackEnd, const int32 InRepetitions)
{
	// Get debug strings
	const FString DeviceTypeString = GetDeviceTypeString(InDeviceType);
	const FString BackEndString = GetBackEndString(InBackEnd);
	// Skip test if reps = 0
	if (InRepetitions < 1)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Display,
			TEXT("ModelSpeedTest skipped for uasset \"%s\" for InDeviceType = %s (%d) and InBackEnd = %s (%d)."),
			*InUAssetPath, *DeviceTypeString, (int32)InDeviceType, *BackEndString, (int32)InBackEnd);
		return true;
	}
	// Load Network
	UNeuralNetwork* InOutNetwork = NetworkUassetLoadTest(InUAssetPath);
	// Sanity checks
	if (!InOutNetwork)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("FModelUnitTester::ModelSpeedTest(): InOutNetwork was a nullptr. Path: \"%s\"."),
			*InUAssetPath);
		return false;
	}
	// Save original network state
	const ENeuralDeviceType OriginalDeviceType = InOutNetwork->GetDeviceType();
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const UNeuralNetwork::ENeuralBackEnd OriginalBackEnd = InOutNetwork->GetBackEnd();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	// Set desired back end
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!InOutNetwork->SetBackEnd(InBackEnd))
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("Backend %s is disabled."), *GetBackEndString(InBackEnd));
		return false;
	}
	// Needed variables
	const int64 NetworkSize = InOutNetwork->GetInputTensor().Num();
	TArray<float> InputArray;
	InputArray.Init(1.f, NetworkSize);
	TArray<float> CPUGPUCPUOutput;
	// Run profiling 1 time
	InOutNetwork->SetDeviceType(InDeviceType);
	InOutNetwork->ResetStats();
	const FNNIUnitTesterTimeData TimeData1 = FModelUnitTester_GetTimeInformation(InOutNetwork, CPUGPUCPUOutput, InputArray, 1);

	// Run profiling n times
	InOutNetwork->SetDeviceType(InDeviceType);
	InOutNetwork->ResetStats();
	const FNNIUnitTesterTimeData TimeDataN = FModelUnitTester_GetTimeInformation(InOutNetwork, CPUGPUCPUOutput, InputArray, InRepetitions);

	// Display speed times
	UE_LOG(LogNeuralNetworkInferenceQA, Display,
		TEXT("%s-%s:, Times(msec)-> Inference = %f, Copy In = %f, Copy Out %f, avg(%d times) = Inference = %f, Copy In = %f, Copy Out %f."),
		*BackEndString, *DeviceTypeString, TimeData1.ComputeTimeData.Average, TimeData1.InputCopyTimeData.Average,
		TimeData1.OutputCopyTimeData.Average, InRepetitions, TimeDataN.ComputeTimeData.Average, TimeDataN.InputCopyTimeData.Average,
		TimeDataN.OutputCopyTimeData.Average);

	// Reset to original network state
	InOutNetwork->SetDeviceType(OriginalDeviceType);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InOutNetwork->SetBackEnd(OriginalBackEnd);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	// Test successful
	return true;
}

double FModelUnitTester::GetAveragedL1Norm(const TArray<float>& InArray)
{
	double AveragedL1Norm = 0.;
	for (const float Value : InArray)
	{
		AveragedL1Norm += FMath::Abs(Value);
	}
	AveragedL1Norm /= InArray.Num();
	return AveragedL1Norm;
}

double FModelUnitTester::GetAveragedL1NormDiff(const TArray<float>& InArray1, const TArray<float>& InArray2)
{
	// Sanity check
	if (InArray1.Num() != InArray2.Num())
	{
		ensureMsgf(false, TEXT("InArray1.Num() == InArray2.Num() failed: %d != %d."), InArray1.Num(), InArray2.Num());
		return -1.;
	}
	// Averaged L1 norm
	double AveragedL1NormDiff = 0.;
	for (int32 Index = 0; Index < InArray1.Num(); ++Index)
	{
		AveragedL1NormDiff += FMath::Abs(InArray1[Index] - InArray2[Index]);
	}
	AveragedL1NormDiff /= InArray1.Num();
	return AveragedL1NormDiff;
}

FString FModelUnitTester::GetDeviceTypeString(const ENeuralDeviceType InDeviceType)
{
	if (InDeviceType == ENeuralDeviceType::CPU)
	{
		return TEXT("CPU");
	}
	else if (InDeviceType == ENeuralDeviceType::GPU)
	{
		return TEXT("GPU");
	}
	else
	{
		ensureMsgf(false, TEXT("Unknown DeviceType = %d."), (int32)InDeviceType);
		return TEXT("");
	}
}

FString FModelUnitTester::GetBackEndString(const UNeuralNetwork::ENeuralBackEnd InBackEnd)
{
	if (InBackEnd == UNeuralNetwork::ENeuralBackEnd::UEAndORT)
	{
		return TEXT("UEAndORT");
	}
	else if (InBackEnd == UNeuralNetwork::ENeuralBackEnd::UEOnly)
	{
		return TEXT("UEOnly");
	}
	else
	{
		ensureMsgf(false, TEXT("Unknown BackEndForCurrentPlatform = %d."), (int32)InBackEnd);
		return TEXT("");
	}
}
