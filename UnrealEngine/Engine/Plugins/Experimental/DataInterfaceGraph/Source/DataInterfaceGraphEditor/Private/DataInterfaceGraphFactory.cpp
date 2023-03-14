// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceGraphFactory.h"
#include "DataInterfaceGraph.h"
#include "DataInterfaceGraph_EditorData.h"
#include "DataInterfaceGraph_EdGraph.h"
#include "DataInterfaceGraph_EdGraphSchema.h"
#include "DataInterfaceUncookedOnlyUtils.h"
#include "RigUnit_DataInterfaceBeginExecution.h"
#include "RigUnit_DataInterfaceEndExecution.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "RigVMModel/RigVMController.h"
#include "Units/RigUnit.h"

UDataInterfaceGraphFactory::UDataInterfaceGraphFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UDataInterfaceGraph::StaticClass();
}

bool UDataInterfaceGraphFactory::ConfigureProperties()
{
	return true;
}

UObject* UDataInterfaceGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UDataInterfaceGraph* NewGraph = NewObject<UDataInterfaceGraph>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);

	// Create internal editor data & graphs
	UDataInterfaceGraph_EditorData* EditorData = NewObject<UDataInterfaceGraph_EditorData>(NewGraph, TEXT("EditorData"));
	NewGraph->EditorData = EditorData;
	EditorData->Initialize(/*bRecompileVM*/false);

	// Add initial execution unit
	URigVMController* RootController = EditorData->GetRigVMClient()->GetController(EditorData->RootGraph);
	URigVMUnitNode* MainEntryPointNode = RootController->AddUnitNode(FRigUnit_DataInterfaceBeginExecution::StaticStruct(), FRigUnit::GetMethodName(), FVector2D(-400.0f, 0.0f), FString(), false);
	URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_DataInterfaceBeginExecution, ExecuteContext));
	check(BeginExecutePin);
	check(BeginExecutePin->GetDirection() == ERigVMPinDirection::Output);
	
	// Add function to function lib
	URigVMController* FunctionLibraryController = EditorData->GetRigVMClient()->GetController(EditorData->GetRigVMClient()->GetFunctionLibrary());
	EditorData->EntryPoint = FunctionLibraryController->AddFunctionToLibrary(UE::DataInterfaceGraph::EntryPointName, true, FVector2D::ZeroVector, false, false);
	
	// Add exposed pin for result to function
	URigVMController* EntryPointController = EditorData->GetRigVMClient()->GetOrCreateController(EditorData->EntryPointGraph);
	
	// TODO: using float for now, but needs to be user-driven
	{
		const FString CPPType = TEXT("float");
		const FName CPPTypeObjectPath = NAME_None;
		const FString DefaultValue = TEXT("0.0");
		
		EntryPointController->AddExposedPin(UE::DataInterfaceGraph::ResultName, ERigVMPinDirection::Output, CPPType, CPPTypeObjectPath, DefaultValue, false, false);
	}

	// Add function call
	URigVMFunctionReferenceNode* FunctionCallNode = RootController->AddFunctionReferenceNode(EditorData->EntryPoint, FVector2D::ZeroVector, UE::DataInterfaceGraph::EntryPointName.ToString(), false, false);
	URigVMPin* FunctionExecutePin = FunctionCallNode->FindPin(FRigVMStruct::ExecuteContextName.ToString());
	check(FunctionExecutePin);
	check(FunctionExecutePin->GetDirection() == ERigVMPinDirection::IO);

	// Link entry point to function call
	RootController->AddLink(BeginExecutePin, FunctionExecutePin, false); 
	
	URigVMPin* FunctionResultPin = FunctionCallNode->FindPin(UE::DataInterfaceGraph::ResultName.ToString());
	check(FunctionResultPin);
	check(FunctionResultPin->GetDirection() == ERigVMPinDirection::Output);
	
	// Add end-execution unit of the correct type
	// TODO: using a float for now, but need to use a registry to determine correct type
	URigVMUnitNode* MainExitPointNode = RootController->AddUnitNode(FRigUnit_DataInterfaceEndExecution_Float::StaticStruct(), FRigUnit::GetMethodName(), FVector2D(400.0f, 0.0f), FString(), false);
	URigVMPin* EndExecutePin = MainExitPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_DataInterfaceEndExecution, ExecuteContext));
	check(EndExecutePin);
	check(EndExecutePin->GetDirection() == ERigVMPinDirection::Input);

	URigVMPin* EndExecuteResultPin = MainExitPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_DataInterfaceEndExecution_Float, Result));
	check(EndExecuteResultPin);
	check(EndExecuteResultPin->GetDirection() == ERigVMPinDirection::Input);
	
	// Link function call to exit point 
	RootController->AddLink(FunctionExecutePin, EndExecutePin, false); 

	// Link result pins 
	RootController->AddLink(FunctionResultPin, EndExecuteResultPin, false); 
	
	// Compile the initial skeleton
	UE::DataInterfaceGraphUncookedOnly::FUtils::Compile(NewGraph);
	check(!EditorData->bErrorsDuringCompilation);
	
	return NewGraph;
}