// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleFactory.h"
#include "Scheduler/AnimNextSchedule.h"

UAnimNextScheduleFactory::UAnimNextScheduleFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimNextSchedule::StaticClass();
}

bool UAnimNextScheduleFactory::ConfigureProperties()
{
	return true;
}

UObject* UAnimNextScheduleFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UAnimNextSchedule* NewSchedule = NewObject<UAnimNextSchedule>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);

	return NewSchedule;
}