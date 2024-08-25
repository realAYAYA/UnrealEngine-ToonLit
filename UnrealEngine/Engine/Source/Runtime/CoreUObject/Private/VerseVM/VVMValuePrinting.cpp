// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMValuePrinting.h"
#include "Containers/UnrealString.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMRational.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMVar.h"
#include <inttypes.h>

namespace Verse
{

namespace
{
void AppendDebugName(FStringBuilderBase& Builder, const VEmergentType& EmergentType)
{
	if (EmergentType.CppClassInfo == &VUTF8String::StaticCppClassInfo)
	{
		Builder.Append(TEXT("String"));
	}
	else
	{
		FString Name = EmergentType.CppClassInfo->DebugName();
		FStringView NameView(Name);
		if (NameView.Len() > 0 && NameView[0] == 'V')
		{
			NameView.RightChopInline(1);
		}
		Builder.Append(NameView);
	}
}
} // namespace

struct FDefaultCellFormmatterVisitor : FAbstractVisitor
{
	FDefaultCellFormmatterVisitor(FStringBuilderBase& InBuilder, FAllocationContext InContext, const FCellFormatter& InFormatter)
		: Builder(InBuilder)
		, Context(InContext)
		, Formatter(InFormatter)
	{
	}

	void ToString(VCell* InCell)
	{
		check(NestingInfo.Num() == 0);
		PushNesting(ENestingType::Object);
		AppendDebugName(Builder, *InCell->GetEmergentType());
		Builder.Append(TEXT("("));
		InCell->VisitReferences(*this);
		Builder.Append(TEXT(")"));
		PopNesting(ENestingType::Object);
		check(NestingInfo.Num() == 0);
	}

	virtual void BeginArray(const TCHAR* ElementName, uint64& NumElements) override
	{
		BeginElement(ElementName);
		PushNesting(ENestingType::Array);
		Builder.Append(TEXT("("));
	}

	virtual void EndArray() override
	{
		PopNesting(ENestingType::Array);
		Builder.Append(TEXT(")"));
	}

	virtual void BeginSet(const TCHAR* ElementName, uint64& NumElements) override
	{
		BeginElement(ElementName);
		PushNesting(ENestingType::Set);
		Builder.Append(TEXT("("));
	}

	virtual void EndSet() override
	{
		PopNesting(ENestingType::Set);
		Builder.Append(TEXT(")"));
	}

	virtual void BeginMap(const TCHAR* ElementName, uint64& NumElements) override
	{
		BeginElement(ElementName);
		PushNesting(ENestingType::Map);
		Builder.Append(TEXT("("));
	}

	virtual void EndMap() override
	{
		PopNesting(ENestingType::Map);
		Builder.Append(TEXT(")"));
	}

	virtual void BeginObject(const TCHAR* ElementName) override
	{
		BeginElement(ElementName);
		PushNesting(ENestingType::Object);
		Builder.Append(TEXT("("));
	}

	virtual void EndObject() override
	{
		PopNesting(ENestingType::Object);
		Builder.Append(TEXT(")"));
	}

	virtual void VisitNonNull(VCell*& InCell, const TCHAR* ElementName) override
	{
		BeginElement(ElementName);
		Formatter.Append(Builder, Context, *InCell);
	}

	virtual void VisitEmergentType(const VCell* InEmergentType) override
	{
		// Any emergent type formatting has already been done
	}

	virtual void VisitNonNull(UObject*& InObject, const TCHAR* ElementName) override
	{
		BeginElement(ElementName);
		Builder.Append(TEXT("\"UObject\""));
	}

	virtual void Visit(VCell*& InCell, const TCHAR* ElementName) override
	{
		if (InCell != nullptr)
		{
			VisitNonNull(InCell, ElementName);
			return;
		}
		BeginElement(ElementName);
		Builder.Append(TEXT("nullptr"));
	}

	virtual void Visit(UObject*& InObject, const TCHAR* ElementName) override
	{
		if (InObject != nullptr)
		{
			VisitNonNull(InObject, ElementName);
			return;
		}
		BeginElement(ElementName);
		Builder.Append(TEXT("nullptr"));
	}

