// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralNetworkInferenceQAAsset.h"
#include "NeuralNetworkInferenceQAUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"


/* FNeuralNetworkInferenceQAOperatorTestAsset public functions
 *****************************************************************************/

void FNeuralNetworkInferenceQAOperatorTestAsset::AddInputTensors(const UNeuralNetwork* const InNetwork)
{
	for (int32 InputIndex = 0; InputIndex < InNetwork->GetInputTensorNumber(); ++InputIndex)
	{
		InputTensors.Push(InNetwork->GetInputTensor(InputIndex));
	}
}

void FNeuralNetworkInferenceQAOperatorTestAsset::AddOutputTensors(const UNeuralNetwork* const InNetwork)
{
	for (int32 OutputIndex = 0; OutputIndex < InNetwork->GetOutputTensorNumber(); ++OutputIndex)
	{
		OutputTensors.Push(InNetwork->GetOutputTensor(OutputIndex));
	}
}

bool FNeuralNetworkInferenceQAOperatorTestAsset::CompareAverageL1DiffNewVsPreviousTests(
	const FNeuralNetworkInferenceQAOperatorTestAsset& InOperatorTestAsset1, const FNeuralNetworkInferenceQAOperatorTestAsset& InOperatorTestAsset2,
	const FString& InOperatorName)
{
	// Compare each input and output
	return (CompareAverageL1DiffNewVsPreviousTests(InOperatorTestAsset1.InputTensors, InOperatorTestAsset2.InputTensors, InOperatorName)
		&& CompareAverageL1DiffNewVsPreviousTests(InOperatorTestAsset1.OutputTensors, InOperatorTestAsset2.OutputTensors, InOperatorName));
}

bool FNeuralNetworkInferenceQAOperatorTestAsset::CompareAverageL1DiffNewVsPreviousTests(const TArray<FNeuralTensor>& InTensorsA,
	const TArray<FNeuralTensor>& InTensorsB, const FString& InOperatorName)
{
	// Number of inputs and outputs should be the same
	if (InTensorsA.Num() != InTensorsB.Num())
	{
		return false;
	}
	// Compare each input
	const FString DebugName = InOperatorName + TEXT("-CPUvsGT");
	for (int32 TensorIndex = 0; TensorIndex < InTensorsA.Num(); ++TensorIndex)
	{
		if (!FNeuralNetworkInferenceQAUtils::EstimateTensorL1DiffError(InTensorsA[TensorIndex], InTensorsB[TensorIndex], /*ZeroThreshold*/5e-4,
		DebugName))
		{
			return false;
		}
	}
	return true;
}



/* FNeuralNetworkInferenceQAOperatorAsset public functions
 *****************************************************************************/

FString FNeuralNetworkInferenceQAOperatorAsset::RunNetworkCPUAndGetString(UNeuralNetwork* InOutNetwork)
{
	// Create input string and InputTensorArray
	const TArray<FNeuralTensor> InputTensorArray = FNeuralNetworkInferenceQAUtils::CreateInputArrayCopy(InOutNetwork);
	FString TensorsAsString = TEXT("Input(s):\n");
	for (int32 InputIndex = 0; InputIndex < InOutNetwork->GetInputTensorNumber(); ++InputIndex)
	{
		TensorsAsString += InOutNetwork->GetInputTensor(InputIndex).ToString() + TEXT("\n");
	}
	// Run network
	InOutNetwork->SetDeviceType(ENeuralDeviceType::CPU);
	InOutNetwork->Run();
	// Create output string
	TensorsAsString += TEXT("\nOutput(s):\n");
	for (int32 OutputIndex = 0; OutputIndex < InOutNetwork->GetOutputTensorNumber(); ++OutputIndex)
	{
		TensorsAsString += InOutNetwork->GetOutputTensor(OutputIndex).ToString() + TEXT("\n");
	}
	// Reset memory
	FNeuralNetworkInferenceQAUtils::SetInputFromArrayCopy(InOutNetwork, InputTensorArray); // This is doing an unnecessary copy (it should move)
	// Return TensorsAsString
	return TensorsAsString + TEXT("\n\n\n");
}

