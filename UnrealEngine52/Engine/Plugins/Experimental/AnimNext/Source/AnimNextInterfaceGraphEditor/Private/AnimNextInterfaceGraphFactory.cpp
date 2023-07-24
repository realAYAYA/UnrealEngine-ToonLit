// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceGraphFactory.h"
#include "AnimNextInterfaceGraph.h"
#include "AnimNextInterfaceGraph_EditorData.h"
#include "AnimNextInterfaceGraph_EdGraph.h"
#include "AnimNextInterfaceGraph_EdGraphSchema.h"
#include "AnimNextInterfaceUncookedOnlyUtils.h"
#include "RigUnit_AnimNextInterfaceBeginExecution.h"
#include "RigUnit_AnimNextInterfaceEndExecution.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "RigVMModel/RigVMController.h"
#include "Units/RigUnit.h"

UAnimNextInterfaceGraphFactory::UAnimNextInterfaceGraphFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimNextInterfaceGraph::StaticClass();
}

bool UAnimNextInterfaceGraphFactory::ConfigureProperties()
{
	return true;
}

UObject* UAnimNextInterfaceGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UAnimNextInterfaceGraph* NewGraph = NewObject<UAnimNextInterfaceGraph>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);

	// Create internal editor data & graphs
	UAnimNextInterfaceGraph_EditorData* EditorData = NewObject<UAnimNextInterfaceGraph_EditorData>(NewGraph, TEXT("EditorData"));
	NewGraph->EditorData = EditorData;
	EditorData->Initialize(/*bRecompileVM*/false);
	EditorData->GetRigVMClient()->SetExecuteContextStruct(FAnimNextInterfaceExecuteContext::StaticStruct());

	// Add initial execution unit
	URigVMController* Controller = EditorData->GetRigVMClient()->GetController(EditorData->RootGraph);

	URigVMUnitNode* MainEntryPointNode = Controller->AddUnitNode(FRigUnit_AnimNextInterfaceBeginExecution::StaticStruct(), FRigUnit::GetMethodName(), FVector2D(-400.0f, 0.0f), FString(), false);
	URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextInterfaceBeginExecution, ExecuteContext));
	check(BeginExecutePin);
	check(BeginExecutePin->GetDirection() == ERigVMPinDirection::Output);

	//// Add function to function lib
	//{
	//	FRigVMControllerGraphGuard LibraryGraphGuard(Controller, EditorData->GetRigVMClient()->GetFunctionLibrary());
	//	EditorData->EntryPoint = Controller->AddFunctionToLibrary(UE::AnimNext::InterfaceGraph::EntryPointName, true, FVector2D::ZeroVector, false, false);

	//	// Add exposed pin for result to function
	//	{
	//		FRigVMControllerGraphGuard FunctionGraphGuard(Controller, EditorData->EntryPoint->GetContainedGraph());

	//		// TODO: using float for now, but needs to be user-driven
	//		const FString CPPType = TEXT("float");
	//		const FName CPPTypeObjectPath = NAME_None;
	//		const FString DefaultValue = TEXT("0.0");

	//		//FunctionController->AddExposedPin(UE::AnimNext::InterfaceGraph::ResultName, ERigVMPinDirection::Output, CPPType, CPPTypeObjectPath, DefaultValue, false, false);
	//		Controller->AddExposedPin(UE::AnimNext::InterfaceGraph::ResultName, ERigVMPinDirection::Output, CPPType, CPPTypeObjectPath, DefaultValue, false, false);
	//	}
	//}

	//// Add function call
	//URigVMFunctionReferenceNode* FunctionCallNode = Controller->AddFunctionReferenceNode(EditorData->EntryPoint, FVector2D::ZeroVector, UE::AnimNext::InterfaceGraph::EntryPointName.ToString(), false, false);
	//URigVMPin* FunctionExecutePin = FunctionCallNode->FindPin(FRigVMStruct::ExecuteContextName.ToString());
	//check(FunctionExecutePin);
	//check(FunctionExecutePin->GetDirection() == ERigVMPinDirection::IO);

	//// Link entry point to function call
	//Controller->AddLink(BeginExecutePin, FunctionExecutePin, false);

	//URigVMPin* FunctionResultPin = FunctionCallNode->FindPin(UE::AnimNext::InterfaceGraph::ResultName.ToString());
	//check(FunctionResultPin);
	//check(FunctionResultPin->GetDirection() == ERigVMPinDirection::Output);

	//// Add end-execution unit of the correct type
	//// TODO: using a float for now, but need to use a registry to determine correct type
	//URigVMUnitNode* MainExitPointNode = Controller->AddUnitNode(FRigUnit_AnimNextInterfaceEndExecution_Float::StaticStruct(), FRigUnit::GetMethodName(), FVector2D(400.0f, 0.0f), FString(), false);
	//URigVMPin* EndExecutePin = MainExitPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextInterfaceEndExecution, ExecuteContext));
	//check(EndExecutePin);
	//check(EndExecutePin->GetDirection() == ERigVMPinDirection::Input);

	//URigVMPin* EndExecuteResultPin = MainExitPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextInterfaceEndExecution_Float, Result));
	//check(EndExecuteResultPin);
	//check(EndExecuteResultPin->GetDirection() == ERigVMPinDirection::Input);

	//// Link function call to exit point 
	//Controller->AddLink(FunctionExecutePin, EndExecutePin, false);

	//// Link result pins 
	//Controller->AddLink(FunctionResultPin, EndExecuteResultPin, false);

	// Compile the initial skeleton
	UE::AnimNext::InterfaceGraphUncookedOnly::FUtils::Compile(NewGraph);
	check(!EditorData->bErrorsDuringCompilation);
	
	return NewGraph;
}