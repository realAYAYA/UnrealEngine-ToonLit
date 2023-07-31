// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPIEManager.h"

#include "DMXEditorUtils.h"

#include "Editor.h"


FDMXPIEManager::FDMXPIEManager()
{
	FEditorDelegates::BeginPIE.AddRaw(this, &FDMXPIEManager::OnBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FDMXPIEManager::OnEndPIE);
}

FDMXPIEManager::~FDMXPIEManager()
{
	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
}

void FDMXPIEManager::OnBeginPIE(const bool bIsSimulating)
{
	FDMXEditorUtils::ClearAllDMXPortBuffers();
	FDMXEditorUtils::ClearFixturePatchCachedData();
}

void FDMXPIEManager::OnEndPIE(const bool bIsSimulating)
{
	FDMXEditorUtils::ClearAllDMXPortBuffers();
	FDMXEditorUtils::ClearFixturePatchCachedData();
}