void FNeuralNetworkInferenceQAOperatorAsset::RunAndAddTest(UNeuralNetwork* InOutNetwork)
{
	// Resize NewTests
	NewTests.Emplace();

	FNeuralNetworkInferenceQAOperatorTestAsset& NewTest = NewTests.Last();
	NewTestsString += RunNetworkCPUAndGetString(InOutNetwork);
	NewTest.AddInputTensors(InOutNetwork);
	NewTest.AddOutputTensors(InOutNetwork);
}

bool FNeuralNetworkInferenceQAOperatorAsset::CompareNewVsPreviousTests(const FString& InGroundTruthDirectory, const FString& InOperatorName)
{
	bool bWasComparisonSuccessful = true;
	// If PreviousTests is empty --> It is the first time it is being used
	if (PreviousTests.IsEmpty() || PreviousTests.Num() != NewTests.Num())
	{
		if (PreviousTests.IsEmpty())
		{
			UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("First time that this test is generated, no previous results exist."));
		}
		else
		{
			UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("New tests added or removed, PreviousTests.Num() = %d and NewTests.Num() = %d."),
				PreviousTests.Num(), NewTests.Num());
		}
		bWasComparisonSuccessful = false;
	}
	// Everything has to pass to be a successful comparison
	else // if (PreviousTests.Num() == NewTests.Num())
	{
		for (int32 TestIndex = 0; TestIndex < PreviousTests.Num(); ++TestIndex)
		{
			// Easy but CPU-and-compiler dependent
			if (PreviousTestsString != NewTestsString)
			{
				// Even if PreviousTestsString != NewTestsString, it might be a floating value precision error
				const FNeuralNetworkInferenceQAOperatorTestAsset& Test = PreviousTests[TestIndex];
				const FNeuralNetworkInferenceQAOperatorTestAsset& UpdatedTest = NewTests[TestIndex];
				bWasComparisonSuccessful &= FNeuralNetworkInferenceQAOperatorTestAsset::CompareAverageL1DiffNewVsPreviousTests(Test, UpdatedTest,
					InOperatorName);
			}
		}
	}
	// Save on disk
	if (!bWasComparisonSuccessful)
	{
		const FString BaseTestPath = InGroundTruthDirectory / TEXT("temp_") + InOperatorName;
		const FString GroundTruthFileExtension = TEXT(".txt");
		const FString FilePathPreviousTest = BaseTestPath + TEXT("_previous") + GroundTruthFileExtension;
		const FString FilePathNewTest = BaseTestPath + TEXT("_new") + GroundTruthFileExtension;
		ensureMsgf(FFileHelper::SaveStringToFile(PreviousTestsString, *FilePathPreviousTest), TEXT("FFileHelper::SaveStringToFile returned false."));
		ensureMsgf(FFileHelper::SaveStringToFile(NewTestsString, *FilePathNewTest), TEXT("FFileHelper::SaveStringToFile returned false."));
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("FNeuralNetworkInferenceQAOperatorAsset::CompareNewVsPreviousTests(): Mismatch between"
			" expected and actual results, they should match. Check the following files for differences:\n"
			"\t- Character length (saved previous vs. new): %d vs. %d\n"
			"\t- Previous results saved in %s\n"
			"\t- New results saved in %s\n"),
			PreviousTestsString.Len(), NewTestsString.Len(),
			*IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FilePathPreviousTest),
			*IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FilePathNewTest));
		// Final swap
		Swap(PreviousTests, NewTests);
		Swap(PreviousTestsString, NewTestsString);
	}
	// Flush
	FlushNewTests();
	// Return bWasComparisonSuccessful
	return bWasComparisonSuccessful;
}

void FNeuralNetworkInferenceQAOperatorAsset::FlushNewTests()
{
	NewTests.Empty();
	NewTestsString.Empty();
}



/* UNeuralNetworkInferenceQAAsset functions
 *****************************************************************************/

void UNeuralNetworkInferenceQAAsset::FindOrAddOperators(const TArray<FString>& InOperatorNames)
{
	FString NotFoundOperatorNames;
	for (const FString& OperatorName : InOperatorNames)
	{
		if (!Operators.Find(OperatorName))
		{
			NotFoundOperatorNames += OperatorName + TEXT(", ");
			Operators.Add(OperatorName, FNeuralNetworkInferenceQAOperatorAsset());
			UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("QA for operator %s was not found on UNeuralNetworkInferenceQAAsset, added!"),
				*OperatorName);
		}
	}
	if (NotFoundOperatorNames.Len() > 0)
	{
		NotFoundOperatorNames.LeftChopInline(2); // Removes the last TEXT(", ")
		ensureMsgf(false, TEXT("Some operators are new and were not found on UNeuralNetworkInferenceQAAsset, they have been added: %s."),
			*NotFoundOperatorNames);
	}
}

