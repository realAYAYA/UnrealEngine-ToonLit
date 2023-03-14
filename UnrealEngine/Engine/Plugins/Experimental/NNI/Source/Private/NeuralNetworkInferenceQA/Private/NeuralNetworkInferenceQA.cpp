// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralNetworkInferenceQA.h"
#include "NeuralNetworkInferenceQAUtils.h"
#include "UnitTester.h"
#include "Misc/Paths.h"



/* UNeuralNetworkInferenceQA static functions
 *****************************************************************************/

bool UNeuralNetworkInferenceQA::UnitTesting()
{
	const FString ProjectContentDir = FPaths::ProjectContentDir();
	const FString MachineLearningTestsRelativeDirectory = TEXT("Tests/MachineLearning/");
	const FString ModelZooRelativeDirectory = MachineLearningTestsRelativeDirectory / TEXT("Models/"); // Eg "Tests/MachineLearning/Models/"
	const FString UnitTestRelativeDirectory = MachineLearningTestsRelativeDirectory / TEXT("UnitTests/"); // Eg "Tests/MachineLearning/UnitTests/"
	const bool bDidGlobalTestPassed = FUnitTester::GlobalTest(ProjectContentDir, ModelZooRelativeDirectory, UnitTestRelativeDirectory);
	ensureMsgf(bDidGlobalTestPassed, TEXT("UNeuralNetworkInferenceQA::UnitTesting() returned false. See log above."));
	return bDidGlobalTestPassed;
}



#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeuralNetworkInferenceTest, "System.Engine.MachineLearning.NeuralNetworkInference (NNI)",
	EAutomationTestFlags::ApplicationContextMask // = EditorContext | ClientContext | ServerContext | CommandletContext
	| EAutomationTestFlags::FeatureMask // = NonNullRHI | RequiresUser
	| EAutomationTestFlags::EngineFilter)

bool FNeuralNetworkInferenceTest::RunTest(const FString& Parameters)
{
	if (!UNeuralNetworkInferenceQA::UnitTesting())
	{
		AddError(TEXT("FNeuralNetworkInferenceTest::RunTest(): UNeuralNetworkInferenceQA::UnitTesting() returned false. See log above.")); // Both AddWarning/AddError would work
	}
	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
