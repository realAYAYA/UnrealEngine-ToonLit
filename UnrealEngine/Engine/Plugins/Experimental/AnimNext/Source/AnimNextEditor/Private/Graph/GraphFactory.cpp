// Copyright Epic Games, Inc. All Rights Reserved.

#include "GraphFactory.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "UncookedOnlyUtils.h"
#include "Graph/RigUnit_AnimNextBeginExecution.h"
#include "Graph/RigUnit_AnimNextEndExecution.h"
#include "RigVMModel/RigVMController.h"
#include "Units/RigUnit.h"

UAnimNextGraphFactory::UAnimNextGraphFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimNextGraph::StaticClass();
}

bool UAnimNextGraphFactory::ConfigureProperties()
{
	return true;
}

UObject* UAnimNextGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UAnimNextGraph* NewGraph = NewObject<UAnimNextGraph>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);

	// Create internal editor data & graphs
	UAnimNextGraph_EditorData* EditorData = NewObject<UAnimNextGraph_EditorData>(NewGraph, TEXT("EditorData"));
	NewGraph->EditorData = EditorData;
	EditorData->Initialize(/*bRecompileVM*/false);
	EditorData->GetRigVMClient()->SetExecuteContextStruct(FAnimNextGraphExecuteContext::StaticStruct());

	// Add initial execution unit
	URigVMController* Controller = EditorData->GetRigVMClient()->GetController(EditorData->RootGraph);

	URigVMUnitNode* MainEntryPointNode = Controller->AddUnitNode(FRigUnit_AnimNextBeginExecution::StaticStruct(), FRigUnit::GetMethodName(), FVector2D(-400.0f, 0.0f), FString(), false);
	URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextBeginExecution, ExecuteContext));
	check(BeginExecutePin);
	check(BeginExecutePin->GetDirection() == ERigVMPinDirection::Output);

	//// Add function to function lib
	//{
	//	FRigVMControllerGraphGuard LibraryGraphGuard(Controller, EditorData->GetRigVMClient()->GetFunctionLibrary());
	//	EditorData->EntryPoint = Controller->AddFunctionToLibrary(UE::AnimNext::Graph::EntryPointName, true, FVector2D::ZeroVector, false, false);

	//	// Add exposed pin for result to function
	//	{
	//		FRigVMControllerGraphGuard FunctionGraphGuard(Controller, EditorData->EntryPoint->GetContainedGraph());

	//		// TODO: using float for now, but needs to be user-driven
	//		const FString CPPType = TEXT("float");
	//		const FName CPPTypeObjectPath = NAME_None;
	//		const FString DefaultValue = TEXT("0.0");

	//		//FunctionController->AddExposedPin(UE::AnimNext::Graph::ResultName, ERigVMPinDirection::Output, CPPType, CPPTypeObjectPath, DefaultValue, false, false);
	//		Controller->AddExposedPin(UE::AnimNext::Graph::ResultName, ERigVMPinDirection::Output, CPPType, CPPTypeObjectPath, DefaultValue, false, false);
	//	}
	//}

	//// Add function call
	//URigVMFunctionReferenceNode* FunctionCallNode = Controller->AddFunctionReferenceNode(EditorData->EntryPoint, FVector2D::ZeroVector, UE::AnimNext::Graph::EntryPointName.ToString(), false, false);
	//URigVMPin* FunctionExecutePin = FunctionCallNode->FindPin(FRigVMStruct::ExecuteContextName.ToString());
	//check(FunctionExecutePin);
	//check(FunctionExecutePin->GetDirection() == ERigVMPinDirection::IO);

	//// Link entry point to function call
	//Controller->AddLink(BeginExecutePin, FunctionExecutePin, false);

	//URigVMPin* FunctionResultPin = FunctionCallNode->FindPin(UE::AnimNext::Graph::ResultName.ToString());
	//check(FunctionResultPin);
	//check(FunctionResultPin->GetDirection() == ERigVMPinDirection::Output);

	//// Add end-execution unit of the correct type
	//// TODO: using a float for now, but need to use a registry to determine correct type
	//URigVMUnitNode* MainExitPointNode = Controller->AddUnitNode(FRigUnit_AnimNextEndExecution_Float::StaticStruct(), FRigUnit::GetMethodName(), FVector2D(400.0f, 0.0f), FString(), false);
	//URigVMPin* EndExecutePin = MainExitPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextEndExecution, ExecuteContext));
	//check(EndExecutePin);
	//check(EndExecutePin->GetDirection() == ERigVMPinDirection::Input);

	//URigVMPin* EndExecuteResultPin = MainExitPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextEndExecution_Float, Result));
	//check(EndExecuteResultPin);
	//check(EndExecuteResultPin->GetDirection() == ERigVMPinDirection::Input);

	//// Link function call to exit point 
	//Controller->AddLink(FunctionExecutePin, EndExecutePin, false);

	//// Link result pins 
	//Controller->AddLink(FunctionResultPin, EndExecuteResultPin, false);

	// Compile the initial skeleton
	UE::AnimNext::UncookedOnly::FUtils::Compile(NewGraph);
	check(!EditorData->bErrorsDuringCompilation);
	
	return NewGraph;
}