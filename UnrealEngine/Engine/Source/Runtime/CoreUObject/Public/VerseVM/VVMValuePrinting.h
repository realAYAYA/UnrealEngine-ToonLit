// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/StringFwd.h"
#include "HAL/Platform.h"
#include "Misc/EnumClassFlags.h"

class FString;

namespace Verse
{
struct FAllocationContext;
struct VCell;
struct VEmergentType;
struct VInt;
struct VRestValue;
struct VValue;

struct FCellFormatter
{
	virtual ~FCellFormatter() {}

	// Format the cell into a string. Where possible, use a string builder and the Append method below
	virtual FString ToString(FAllocationContext Context, VCell& Cell) const = 0;

	// Format the cell into an existing string builder.
	virtual void Append(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const = 0;
};

enum class ECellFormatterMode : uint8
{
	EnableToString = 1 << 0,   ///< First try to use the ToString method to format the cell
	EnableVisitor = 1 << 1,    ///< Second try to use the VisitReferences to format the cell (Warning, ATM this doesn't handle circular references)
	IncludeCellNames = 1 << 2, ///< Wrap the value of the cell with the debug name of the cell
	FormatAddresses = 1 << 3,  ///< Format addresses when enabled, otherwise use a constant string for unit testing
	UniqueAddresses = 1 << 4,  ///< Add the address of UniqueString or UniqueStringSet to the output
};
ENUM_CLASS_FLAGS(ECellFormatterMode);

struct FDefaultCellFormatter : FCellFormatter
{
	static constexpr ECellFormatterMode DefaultMode = ECellFormatterMode::EnableToString | ECellFormatterMode::IncludeCellNames | ECellFormatterMode::UniqueAddresses | ECellFormatterMode::FormatAddresses;

	FDefaultCellFormatter(ECellFormatterMode InMode = DefaultMode)
		: Mode(InMode)
	{
	}

	// FCellFormatter implementation
	COREUOBJECT_API virtual FString ToString(FAllocationContext Context, VCell& Cell) const override;
	COREUOBJECT_API virtual void Append(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const override;

protected:
	// This helper method appends the cell to the string builder but without any of the debugging address text.
	// This allows such things as unit tests to override Append to provide stable strings to compare.
	COREUOBJECT_API virtual bool TryAppend(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const;

	void BeginCell(FStringBuilderBase& Builder, VCell& Cell) const;
	void EndCell(FStringBuilderBase& Builder) const;
	void AppendAddress(FStringBuilderBase& Builder, const void* Address) const;

	ECellFormatterMode Mode;
};

COREUOBJECT_API FString ToString(const VInt& Int);
COREUOBJECT_API FString ToString(FAllocationContext Context, const FCellFormatter& Formatter, const VValue& Value);
COREUOBJECT_API FString ToString(FAllocationContext Context, const FCellFormatter& Formatter, const VRestValue& Value);
COREUOBJECT_API void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter, const VValue& Value);
COREUOBJECT_API void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter, const VRestValue& Value);

} // namespace Verse
#endif // WITH_VERSE_VM
