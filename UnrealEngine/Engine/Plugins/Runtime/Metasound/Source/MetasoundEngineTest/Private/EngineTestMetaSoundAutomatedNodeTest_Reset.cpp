// Copyright Epic Games, Inc. All Rights Reserved.

#include "Algo/AllOf.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EngineTestMetaSoundAutomatedNodeTest.h"
#include "Misc/AutomationTest.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundLog.h"
#include "MetasoundVertexData.h"
#include "Templates/UniquePtr.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#if WITH_EDITORONLY_DATA


IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMetasoundAutomatedNodeTest_Reset, "Audio.Metasound.AutomatedNodeTest.Reset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FMetasoundAutomatedNodeTest_Reset::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace Metasound::EngineTest;
	GetAllRegisteredNativeNodes(OutBeautifiedNames, OutTestCommands);

	UE_LOG(LogMetaSound, Verbose, TEXT("Found %d metasound nodes to test"), OutTestCommands.Num());
}

bool FMetasoundAutomatedNodeTest_Reset::RunTest(const FString& InRegistryKeyString)
{
	using namespace Metasound;
	using namespace Metasound::EngineTest;
	using namespace Metasound::Frontend;

	static const FOperatorSettings OperatorSettings{48000  /* samplerate */, 100.f /* block rate */};
	static const FMetasoundEnvironment SourceEnvironment = GetSourceEnvironmentForTest();

	FNodeRegistryKey RegistryKey;
	if (!FNodeRegistryKey::Parse(InRegistryKeyString, RegistryKey))
	{
		AddError(FString::Printf(TEXT("Failed to parse registry key string %s"), *InRegistryKeyString));
		return false;
	}

	TUniquePtr<INode> Node = CreateNodeFromRegistry(RegistryKey);
	if (!Node.IsValid())
	{
		AddError(FString::Printf(TEXT("Failed to create node %s from registry"), *InRegistryKeyString));
		return false;
	}

	// Populate inputs to node. 
	FInputVertexInterface InputInterface = Node->GetVertexInterface().GetInputInterface();
	FInputVertexInterfaceData InputVertexData(InputInterface);
	CreateDefaultsAndVariables(OperatorSettings, InputVertexData);

	FInputVertexDataTestController InputTester(OperatorSettings, InputInterface, InputVertexData);

	// Create operator 
	FBuildOperatorParams BuildParams
	{
		*Node,
		OperatorSettings,
		InputVertexData,
		SourceEnvironment
	};

	IOperator::FResetParams ResetParams
	{
		OperatorSettings,
		SourceEnvironment
	};

	// Convenience function for testing entire lifecycle of an individual operator 
	// with a variety of inputs.
	auto RunTestIteration = [&]()
	{
		// Create the operator from the node factory
		FBuildResults BuildResults;
		TUniquePtr<IOperator> Operator = Node->GetDefaultOperatorFactory()->CreateOperator(BuildParams, BuildResults);

		if (!Operator.IsValid())
		{
			AddError(FString::Printf(TEXT("Failed to create operator from node %s - %s."), *InRegistryKeyString, *GetPrettyName(RegistryKey)));
		}

		// Store a copy of the input values data values so the inputs
		// can be reset to this value later during the test.
		FInterfaceState InitialInputState = InputTester.GetInterfaceState();

		// Bind to inputs and output data of operator.
		FVertexInterface VertexInterface = Node->GetVertexInterface();
		FVertexInterfaceData VertexInterfaceData{VertexInterface};
		Operator->BindInputs(VertexInterfaceData.GetInputs());
		Operator->BindOutputs(VertexInterfaceData.GetOutputs());

		// Create output tester which will analyzer outputs of operator.
		FOutputVertexDataTestController OutputTester{VertexInterface.GetOutputInterface(), VertexInterfaceData.GetOutputs()};
		
		// Capture current state of outputs so they can be referenced at a later time.
		// The captured values are held within the OutputTester.
		OutputTester.CaptureCurrentOutputValues();

		IOperator::FExecuteFunction OpExecFunc = Operator->GetExecuteFunction();
		auto ConditionallyCallExecFunc = [&OpExecFunc] (IOperator* Operator)
		{
			if (OpExecFunc)
			{
				OpExecFunc(Operator);
			}
		};
		IOperator::FPostExecuteFunction OpPostExecFunc = Operator->GetPostExecuteFunction();
		auto ConditionallyCallPostExecFunc = [&OpPostExecFunc] (IOperator* Operator)
        {
        	if (OpPostExecFunc)
        	{
        		OpPostExecFunc(Operator);
        	}
        };
		IOperator::FResetFunction OpResetFunc = Operator->GetResetFunction();
		
		// Test execute function with input variations
		if (OpExecFunc || OpPostExecFunc)
		{
			ConditionallyCallExecFunc(Operator.Get());
			ConditionallyCallPostExecFunc(Operator.Get());
			
			if (InputTester.GetNumMutableInputs() > 0)
			{
				InputTester.SetMutableInputsToDefault();
				ConditionallyCallExecFunc(Operator.Get());
				ConditionallyCallPostExecFunc(Operator.Get());
				InputTester.SetMutableInputsToMin();
				ConditionallyCallExecFunc(Operator.Get());
				ConditionallyCallPostExecFunc(Operator.Get());
				InputTester.SetMutableInputsToMax();
				ConditionallyCallExecFunc(Operator.Get());
				ConditionallyCallPostExecFunc(Operator.Get());
				InputTester.SetMutableInputsToRandom();
				ConditionallyCallExecFunc(Operator.Get());
				ConditionallyCallPostExecFunc(Operator.Get());
			}
		}

		// Return inputs to initial state. 
		InputTester.SetMutableInputsToState(InitialInputState);

		if (OpResetFunc)
		{
			OpResetFunc(Operator.Get(), ResetParams);
		}
		else if (OpExecFunc || OpPostExecFunc)
		{
			AddError(FString::Printf(TEXT("Missing Reset(...) function when execute function exists for node %s - %s"), *InRegistryKeyString, *GetPrettyName(RegistryKey)));
		}

		// Check that after returning all inputs to their original state and calling
		// reset on the operator, that all output values have returned to their initail state. 
		if (!OutputTester.AreAllOutputValuesEqualToCapturedValues())
		{
			AddError(FString::Printf(TEXT("Reset(...) function resulted in different starting conditions for node %s - %s"), *InRegistryKeyString, *GetPrettyName(RegistryKey)));
		}
	};

	// Test entire operator lifecycle with different starting conditions if
	// any of the inputs are mutable
	InputTester.SetMutableInputsToDefault();
	RunTestIteration();

	if (InputTester.GetNumMutableInputs() > 0)
	{
		InputTester.SetMutableInputsToMin();
		RunTestIteration();
		InputTester.SetMutableInputsToMax();
		RunTestIteration();
		InputTester.SetMutableInputsToRandom();
		RunTestIteration();
	}

	return true;
}

#endif // WITH_EDITORONLY_DATA

#endif //WITH_DEV_AUTOMATION_TESTS

