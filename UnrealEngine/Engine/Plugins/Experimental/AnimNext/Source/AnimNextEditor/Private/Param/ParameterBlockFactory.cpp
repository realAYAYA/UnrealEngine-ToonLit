﻿// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParameterBlockFactory.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/ParametersExecuteContext.h"
#include "UncookedOnlyUtils.h"

UAnimNextParameterBlockFactory::UAnimNextParameterBlockFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimNextParameterBlock::StaticClass();
}

bool UAnimNextParameterBlockFactory::ConfigureProperties()
{
	return true;
}

UObject* UAnimNextParameterBlockFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UAnimNextParameterBlock* NewBlock = NewObject<UAnimNextParameterBlock>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);

	UAnimNextParameterBlock_EditorData* EditorData = NewObject<UAnimNextParameterBlock_EditorData>(NewBlock, TEXT("EditorData"), RF_Transactional);
	NewBlock->EditorData = EditorData;
	EditorData->Initialize(/*bRecompileVM*/false);
	EditorData->GetRigVMClient()->SetExecuteContextStruct(FAnimNextParametersExecuteContext::StaticStruct());

	// Compile the initial skeleton
	UE::AnimNext::UncookedOnly::FUtils::Compile(NewBlock);
	check(!EditorData->bErrorsDuringCompilation);
	
	return NewBlock;
}