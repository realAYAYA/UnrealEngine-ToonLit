// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "Serialization/StructuredArchiveFormatter.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMHeapInt.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{

namespace
{
const static FLazyName NAME_VNull("VNull");
const static FLazyName NAME_VTrue("VTrue");
const static FLazyName NAME_VFalse("VFalse");
const static FLazyName NAME_VInt("VInt");
const static FLazyName NAME_VFloat("VFloat");
const static FLazyName NAME_VChar("VChar");
const static FLazyName NAME_VChar32("VChar32");
const static FLazyName NAME_VNone("VNone");
const static FLazyName NAME_Batch("Batch");
const static FLazyName NAME_CellIndex("CellIndex");
const static TCHAR* TypeElementName = TEXT("_type");
const static TCHAR* CellTypeElementName = TEXT("_cellType");
const static TCHAR* CellIdName = TEXT("_cellid");
const static TCHAR* PreCreateName = TEXT("_precreates");
const static TCHAR* CellsName = TEXT("_cells");
const static TCHAR* BatchSizeName = TEXT("_batchSize");
} // namespace

FStructuredArchiveVisitor::ScopedRecord::ScopedRecord(FStructuredArchiveVisitor& InVisitor, const TCHAR* InName)
	: Visitor(InVisitor)
	, Name(InName)
	, Record(Visitor.EnterObject(Name))
{
}

FStructuredArchiveVisitor::ScopedRecord::~ScopedRecord()
{
	Visitor.LeaveObject();
}

void FStructuredArchiveVisitor::WriteElementType(FStructuredArchiveRecord Record, FEncodedType EncodedType)
{
	if (IsTextFormat())
	{
		FName TypeName;
		switch (EncodedType.EncodedType)
		{
			case EEncodedType::None:
				TypeName = NAME_VNone;
				break;
			case EEncodedType::Null:
				TypeName = NAME_VNull;
				break;
			case EEncodedType::True:
				TypeName = NAME_VTrue;
				break;
			case EEncodedType::False:
				TypeName = NAME_VFalse;
				break;
			case EEncodedType::Int:
				TypeName = NAME_VInt;
				break;
			case EEncodedType::Float:
				TypeName = NAME_VFloat;
				break;
			case EEncodedType::Char:
				TypeName = NAME_VChar;
				break;
			case EEncodedType::Char32:
				TypeName = NAME_VChar32;
				break;
			case EEncodedType::Batch:
				TypeName = NAME_Batch;
				break;
			case EEncodedType::CellIndex:
				TypeName = NAME_CellIndex;
				break;
			case EEncodedType::Cell:
				ensure(EncodedType.CppClassInfo != nullptr);
				TypeName = FName(EncodedType.CppClassInfo->Name);
				break;
			default:
				V_DIE("Unexpected EncodedType");
		}
		Record.EnterField(TypeElementName) << TypeName;
	}
	else
	{
		{
			FStructuredArchiveSlot Field = Record.EnterField(TypeElementName);
			uint8 ScratchType = (uint8)EncodedType.EncodedType;
			Field << ScratchType;
		}
		if (EncodedType.EncodedType == EEncodedType::Cell)
		{
			ensure(EncodedType.CppClassInfo != nullptr);
			FStructuredArchiveSlot Field = Record.EnterField(CellTypeElementName);
			FName TypeName(EncodedType.CppClassInfo->Name);
			Field << TypeName;
		}
	}
}

