// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchCellAttributeFader.h"

#include "DMXProtocolTypes.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "Library/DMXEntityFixtureType.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFixturePatchCellAttributeFader"

UDMXControlConsoleFixturePatchCellAttributeFader::UDMXControlConsoleFixturePatchCellAttributeFader()
	: DataType(EDMXFixtureSignalFormat::E8Bit)
{}

UDMXControlConsoleFaderGroup& UDMXControlConsoleFixturePatchCellAttributeFader::GetOwnerFaderGroupChecked() const
{
	const UDMXControlConsoleFixturePatchMatrixCell& MatrixCellFader = GetOwnerMatrixCellChecked();
	return MatrixCellFader.GetOwnerFaderGroupChecked();
}

int32 UDMXControlConsoleFixturePatchCellAttributeFader::GetIndex() const
{
	const UDMXControlConsoleFixturePatchMatrixCell* Outer = Cast<UDMXControlConsoleFixturePatchMatrixCell>(GetOuter());
	if (!ensureMsgf(Outer, TEXT("Invalid outer for '%s', cannot get matrix cell fader index correctly."), *GetName()))
	{
		return INDEX_NONE;
	}

	const TArray<UDMXControlConsoleFaderBase*> CellAttributeFaders = Outer->GetFaders();
	return CellAttributeFaders.IndexOfByKey(this);
}

void UDMXControlConsoleFixturePatchCellAttributeFader::Destroy()
{
	UDMXControlConsoleFixturePatchMatrixCell* Outer = Cast<UDMXControlConsoleFixturePatchMatrixCell>(GetOuter());
	if (!ensureMsgf(Outer, TEXT("Invalid outer for '%s', cannot destroy fader correctly."), *GetName()))
	{
		return;
	}

#if WITH_EDITOR
	Outer->PreEditChange(UDMXControlConsoleFixturePatchMatrixCell::StaticClass()->FindPropertyByName(UDMXControlConsoleFixturePatchMatrixCell::GetCellAttributeFadersPropertyName()));
#endif // WITH_EDITOR

	Outer->DeleteCellAttributeFader(this);

#if WITH_EDITOR
	Outer->PostEditChange();
#endif // WITH_EDITOR
}

void UDMXControlConsoleFixturePatchCellAttributeFader::SetPropertiesFromFixtureCellAttribute(const FDMXFixtureCellAttribute& FixtureCellAttribute, const int32 InUniverseID, const int32 StartingChannel)
{
	// Order of initialization matters
	FaderName = FixtureCellAttribute.Attribute.Name.ToString();
	Attribute = FixtureCellAttribute.Attribute;

	UniverseID = InUniverseID;
	StartingAddress = StartingChannel;
	Value = FixtureCellAttribute.DefaultValue;
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

void UDMXControlConsoleFixturePatchCellAttributeFader::SetValueRange()
{
	const uint8 NumChannels = static_cast<uint8>(DataType) + 1;
	MaxValue = MinValue + ((uint32)FMath::Pow(2.f, 8.f * NumChannels) - 1);
}

void UDMXControlConsoleFixturePatchCellAttributeFader::SetDataType(EDMXFixtureSignalFormat InDataType)
{
	DataType = InDataType;
	SetAddressRange(StartingAddress);
	SetValueRange();
}

void UDMXControlConsoleFixturePatchCellAttributeFader::SetAddressRange(int32 InStartingAddress)
{
	const uint8 NumChannels = static_cast<uint8>(DataType);
	StartingAddress = FMath::Clamp(InStartingAddress, 1, DMX_MAX_ADDRESS - NumChannels);
	EndingAddress = StartingAddress + NumChannels;
}

#undef LOCTEXT_NAMESPACE
