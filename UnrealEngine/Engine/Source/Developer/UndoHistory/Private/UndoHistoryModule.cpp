// Copyright Epic Games, Inc. All Rights Reserved.

#include "UndoHistoryModule.h"

#define LOCTEXT_NAMESPACE "FUndoHistoryModule"

void FUndoHistoryModule::StartupModule()
{}

void FUndoHistoryModule::ShutdownModule()
{}

bool FUndoHistoryModule::SupportsDynamicReloading()
{
	return true;
}


IMPLEMENT_MODULE(FUndoHistoryModule, UndoHistory);


#undef LOCTEXT_NAMESPACE