FStructuredArchiveVisitor::FEncodedType FStructuredArchiveVisitor::ReadElementType(FStructuredArchiveRecord Record)
{
	FEncodedType EncodedType(EEncodedType::None);
	if (IsTextFormat())
	{
		FName TypeName;
		Record.EnterField(TypeElementName) << TypeName;
		if (TypeName == NAME_VNone)
		{
			EncodedType = FEncodedType(EEncodedType::None);
		}
		else if (TypeName == NAME_VNull)
		{
			EncodedType = FEncodedType(EEncodedType::Null);
		}
		else if (TypeName == NAME_VTrue)
		{
			EncodedType = FEncodedType(EEncodedType::True);
		}
		else if (TypeName == NAME_VFalse)
		{
			EncodedType = FEncodedType(EEncodedType::False);
		}
		else if (TypeName == NAME_VInt)
		{
			EncodedType = FEncodedType(EEncodedType::Int);
		}
		else if (TypeName == NAME_VFloat)
		{
			EncodedType = FEncodedType(EEncodedType::Float);
		}
		else if (TypeName == NAME_VChar)
		{
			EncodedType = FEncodedType(EEncodedType::Char);
		}
		else if (TypeName == NAME_VChar32)
		{
			EncodedType = FEncodedType(EEncodedType::Char32);
		}
		else if (TypeName == NAME_Batch)
		{
			EncodedType = FEncodedType(EEncodedType::Batch);
		}
		else if (TypeName == NAME_CellIndex)
		{
			EncodedType = FEncodedType(EEncodedType::CellIndex);
		}
		else
		{
			const VCppClassInfo* CppClassInfo = VCppClassInfoRegistry::GetCppClassInfo(*TypeName.ToString());
			if (CppClassInfo != nullptr)
			{
				EncodedType = FEncodedType(EEncodedType::Cell, CppClassInfo);
			}
			else
			{
				V_DIE("Unable to find class information for %s", *TypeName.ToString());
			}
		}
	}
	else
	{
		{
			FStructuredArchiveSlot Field = Record.EnterField(TypeElementName);
			uint8 ScratchType;
			Field << ScratchType;
			EncodedType.EncodedType = (EEncodedType)ScratchType;
		}

		if (EncodedType.EncodedType == EEncodedType::Cell)
		{
			FStructuredArchiveSlot Field = Record.EnterField(CellTypeElementName);
			FName TypeName;
			Field << TypeName;
			EncodedType.CppClassInfo = VCppClassInfoRegistry::GetCppClassInfo(*TypeName.ToString());
			if (EncodedType.CppClassInfo == nullptr)
			{
				V_DIE("Unable to find class information for %s", *TypeName.ToString());
			}
		}
	}
	return EncodedType;
}

void FStructuredArchiveVisitor::WriteCellBodyInternal(FStructuredArchiveRecord Record, VCell* InCell)
{
	const VCppClassInfo* CppClassInfo = InCell->GetCppClassInfo();
	if (CppClassInfo->Serialize)
	{
		WriteElementType(Record, FEncodedType(EEncodedType::Cell, CppClassInfo));
		CppClassInfo->Serialize(InCell, Context, *this);
	}
	else
	{
		V_DIE("The class \"%s\" does not have a serialization method defined", *CppClassInfo->DebugName());
	}
}

void FStructuredArchiveVisitor::WriteCellBody(FStructuredArchiveRecord Record, VCell* InCell)
{
	if (InCell == nullptr)
	{
		WriteElementType(Record, FEncodedType(EEncodedType::Null));
	}
	else if (VValue Logic(*InCell); Logic.IsLogic())
	{
		WriteElementType(Record, FEncodedType(Logic.AsBool() ? EEncodedType::True : EEncodedType::False));
	}
	else
	{
		if (SerializeContext == nullptr)
		{
			WriteCellBodyInternal(Record, InCell);
		}
		else if (bIsInBatch)
		{
			WriteElementType(Record, FEncodedType(EEncodedType::CellIndex));
			Record.EnterField(CellIdName) << SerializeContext->CellToPackageIndex.FindChecked(InCell);
		}
		else
		{
			WriteElementType(Record, FEncodedType(EEncodedType::Batch));

			FVCellSerializeContext::FBatch Batch = SerializeContext->BuildBatch(InCell);
			Record.EnterField(CellIdName) << Batch.RootCell;

			int32 BatchSize = Batch.Exports.Num();
			Record.EnterField(BatchSizeName) << BatchSize;

			{
				int32 Num = Batch.Precreate.Num();
				FStructuredArchiveArray Array = EnterArray(PreCreateName, Num, ENestingType::Array);
				for (FPackageIndex PackageIndex : Batch.Precreate)
				{
					VCell* Cell = SerializeContext->ExportMap[PackageIndex.ToExport()];
					ensure(Cell->GetEmergentType()->CppClassInfo != nullptr);
					FName TypeName(Cell->GetEmergentType()->CppClassInfo->Name);

					FStructuredArchiveRecord ArrayRecord = Array.EnterElement().EnterRecord();
					ArrayRecord.EnterField(CellIdName) << PackageIndex;
					ArrayRecord.EnterField(CellTypeElementName) << TypeName;
				}
				LeaveArray(ENestingType::Array);
			}

			{
				bIsInBatch = true;
				int32 Num = Batch.Exports.Num();
				FStructuredArchiveArray Array = EnterArray(CellsName, Num, ENestingType::Array);
				for (FPackageIndex PackageIndex : Batch.Exports)
				{
					VCell* Cell = SerializeContext->ExportMap[PackageIndex.ToExport()];
					ensure(Cell->GetEmergentType()->CppClassInfo != nullptr);
					FName TypeName(Cell->GetEmergentType()->CppClassInfo->Name);

					FStructuredArchiveRecord ArrayRecord = EnterObject(TEXT(""));
					ArrayRecord.EnterField(CellIdName) << PackageIndex;
					WriteCellBodyInternal(ArrayRecord, Cell);
					LeaveObject();
				}
				LeaveArray(ENestingType::Array);
				bIsInBatch = false;
			}
		}
	}
}

