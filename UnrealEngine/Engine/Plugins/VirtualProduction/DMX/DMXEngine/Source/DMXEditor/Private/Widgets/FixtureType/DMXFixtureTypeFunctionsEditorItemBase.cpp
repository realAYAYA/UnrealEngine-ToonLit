// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXFixtureTypeFunctionsEditorItemBase.h"

#include "DMXEditor.h"
#include "Library/DMXEntityFixtureType.h"


FDMXFixtureTypeFunctionsEditorItemBase::FDMXFixtureTypeFunctionsEditorItemBase(const TSharedRef<FDMXEditor>& InDMXEditor, TWeakObjectPtr<UDMXEntityFixtureType> InFixtureType, int32 InModeIndex)
	: FixtureType(InFixtureType)
	, ModeIndex(InModeIndex)
	, SharedData(InDMXEditor->GetFixtureTypeSharedData())
	, WeakDMXEditor(InDMXEditor)
{
	ensureMsgf(SharedData.IsValid(), TEXT("FixtureTypeSharedData not valid when trying to create a FDMXFixtureTypeFunctionsEditorItemBase"));
}

int32 FDMXFixtureTypeFunctionsEditorItemBase::GetModeIndex() const
{
	if (ensureMsgf(FixtureType.IsValid() && FixtureType->Modes.IsValidIndex(ModeIndex), TEXT("Invalid Fixture Type, Mode or Function in FDMXFixtureTypeFunctionsEditorFunctionItem.")))
	{
		return ModeIndex;
	}

	return INDEX_NONE;
}
