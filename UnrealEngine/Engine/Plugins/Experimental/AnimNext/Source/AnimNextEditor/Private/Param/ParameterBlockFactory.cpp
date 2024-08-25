// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParameterBlockFactory.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/AnimNextParameterExecuteContext.h"
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
	EObjectFlags FlagsToUse = Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted;
	if(InParent == GetTransientPackage())
	{
		FlagsToUse &= ~RF_Standalone;
	}

	UAnimNextParameterBlock* NewBlock = NewObject<UAnimNextParameterBlock>(InParent, Class, Name, FlagsToUse);

	UAnimNextParameterBlock_EditorData* EditorData = NewObject<UAnimNextParameterBlock_EditorData>(NewBlock, TEXT("EditorData"), RF_Transactional);
	NewBlock->EditorData = EditorData;
	EditorData->Initialize(/*bRecompileVM*/false);

	// Compile the initial skeleton
	UE::AnimNext::UncookedOnly::FUtils::Compile(NewBlock);
	check(!EditorData->bErrorsDuringCompilation);
	
	return NewBlock;
}