	virtual void Visit(VValue& Value, const TCHAR* ElementName) override
	{
		BeginElement(ElementName);
		Value.ToString(Builder, Context, Formatter);
	}

	virtual void Visit(VRestValue& Value, const TCHAR* ElementName) override
	{
		BeginElement(ElementName);
		Value.ToString(Builder, Context, Formatter);
	}

	void Visit(bool& bValue, const TCHAR* ElementName) override
	{
		BeginElement(ElementName);
		Builder.Append(bValue ? TEXT("true") : TEXT("false"));
	}

	virtual void Visit(FString& Value, const TCHAR* ElementName) override
	{
		BeginElement(ElementName);
		Builder.Append(TEXT("\""));
		Builder.Append(Value);
		Builder.Append(TEXT("\""));
	}

private:
	enum class ENestingType : uint8
	{
		Object,
		Array,
		Set,
		Map,
	};

	struct FNestingInfo
	{
		ENestingType Type;
		uint32 Index;
	};

	void PushNesting(ENestingType InType)
	{
		NestingInfo.Add(FNestingInfo{InType, 0});
	}

	void PopNesting(ENestingType InExpectedType)
	{
		CheckNesting(InExpectedType);
		NestingInfo.Pop();
	}

	void CheckNesting(ENestingType InExpectedType)
	{
		check(NestingInfo.Num() > 0 && NestingInfo.Last().Type == InExpectedType);
	}

	void BeginElement(const TCHAR* ElementName)
	{
		check(NestingInfo.Num() > 0);
		if (NestingInfo.Last().Index++ != 0)
		{
			Builder.Append(TEXT(", "));
		}
		if (NestingInfo.Last().Type == ENestingType::Object)
		{
			Builder.Append(ElementName);
			Builder.Append(TEXT("="));
		}
	}

