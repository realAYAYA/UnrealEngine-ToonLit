// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchCellAttributeFader.h"

#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "Library/DMXEntityFixtureType.h"


UDMXControlConsoleFaderGroup& UDMXControlConsoleFixturePatchCellAttributeFader::GetOwnerFaderGroupChecked() const
{
	const UDMXControlConsoleFixturePatchMatrixCell& MatrixCellFader = GetOwnerMatrixCellChecked();
	return MatrixCellFader.GetOwnerFaderGroupChecked();
}

int32 UDMXControlConsoleFixturePatchCellAttributeFader::GetIndex() const
{
	const UDMXControlConsoleFixturePatchMatrixCell& OwnerMatrixCell = GetOwnerMatrixCellChecked();
	const TArray<UDMXControlConsoleFaderBase*> CellAttributeFaders = OwnerMatrixCell.GetFaders();
	return CellAttributeFaders.IndexOfByKey(this);
}

#if WITH_EDITOR
void UDMXControlConsoleFixturePatchCellAttributeFader::SetIsMatchingFilter(bool bMatches)
{
	bIsMatchingFilter = bMatches;

	UDMXControlConsoleFixturePatchMatrixCell& MatrixCell = GetOwnerMatrixCellChecked();
	// Set Matrix Cell to not visible if it has no more visible CellAttributeFaders
	if (MatrixCell.IsMatchingFilter() && !bIsMatchingFilter)
	{
		if (!MatrixCell.HasVisibleInEditorCellAttributeFaders())
		{
			MatrixCell.SetIsMatchingFilter(false);
		}
	}
	else if (!MatrixCell.IsMatchingFilter() && bIsMatchingFilter)
	{
		MatrixCell.SetIsMatchingFilter(true);
	}
}
#endif // WITH_EDITOR

void UDMXControlConsoleFixturePatchCellAttributeFader::Destroy()
{
	UDMXControlConsoleFixturePatchMatrixCell& OwnerMatrixCell = GetOwnerMatrixCellChecked();

#if WITH_EDITOR
	OwnerMatrixCell.PreEditChange(UDMXControlConsoleFixturePatchMatrixCell::StaticClass()->FindPropertyByName(UDMXControlConsoleFixturePatchMatrixCell::GetCellAttributeFadersPropertyName()));
#endif // WITH_EDITOR

	OwnerMatrixCell.DeleteCellAttributeFader(this);

#if WITH_EDITOR
	OwnerMatrixCell.PostEditChange();
#endif // WITH_EDITOR
}

void UDMXControlConsoleFixturePatchCellAttributeFader::SetPropertiesFromFixtureCellAttribute(const FDMXFixtureCellAttribute& FixtureCellAttribute, const int32 InUniverseID, const int32 StartingChannel)
{
	// Order of initialization matters
	FaderName = FixtureCellAttribute.Attribute.Name.ToString();
	Attribute = FixtureCellAttribute.Attribute;

	UniverseID = InUniverseID;
	StartingAddress = StartingChannel;
	DefaultValue = FixtureCellAttribute.DefaultValue;
	Value = DefaultValue;
	MinValue = 0;

	SetDataType(FixtureCellAttribute.DataType);

	bUseLSBMode = FixtureCellAttribute.bUseLSBMode;
}

UDMXControlConsoleFixturePatchMatrixCell& UDMXControlConsoleFixturePatchCellAttributeFader::GetOwnerMatrixCellChecked() const
{
	UDMXControlConsoleFixturePatchMatrixCell* Outer = Cast<UDMXControlConsoleFixturePatchMatrixCell>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get fader owner correctly."), *GetName());

	return *Outer;
}