void FStructuredArchiveVisitor::ReadCellBodyInternal(FStructuredArchiveRecord Record, FEncodedType EncodedType, VCell*& InOutCell)
{
	if (EncodedType.CppClassInfo->Serialize)
	{
		EncodedType.CppClassInfo->Serialize(InOutCell, Context, *this);
	}
	else
	{
		V_DIE("The class \"%s\" does not have a serialization method defined", *EncodedType.CppClassInfo->DebugName());
	}
}

VCell* FStructuredArchiveVisitor::ReadCellBody(FStructuredArchiveRecord Record, FEncodedType EncodedType)
{
	switch (EncodedType.EncodedType)
	{
		case EEncodedType::Null:
			return nullptr;

		case EEncodedType::False:
			return GlobalFalsePtr.Get();

		case EEncodedType::True:
			return GlobalTruePtr.Get();

		case EEncodedType::Cell:
		{
			VCell* NewCell = nullptr;
			ReadCellBodyInternal(Record, EncodedType, NewCell);
			return NewCell;
		}

		case EEncodedType::CellIndex:
		{
			if (SerializeContext == nullptr)
			{
				V_DIE("Request to deserialize a cell index but no serialization context is available");
			}

			FPackageIndex PackageIndex;
			Record.EnterField(CellIdName) << PackageIndex;
			return SerializeContext->ExportMap[PackageIndex.ToExport()];
		}

		case EEncodedType::Batch:
		{
			check(bIsInBatch == false);
			if (SerializeContext == nullptr)
			{
				V_DIE("Request to deserialize a batch of VCells but no serialization context is available");
			}

			FPackageIndex RootCell;
			Record.EnterField(CellIdName) << RootCell;

			int32 BatchSize;
			Record.EnterField(BatchSizeName) << BatchSize;

			SerializeContext->ExportMap.SetNum(SerializeContext->ExportMap.Num() + BatchSize);

			// Handle all the precreates.  These are cells without any actual data that need to be
			// created prior to deserialization to handle circular dependencies.
			{
				int32 Num = 0;
				FStructuredArchiveArray Array = EnterArray(PreCreateName, Num, ENestingType::Array);
				for (int32 Index = 0; Index < Num; ++Index)
				{
					FStructuredArchiveRecord ArrayRecord = Array.EnterElement().EnterRecord();

					FPackageIndex PackageIndex;
					ArrayRecord.EnterField(CellIdName) << PackageIndex;

					FName TypeName;
					ArrayRecord.EnterField(CellTypeElementName) << TypeName;

					VCppClassInfo* CppClassInfo = VCppClassInfoRegistry::GetCppClassInfo(*TypeName.ToString());
					if (CppClassInfo == nullptr)
					{
						V_DIE("Unable to find class information for %s", *TypeName.ToString());
					}

					SerializeContext->ExportMap[PackageIndex.ToExport()] = &CppClassInfo->SerializeNew(Context);
				}
				LeaveArray(ENestingType::Array);
			}

			// Read all the cells which includes the requested cell and any cell that it references.
			{
				bIsInBatch = true;
				int32 Num = 0;
				FStructuredArchiveArray Array = EnterArray(CellsName, Num, ENestingType::Array);
				for (int Index = 0; Index < Num; ++Index)
				{
					FStructuredArchiveRecord ArrayRecord = EnterObject(TEXT(""));
					FPackageIndex PackageIndex;
					ArrayRecord.EnterField(CellIdName) << PackageIndex;
					VCell*& ExportCell = SerializeContext->ExportMap[PackageIndex.ToExport()];
					FEncodedType InnerEncodedType = ReadElementType(ArrayRecord);
					ReadCellBodyInternal(ArrayRecord, InnerEncodedType, ExportCell);
					LeaveObject();
				}
				LeaveArray(ENestingType::Array);
				bIsInBatch = false;
			}

			// Return the referenced cell
			return SerializeContext->ExportMap[RootCell.ToExport()];
		}

		case EEncodedType::None:
		case EEncodedType::Int:
		case EEncodedType::Float:
		case EEncodedType::Char:
		case EEncodedType::Char32:
		default:
			V_DIE("Unexpected encoded type");
	}
}