	FStringBuilderBase& Builder;
	FAllocationContext Context;
	const FCellFormatter& Formatter;
	TArray<FNestingInfo> NestingInfo;
};

FString FDefaultCellFormatter::ToString(FAllocationContext Context, VCell& Cell) const
{
	TStringBuilder<128> Builder;
	Append(Builder, Context, Cell);
	return Builder.ToString();
}

bool FDefaultCellFormatter::TryAppend(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const
{
	// Logical values are handled via two globally unique cells.
	if (VValue Logic(Cell); Logic.IsLogic())
	{
		Builder << (Logic.AsBool() ? TEXT("true") : TEXT("false"));
		return true;
	}

	if (EnumHasAnyFlags(Mode, ECellFormatterMode::EnableToString))
	{
		const VCppClassInfo* ClassInfo = Cell.GetCppClassInfo();
		if (ClassInfo != nullptr && ClassInfo->ToString != nullptr)
		{
			BeginCell(Builder, Cell);
			ClassInfo->ToString(&Cell, Builder, Context, *this);
			EndCell(Builder);
			return true;
		}
	}

	if (EnumHasAnyFlags(Mode, ECellFormatterMode::EnableVisitor))
	{
		FDefaultCellFormmatterVisitor Visitor(Builder, Context, *this);
		Visitor.ToString(&Cell);
		return true;
	}

	return false;
}

void FDefaultCellFormatter::Append(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const
{
	if (!TryAppend(Builder, Context, Cell))
	{
		BeginCell(Builder, Cell);
		Builder.Append(TEXT("address "));
		AppendAddress(Builder, &Cell);
		EndCell(Builder);
	}

	if (EnumHasAnyFlags(Mode, ECellFormatterMode::UniqueAddresses))
	{
		if (Cell.IsA<VUniqueString>() || Cell.IsA<VUniqueStringSet>())
		{
			Builder.Append(TEXT(", address "));
			AppendAddress(Builder, &Cell);
		}
	}
}

void FDefaultCellFormatter::BeginCell(FStringBuilderBase& Builder, VCell& Cell) const
{
	if (EnumHasAnyFlags(Mode, ECellFormatterMode::IncludeCellNames))
	{
		const VEmergentType* EmergentType = Cell.GetEmergentType();
		AppendDebugName(Builder, *Cell.GetEmergentType());
		Builder.Append(TEXT("("));
	}
}

void FDefaultCellFormatter::EndCell(FStringBuilderBase& Builder) const
{
	if (EnumHasAnyFlags(Mode, ECellFormatterMode::IncludeCellNames))
	{
		Builder.Append(TEXT(")"));
	}
}

void FDefaultCellFormatter::AppendAddress(FStringBuilderBase& Builder, const void* Address) const
{
	if (EnumHasAnyFlags(Mode, ECellFormatterMode::FormatAddresses))
	{
		Builder.Append(TEXT("0x"));
		Builder.Appendf(TEXT("%p"), Address);
	}
	else
	{
		Builder.Append(TEXT("0x____"));
	}
}

FString ToString(const VInt& Int)
{
	if (Int.IsInt64())
	{
		const int64 Int64 = Int.AsInt64();
		return LexToString(Int64);
	}
	else
	{
		V_DIE("Arbitrary-precision integers are not yet supported.");
	}
}

FString ToString(FAllocationContext Context, const FCellFormatter& Formatter, const VValue& Value)
{
	return Value.ToString(Context, Formatter);
}

void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter, const VValue& Value)
{
	return Value.ToString(Builder, Context, Formatter);
}

FString VValue::ToString(FAllocationContext Context, const FCellFormatter& Formatter) const
{
	TStringBuilder<128> Builder;
	ToString(Builder, Context, Formatter);
	return Builder.ToString();
}

void VValue::ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter) const
{
	if (*this == VValue::EffectDoneMarker())
	{
		Builder.Appendf(TEXT("0x%x"), AsInt32());
	}
	else if (IsInt())
	{
		if (IsCellOfType<VHeapInt>())
		{
			// larger heap-ints
			Formatter.Append(Builder, Context, AsCell());
		}
		else
		{
			// Smaller ints
			Builder.Append(::Verse::ToString(AsInt()));
		}
	}
	else if (IsFloat())
	{
		Builder << AsFloat().AsDouble();
	}
	else if (IsCell())
	{
		Formatter.Append(Builder, Context, AsCell());
	}
	else if (IsUObject())
	{
		Builder.Appendf(TEXT("UObject(%p)"), AsUObject());
	}
	else if (IsRoot())
	{
		Builder.Appendf(TEXT("Root(%u)"), GetSplitDepth());
	}
	else if (IsChar())
	{
		Builder.Appendf(TEXT("'%c'"), static_cast<char>(AsChar()));
	}
	else if (IsChar32())
	{
		Builder.Appendf(TEXT("Char32(%u)"), AsChar32());
	}
	else if (IsPlaceholder())
	{
		VValue Temp = *this; // Don't have printing path compress
		VPlaceholder& Placeholder = Temp.AsPlaceholder();
		Builder.Appendf(TEXT("Placeholder(0x%" PRIxPTR "->"), &Placeholder);
		VValue Pointee = Placeholder.Follow();
		if (Pointee.IsPlaceholder())
		{
			Builder.Appendf(TEXT("0x%" PRIxPTR), &Pointee.AsPlaceholder());
		}
		else
		{
			Pointee.ToString(Builder, Context, Formatter);
		}
		Builder.Append(TEXT(")"));
	}
	else if (IsUninitialized())
	{
		Builder.Append(TEXT("Uninitialized"));
	}
	else
	{
		V_DIE("Unhandled Verse value encoding: 0x%" PRIxPTR, GetEncodedBits());
	}
}

FString ToString(FAllocationContext Context, const FCellFormatter& CellFormatter, const VRestValue& Value)
{
	return Value.ToString(Context, CellFormatter);
}

void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& CellFormatter, const VRestValue& Value)
{
	return Value.ToString(Builder, Context, CellFormatter);
}

FString VRestValue::ToString(FAllocationContext Context, const FCellFormatter& Formatter) const
{
	return Value.Get().ToString(Context, Formatter);
}

void VRestValue::ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter) const
{
	return Value.Get().ToString(Builder, Context, Formatter);
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