void UNeuralNetworkInferenceQAAsset::RunAndAddTest(UNeuralNetwork* InOutNetwork, const FString& OperatorName)
{
	Operators[OperatorName].RunAndAddTest(InOutNetwork);
}

bool UNeuralNetworkInferenceQAAsset::CompareNewVsPreviousTests(const FString& InGroundTruthDirectory)
{
	bool bWasComparisonSuccessful = true;
	for (auto& OperatorPair : Operators)
	{
		// No break so we can return all wrong results at once
		if (!OperatorPair.Value.CompareNewVsPreviousTests(InGroundTruthDirectory, OperatorPair.Key))
		{
			bWasComparisonSuccessful = false;
		}
	}
	if (!bWasComparisonSuccessful)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("UNeuralNetworkInferenceQAAsset::CompareNewVsPreviousTests(): Mismatch between expected"
			" and actual results, they should match. Check the previous warning messages."));
	}
	return bWasComparisonSuccessful;
}

void UNeuralNetworkInferenceQAAsset::FlushNewTests()
{
	for (auto& OperatorPair : Operators)
	{
		OperatorPair.Value.FlushNewTests();
	}
}

const FString UNeuralNetworkInferenceQAAsset_FileName = TEXT("NeuralNetworkInferenceQAAsset");

UNeuralNetworkInferenceQAAsset* UNeuralNetworkInferenceQAAsset::Load(const FString& InNeuralNetworkInferenceQAAssetParentDirectoryName,
	const FString& InNeuralNetworkInferenceQAAssetName)
{
	// Load NeuralNetworkInferenceQAAsset from disk
	// E.g., "NeuralNetworkInferenceQAAsset'/Game/[UNIT_TEST_DIR]/NeuralNetworkInferenceQAAsset.NeuralNetworkInferenceQAAsset'"
	const FString FilePath = UNeuralNetworkInferenceQAAsset_FileName
		+ TEXT("'/Game/") / InNeuralNetworkInferenceQAAssetParentDirectoryName / UNeuralNetworkInferenceQAAsset_FileName + TEXT(".")
		+ UNeuralNetworkInferenceQAAsset_FileName + TEXT("'");
	UNeuralNetworkInferenceQAAsset* NeuralNetworkInferenceQAAsset = LoadObject<UNeuralNetworkInferenceQAAsset>((UObject*)GetTransientPackage(),
		*FilePath);
	// Warning if it does not exist yet
	if (!NeuralNetworkInferenceQAAsset)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Display,
			TEXT("NeuralNetworkInferenceQAAsset not found in %s. Please, create it first or make sure the right path is being used."), *FilePath);
		return nullptr;
	}
	// Return NeuralNetworkInferenceQAAsset
	return NeuralNetworkInferenceQAAsset;
}

bool UNeuralNetworkInferenceQAAsset::Save()
{
	// Flush
	FlushNewTests();
	// Extract Package
	UPackage* Package = GetOutermost();
	if (!Package)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("Package was a nullptr!"));
		return false;
	}
	Package->FullyLoad();
	// Save Uasset
	FString FilePath;
	{
		FString ParentPath, FileName, FileExtension;
		// E.g., "../../../Sandbox/MachineLearning/NNIUnitTest/Content/NeuralNetworkInferenceQAAsset.uasset"
		FPaths::Split(Package->GetLoadedPath().GetLocalFullPath(), ParentPath, FileName, FileExtension);
		// E.g., "../../../Sandbox/MachineLearning/NNIUnitTest/Content/NeuralNetworkInferenceQAAsset_new.uasset"
		FilePath = ParentPath / UNeuralNetworkInferenceQAAsset_FileName + TEXT("_new.") + FileExtension;
	}
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Standalone;
	const bool bWasPackageSaved = UPackage::SavePackage(Package, /*InNeuralNetworkInferenceQAAsset*/nullptr, *FilePath, SaveArgs);
	if (!bWasPackageSaved)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("UNeuralNetworkInferenceQAAsset::Save() failed for %s!"), *FilePath);
	}
	// Return success bool
	return bWasPackageSaved;
}