void FStructuredArchiveVisitor::VisitCellBody(FStructuredArchiveRecord Record, VCell*& InOutCell)
{
	if (IsLoading())
	{
		FEncodedType EncodedType = ReadElementType(Record);
		InOutCell = ReadCellBody(Record, EncodedType);
	}
	else
	{
		WriteCellBody(Record, InOutCell);
	}
}

VValue FStructuredArchiveVisitor::FollowPlaceholder(VValue InValue)
{
	if (!InValue.IsPlaceholder())
	{
		return InValue;
	}

	VPlaceholder& Placeholder = InValue.AsPlaceholder();
	VValue ScratchValue = Placeholder.Follow();
	if (ScratchValue.IsPlaceholder())
	{
		V_DIE("Unfollowable placeholder: 0x%" PRIxPTR, InValue.GetEncodedBits());
	}
	return ScratchValue;
}

void FStructuredArchiveVisitor::WriteValueBody(FStructuredArchiveRecord Record, VValue InValue)
{
	// NOTE: This IsCell should handle Logic and HeapInt values.
	if (InValue.IsCell())
	{
		WriteCellBody(Record, &InValue.AsCell());
	}
	else if (InValue.IsInt())
	{
		VInt Int = InValue.AsInt();
		if (Int.IsInt64())
		{
			WriteElementType(Record, FEncodedType(EEncodedType::Int));
			int64 Int64 = Int.AsInt64();
			Record.EnterField(TEXT("Value")) << Int64;
		}
		else
		{
			V_DIE("Arbitrary-precision integers are handled above in IsCell.");
		}
	}
	else if (InValue.IsFloat())
	{
		WriteElementType(Record, FEncodedType(EEncodedType::Float));
		double DoubleValue = InValue.AsFloat().AsDouble();
		Record.EnterField(TEXT("Value")) << DoubleValue;
	}
	else if (InValue.IsChar())
	{
		WriteElementType(Record, FEncodedType(EEncodedType::Char));
		uint8 Char = InValue.AsChar();
		Record.EnterField(TEXT("Value")) << Char;
	}
	else if (InValue.IsChar32())
	{
		WriteElementType(Record, FEncodedType(EEncodedType::Char32));
		uint32 Char32 = InValue.AsChar32();
		Record.EnterField(TEXT("Value")) << Char32;
	}
	else if (InValue.IsUninitialized())
	{
		WriteElementType(Record, FEncodedType(EEncodedType::None));
	}
	else
	{
		V_DIE("Unhandled Verse value encoding: 0x%" PRIxPTR, InValue.GetEncodedBits());
	}
}

VValue FStructuredArchiveVisitor::ReadValueBody(FStructuredArchiveRecord Record, FEncodedType EncodedType)
{
	switch (EncodedType.EncodedType)
	{
		case EEncodedType::None:
			return VValue();

		case EEncodedType::Int:
		{
			int64 Int64;
			Record.EnterField(TEXT("Value")) << Int64;
			return VValue(VInt(Context, Int64));
		}

		case EEncodedType::Float:
		{
			double DoubleValue;
			Record.EnterField(TEXT("Value")) << DoubleValue;
			return VValue(VFloat(DoubleValue));
		}

		case EEncodedType::Char:
		{
			uint8 Char;
			Record.EnterField(TEXT("Value")) << Char;
			return VValue::Char(Char);
		}

		case EEncodedType::Char32:
		{
			uint32 Char32;
			Record.EnterField(TEXT("Value")) << Char32;
			return VValue::Char32(Char32);
		}

		case EEncodedType::Cell:
		{
			return VValue(*ReadCellBody(Record, EncodedType));
		}

		case EEncodedType::CellIndex:
		{
			if (SerializeContext == nullptr)
			{
				V_DIE("Request to deserialize a cell index but no serialization context is available");
			}

			FPackageIndex PackageIndex;
			Record.EnterField(CellIdName) << PackageIndex;
			return VValue(*SerializeContext->ExportMap[PackageIndex.ToExport()]);
		}

		case EEncodedType::Null:
		default:
			V_DIE("Unexpected encoded type %u", static_cast<uint8>(EncodedType.EncodedType));
	}
}

void FStructuredArchiveVisitor::VisitValueBody(FStructuredArchiveRecord Record, VValue& InOutValue)
{
	if (IsLoading())
	{
		FEncodedType EncodedType = ReadElementType(Record);
		InOutValue = ReadValueBody(Record, EncodedType);
	}
	else
	{
		WriteValueBody(Record, FollowPlaceholder(InOutValue));
	}
}

void FStructuredArchiveVisitor::Serialize(VCell*& InOutCell)
{
	Visit(InOutCell, TEXT(""));
}

void FStructuredArchiveVisitor::Serialize(VValue& InOutValue)
{
	Visit(InOutValue, TEXT(""));
}

void FStructuredArchiveVisitor::BeginArray(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	int32 ScratchNumElements = int32(NumElements);
	EnterArray(ElementName, ScratchNumElements, ENestingType::Array);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::EndArray()
{
	LeaveArray(ENestingType::Array);
}

void FStructuredArchiveVisitor::BeginSet(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	int32 ScratchNumElements = int32(NumElements);
	EnterArray(ElementName, ScratchNumElements, ENestingType::Set);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::FStructuredArchiveVisitor::EndSet()
{
	LeaveArray(ENestingType::Set);
}

void FStructuredArchiveVisitor::BeginMap(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	int32 ScratchNumElements = int32(NumElements);
	EnterArray(ElementName, ScratchNumElements, ENestingType::Map);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::EndMap()
{
	LeaveArray(ENestingType::Map);
}

void FStructuredArchiveVisitor::BeginObject(const TCHAR* ElementName)
{
	EnterObject(ElementName);
}

void FStructuredArchiveVisitor::EndObject()
{
	LeaveObject();
}

void FStructuredArchiveVisitor::VisitNonNull(VCell*& InCell, const TCHAR* ElementName)
{
	VisitCellBody(ScopedRecord(*this, ElementName).Record, InCell);
}

void FStructuredArchiveVisitor::VisitEmergentType(const VCell* InEmergentType)
{
	// Any emergent type formatting has already been done
}

void FStructuredArchiveVisitor::VisitNonNull(UObject*& InObject, const TCHAR* ElementName)
{
}

void FStructuredArchiveVisitor::Visit(VCell*& InCell, const TCHAR* ElementName)
{
	VisitCellBody(ScopedRecord(*this, ElementName).Record, InCell);
}

void FStructuredArchiveVisitor::Visit(UObject*& InObject, const TCHAR* ElementName)
{
}

void FStructuredArchiveVisitor::Visit(VValue& Value, const TCHAR* ElementName)
{
	ScopedRecord ScopedRecord(*this, ElementName);
	VisitValueBody(ScopedRecord.Record, Value);
}

void FStructuredArchiveVisitor::Visit(VRestValue& Value, const TCHAR* ElementName)
{
	// Restrict calling VRestValue visitor to using FAbstractVisitor.
	Value.Visit(static_cast<FAbstractVisitor&>(*this), ElementName);
}

void FStructuredArchiveVisitor::Visit(bool& bValue, const TCHAR* ElementName)
{
	Slot(ElementName) << bValue;
}

void FStructuredArchiveVisitor::Visit(FString& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(uint64& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(int64& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

FStructuredArchiveArray FStructuredArchiveVisitor::EnterArray(const TCHAR* ElementName, int32& Num, ENestingType Type)
{
	if (NestingInfo.Num() == 0)
	{
		FStructuredArchiveArray Child = StructuredArchive.Open().EnterArray(Num);
		NestingInfo.Push(NestingEntry(Child, Type));
		return Child;
	}
	else if (NestingInfo.Last().Type == ENestingType::Object)
	{
		FStructuredArchiveRecord& Record = static_cast<FStructuredArchiveRecord&>(NestingInfo.Last().Slot);
		FStructuredArchiveArray Child = Record.EnterArray(ElementName, Num);
		NestingInfo.Push(NestingEntry(Child, Type));
		return Child;
	}
	else
	{
		FStructuredArchiveArray& Array = static_cast<FStructuredArchiveArray&>(NestingInfo.Last().Slot);
		FStructuredArchiveArray Child = Array.EnterElement().EnterArray(Num);
		NestingInfo.Push(NestingEntry(Child, Type));
		return Child;
	}
}

void FStructuredArchiveVisitor::LeaveArray(ENestingType Type)
{
	check(NestingInfo.Num() > 0 && NestingInfo.Last().Type == Type);
	NestingInfo.Pop();
}

FStructuredArchiveRecord FStructuredArchiveVisitor::EnterObject(const TCHAR* ElementName)
{
	if (NestingInfo.Num() == 0)
	{
		FStructuredArchiveRecord Child = StructuredArchive.Open().EnterRecord();
		NestingInfo.Push(NestingEntry(Child, ENestingType::Object));
		return Child;
	}
	else if (NestingInfo.Last().Type == ENestingType::Object)
	{
		FStructuredArchiveRecord& Record = static_cast<FStructuredArchiveRecord&>(NestingInfo.Last().Slot);
		FStructuredArchiveRecord Child = Record.EnterRecord(ElementName);
		NestingInfo.Push(NestingEntry(Child, ENestingType::Object));
		return Child;
	}
	else
	{
		FStructuredArchiveArray& Array = static_cast<FStructuredArchiveArray&>(NestingInfo.Last().Slot);
		FStructuredArchiveRecord Child = Array.EnterElement().EnterRecord();
		NestingInfo.Push(NestingEntry(Child, ENestingType::Object));
		return Child;
	}
}

void FStructuredArchiveVisitor::LeaveObject()
{
	check(NestingInfo.Num() > 0 && NestingInfo.Last().Type == ENestingType::Object);
	NestingInfo.Pop();
}

FStructuredArchiveSlot FStructuredArchiveVisitor::Slot(const TCHAR* ElementName)
{
	check(NestingInfo.Num() > 0);
	if (NestingInfo.Last().Type == ENestingType::Object)
	{
		FStructuredArchiveRecord& Record = static_cast<FStructuredArchiveRecord&>(NestingInfo.Last().Slot);
		return Record.EnterField(ElementName);
	}
	else
	{
		FStructuredArchiveArray& Array = static_cast<FStructuredArchiveArray&>(NestingInfo.Last().Slot);
		return Array.EnterElement();
	}
}

FArchive* FStructuredArchiveVisitor::GetUnderlyingArchive()
{
	return &StructuredArchive.GetUnderlyingArchive();
}

bool FStructuredArchiveVisitor::IsLoading()
{
	return StructuredArchive.GetUnderlyingArchive().IsLoading();
}

bool FStructuredArchiveVisitor::IsTextFormat()
{
	return StructuredArchive.GetUnderlyingArchive().IsTextFormat();
}

FAccessContext FStructuredArchiveVisitor::GetLoadingContext()
{
	return Context;
}

struct FVCellSerializeContextBatchBuilder;

struct FVCellSerializeContextVistor : Verse::FAbstractVisitor
{
	virtual void VisitNonNull(Verse::VCell*& InCell, const TCHAR* ElementName) override;

	virtual void VisitNonNull(UObject*& InObject, const TCHAR* ElementName) override
	{
	}

	FVCellSerializeContextBatchBuilder& Batch;

	FVCellSerializeContextVistor(FVCellSerializeContextBatchBuilder& InBatch)
		: Batch(InBatch)
	{
	}
};

struct FVCellSerializeContextBatchBuilder
{
	FVCellSerializeContextBatchBuilder(FVCellSerializeContext& InSerializeContext, FVCellSerializeContext::FBatch& InBatch)
		: SerializeContext(InSerializeContext)
		, Batch(InBatch)
		, StartingIndexBias(SerializeContext.ExportMap.Num())
		, CurrentIndex(0)
		, Visitor(*this)
	{
	}

	void Process(VCell* Root)
	{
		check(Root != nullptr);

		// If the cell being exported already exists, then there are no cells to export
		if (!AddExportMap(Root, Batch.RootCell))
		{
			return;
		}

		// Initialize the queue with the root
		Queue.Emplace(FQueueEntry(Root, Batch.RootCell));

		// While there are queue entries, visit the cells to collect the references
		// NOTE: Future optimization.  See if a depth first search will at least cull any leaf
		// nodes and possibly even eliminating the need for the planning algorithm when the
		// graph doesn't contain any cycles.
		while (CurrentIndex < Queue.Num())
		{
			Queue[CurrentIndex].Cell->VisitReferences(Visitor);
			++CurrentIndex;
		}

		// Construct an array of the index of cells remaining to be added to the export list
		TArray<int> Pending;
		Pending.Reserve(CurrentIndex);
		for (int Index = 0; Index < CurrentIndex; ++Index)
		{
			Pending.Emplace(CurrentIndex - (Index + 1)); // Reverse the order to reduce the iterations
			Queue[Index].RemainingOutboundCount = Queue[Index].Outbound.Count;
			Queue[Index].RemainingInboundCount = Queue[Index].Inbound.Count;
		}

		// The planing algorithm:
		//
		// 1) Scan the pending cells looking for any cells with no references to any other cells (that haven't already been exported)
		// 2) If cells can be found that support SerializeNew, then try and locate the best cell to force export that has the
		//    fewest outbound reference.
		// 3) If no cells can be found that either have zero outbound references or support SerializeNew, the we have no way to break
		//    the cycle.
#if !UE_BUILD_SHIPPING
		Batch.Iterations = 0;
#endif
		while (Pending.Num() > 0)
		{
#if !UE_BUILD_SHIPPING
			++Batch.Iterations;
#endif

			int BestPendingIndex = -1;
			int BestOutboundCount = MAX_int32;

			int OutPendingIndex = 0;
			for (int PendingIndex = 0; PendingIndex < Pending.Num(); ++PendingIndex)
			{
				int QueueIndex = Pending[PendingIndex];
				FQueueEntry& Entry = Queue[QueueIndex];
				if (Entry.RemainingOutboundCount == 0)
				{
					AddExport(Entry);
				}
				else if (Entry.Cell->GetCppClassInfo()->SerializeNew != nullptr)
				{
					if (Entry.RemainingOutboundCount < BestOutboundCount)
					{
						BestOutboundCount = Entry.RemainingOutboundCount;
						BestPendingIndex = OutPendingIndex;
					}
					Pending[OutPendingIndex++] = QueueIndex;
				}
				else
				{
					Pending[OutPendingIndex++] = QueueIndex;
				}
			}

			if (OutPendingIndex < Pending.Num())
			{
				Pending.RemoveAt(OutPendingIndex, Pending.Num() - OutPendingIndex);
			}
			else if (BestPendingIndex >= 0)
			{
				AddExport(Queue[Pending[BestPendingIndex]]);
				Pending.RemoveAt(BestPendingIndex);
			}
			else
			{
				checkf(false, TEXT("Unable to break VCell circular list because there is no cell that implements SerializeNew"));
			}
		}
		return;
	}

private:
	struct FReferenceList
	{
		int Last = -1;
		int Count = 0;
	};

	struct FQueueEntry
	{
		VCell* Cell;
		FPackageIndex PackageIndex;
		FReferenceList Inbound;
		FReferenceList Outbound;
		int RemainingOutboundCount = 0;
		int RemainingInboundCount = 0;
		bool Created = false;

		FQueueEntry(VCell* InCell, FPackageIndex InPackageIndex)
			: Cell(InCell)
			, PackageIndex(InPackageIndex)
		{
		}
	};

	struct FReference
	{
		int Index;
		int Next;
	};

	// Append a inbound/outbound reference to the given queue index
	static void AppendList(TArray<FReference>& References, FReferenceList& Current, int QueueIndex)
	{
		int Index = References.Num();
		References.Emplace(FReference{QueueIndex, Current.Last});
		++Current.Count;
		Current.Last = Index;
	}

	// Add the cell to the export map.  Will return true if the cell
	// was added, false if it already existed.
	bool AddExportMap(VCell* Cell, FPackageIndex& PackageIndex)
	{
		check(Cell != nullptr);
		FPackageIndex& ScratchIndex = SerializeContext.CellToPackageIndex.FindOrAdd(Cell);
		if (ScratchIndex.IsNull())
		{
			ScratchIndex = FPackageIndex::FromExport(SerializeContext.ExportMap.Num());
			SerializeContext.ExportMap.Emplace(Cell);
			PackageIndex = ScratchIndex;
			return true;
		}
		PackageIndex = ScratchIndex;
		return false;
	}

	// Invoked by the reference collector to add a cell reference to the things being exported
	void AddCellReference(VCell* Cell)
	{
		if (Cell == nullptr)
		{
			return;
		}

		// Add the cell to the export map
		FPackageIndex PackageIndex;
		AddExportMap(Cell, PackageIndex);

		// Get the export index.  If it was added prior to this batch, then it has already been exported
		// and the reference can be ignored
		int Index = PackageIndex.ToExport() - StartingIndexBias;
		if (Index < 0)
		{
			return;
		}

		// Since we have subtracted off the prior number of exported cells, the index maps directly into the
		// queue.  If new, add a new entry
		check(Index <= Queue.Num());
		if (Index == Queue.Num())
		{
			Queue.Emplace(FQueueEntry(Cell, PackageIndex));
		}

		// Add to the inbound and output queues.
		AppendList(InboundReferences, Queue[Index].Inbound, CurrentIndex);
		AppendList(OutboundReferences, Queue[CurrentIndex].Outbound, Index);
	}

	// Mark the cell as being created (either from exporting or pre-create)
	bool MarkAsCreated(FQueueEntry& Entry)
	{

		// If we were already created, (i.e. added to precreate or export list), then ignore
		if (Entry.Created)
		{
			return false;
		}
		Entry.Created = true;

		// Remove this cell from the inbound and outbound reference counts.
		for (int RefIndex = Entry.Inbound.Last; RefIndex >= 0; RefIndex = InboundReferences[RefIndex].Next)
		{
			--Queue[InboundReferences[RefIndex].Index].RemainingOutboundCount;
		}
		for (int RefIndex = Entry.Outbound.Last; RefIndex >= 0; RefIndex = OutboundReferences[RefIndex].Next)
		{
			--Queue[OutboundReferences[RefIndex].Index].RemainingInboundCount;
		}
		return true;
	}

	// This cell has been selected to be exported next.  If there are still remaining outbound reference, then
	// any cell this cell references will have to be precreated in order to satisfy those references.
	void AddExport(FQueueEntry& Entry)
	{
		bool bIsPrecreate = Entry.RemainingOutboundCount > 0;
		Batch.Exports.Add(Entry.PackageIndex);
		MarkAsCreated(Entry);

		if (bIsPrecreate)
		{
			for (int RefIndex = Entry.Outbound.Last; RefIndex >= 0; RefIndex = OutboundReferences[RefIndex].Next)
			{
				FQueueEntry& RefEntry = Queue[OutboundReferences[RefIndex].Index];
				if (MarkAsCreated(RefEntry))
				{
					Batch.Precreate.Add(RefEntry.PackageIndex);
				}
			}
		}
	}

	FVCellSerializeContext& SerializeContext;
	FVCellSerializeContext::FBatch& Batch;
	int StartingIndexBias;
	int CurrentIndex;
	TArray<FQueueEntry> Queue;
	TArray<FReference> InboundReferences;
	TArray<FReference> OutboundReferences;
	FVCellSerializeContextVistor Visitor;

	friend struct FVCellSerializeContextVistor;
};

void FVCellSerializeContextVistor::VisitNonNull(Verse::VCell*& InCell, const TCHAR* ElementName)
{
	if (!InCell->IsA<Verse::VEmergentType>())
	{
		Batch.AddCellReference(InCell);
	}
}

FVCellSerializeContext::FBatch FVCellSerializeContext::BuildBatch(VCell* Root)
{
	FVCellSerializeContext::FBatch Batch;
	FVCellSerializeContextBatchBuilder Builder(*this, Batch);
	int CurrentCells = ExportMap.Num();
	Builder.Process(Root);
	check(ExportMap.Num() - CurrentCells == Batch.Exports.Num());
	return Batch;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
