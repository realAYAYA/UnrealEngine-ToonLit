#include "PDBReader.h"
#include "MemoryMappedFile.h"

#include <new>
#include <cstring>

#include "raw_pdb/PDB.h"
#include "raw_pdb/PDB_RawFile.h"
#include "raw_pdb/PDB_InfoStream.h"
#include "raw_pdb/PDB_TPIStream.h"
#include "raw_pdb/PDB_DBIStream.h"

enum class EModifierFlags : uint16_t
{
	None = 0x0,
	Const = 0x1,
	Volatile = 0x2,
	Unaligned = 0x4,
};

struct FRecordBasicInfo
{
	const char* Name = nullptr;
	uint64_t Size = 0;
	uint32_t RelatedRecord = 0;
	EModifierFlags Modifiers = EModifierFlags::None;
	PDB::CodeView::TPI::TypeRecordKind Kind = (PDB::CodeView::TPI::TypeRecordKind)0;
};

namespace PrimitiveTypes
{
	const FRecordBasicInfo Unknown = {"UNKNOWN", 0, 0};
	const FRecordBasicInfo None = { "NONE", 0, 0};
	const FRecordBasicInfo Void[] = {
		{"void", 0, 0},
		{"void*", 2, 0},
		{"void*", 4, 0},
		{"void*", 4, 0},
		{"void*", 4, 0},
		{"void*", 4, 0},
		{"void*", 8, 0},
	};
	const FRecordBasicInfo Hresult[] = {
		{"HRESULT", 4, 0},
		{"HRESULT*", 2, 0},
		{"HRESULT*", 4, 0},
		{"HRESULT*", 4, 0},
		{"HRESULT*", 4, 0},
		{"HRESULT*", 4, 0},
		{"HRESULT*", 8, 0},
	};
	const FRecordBasicInfo Int8[] = {
		{"int8", 1, 0},
		{"int8*", 2, 0},
		{"int8*", 4, 0},
		{"int8*", 4, 0},
		{"int8*", 4, 0},
		{"int8*", 4, 0},
		{"int8*", 8, 0},
	};
	const FRecordBasicInfo Uint8[] = {
		{"uint8", 1, 0},
		{"uint8*", 2, 0},
		{"uint8*", 4, 0},
		{"uint8*", 4, 0},
		{"uint8*", 4, 0},
		{"uint8*", 4, 0},
		{"uint8*", 8, 0},
	};
	const FRecordBasicInfo Int16[] = {
		{"int16", 2, 0},
		{"int16*", 2, 0},
		{"int16*", 4, 0},
		{"int16*", 4, 0},
		{"int16*", 4, 0},
		{"int16*", 4, 0},
		{"int16*", 8, 0},
	};
	const FRecordBasicInfo Uint16[] = {
		{"uint16", 2, 0},
		{"uint16*", 2, 0},
		{"uint16*", 4, 0},
		{"uint16*", 4, 0},
		{"uint16*", 4, 0},
		{"uint16*", 4, 0},
		{"uint16*", 8, 0},
	};
	const FRecordBasicInfo Int32[] = {
		{"int32", 4, 0},
		{"int32*", 2, 0},
		{"int32*", 4, 0},
		{"int32*", 4, 0},
		{"int32*", 4, 0},
		{"int32*", 4, 0},
		{"int32*", 8, 0},
	};
	const FRecordBasicInfo Uint32[] = {
		{"uint32", 4, 0},
		{"uint32*", 2, 0},
		{"uint32*", 4, 0},
		{"uint32*", 4, 0},
		{"uint32*", 4, 0},
		{"uint32*", 4, 0},
		{"uint32*", 8, 0},
	};
	const FRecordBasicInfo Int64[] = {
		{"int64", 8, 0},
		{"int64*", 2, 0},
		{"int64*", 4, 0},
		{"int64*", 4, 0},
		{"int64*", 4, 0},
		{"int64*", 4, 0},
		{"int64*", 8, 0},
	};
	const FRecordBasicInfo Uint64[] = {
		{"uint64", 8, 0},
		{"uint64*", 2, 0},
		{"uint64*", 4, 0},
		{"uint64*", 4, 0},
		{"uint64*", 4, 0},
		{"uint64*", 4, 0},
		{"uint64*", 8, 0},
	};
	const FRecordBasicInfo Bool8[] = {
		{"bool8", 1, 0},
		{"bool8*", 2, 0},
		{"bool8*", 4, 0},
		{"bool8*", 4, 0},
		{"bool8*", 4, 0},
		{"bool8*", 4, 0},
		{"bool8*", 8, 0},
	};
	const FRecordBasicInfo Bool16[] = {
		{"bool16", 2, 0},
		{"bool16*", 2, 0},
		{"bool16*", 4, 0},
		{"bool16*", 4, 0},
		{"bool16*", 4, 0},
		{"bool16*", 4, 0},
		{"bool16*", 8, 0},
	};
	const FRecordBasicInfo Bool32[] = {
		{"bool32", 4, 0},
		{"bool32*", 2, 0},
		{"bool32*", 4, 0},
		{"bool32*", 4, 0},
		{"bool32*", 4, 0},
		{"bool32*", 4, 0},
		{"bool32*", 8, 0},
	};
	const FRecordBasicInfo Bool64[] = {
		{"bool64", 8, 0},
		{"bool64*", 2, 0},
		{"bool64*", 4, 0},
		{"bool64*", 4, 0},
		{"bool64*", 4, 0},
		{"bool64*", 4, 0},
		{"bool64*", 8, 0},
	};
	const FRecordBasicInfo Float[] = {
		{"float", 4, 0},
		{"float*", 2, 0},
		{"float*", 4, 0},
		{"float*", 4, 0},
		{"float*", 4, 0},
		{"float*", 4, 0},
		{"float*", 8, 0},
	};
	const FRecordBasicInfo Double[] = {
		{"double", 8, 0},
		{"double*", 2, 0},
		{"double*", 4, 0},
		{"double*", 4, 0},
		{"double*", 4, 0},
		{"double*", 4, 0},
		{"double*", 8, 0},
	};
	const FRecordBasicInfo Char[] = {
		{"char", 1, 0},
		{"char*", 2, 0},
		{"char*", 4, 0},
		{"char*", 4, 0},
		{"char*", 4, 0},
		{"char*", 4, 0},
		{"char*", 8, 0},
	};
	const FRecordBasicInfo Wchar[] = {
		{"wchar", 2, 0},
		{"wchar*", 2, 0},
		{"wchar*", 4, 0},
		{"wchar*", 4, 0},
		{"wchar*", 4, 0},
		{"wchar*", 4, 0},
		{"wchar*", 8, 0},
	};
}

namespace TPIHelpers
{
	FRecordBasicInfo GetPrimitiveTypeBasicInfo(uint32_t typeIndex)
	{
		uint64_t sizeField = typeIndex & 0x7;
		uint64_t typeField = (typeIndex >> 4) & 0xf;
		uint64_t modeField = (typeIndex >> 8) & 0x7; // For pointers only 

		bool isPointer = true;
		uint32_t pointerSize = 0;

		// Pointers
		switch (modeField)
		{
			case 1:
				pointerSize = 2;
				break;
			case 2:
			case 3:
			case 4:
			case 5:
				pointerSize = 4;
				break;
			case 6:
				pointerSize = 8;
				break;
			case 0:
			default:
				isPointer = false;
				break;
		}

		switch (typeField)
		{
			default: return PrimitiveTypes::Unknown;
			// "special"
			case 0: 
			{
				switch (sizeField)
				{
					case 0: return PrimitiveTypes::None;
					case 1: // Absolute symbol
					case 2: // Segment
						return PrimitiveTypes::Unknown; 
					case 3: return PrimitiveTypes::Void[modeField];
					case 4: // 8 byte currency valye
					case 5: // near basic string
					case 6: // far basic string
					case 7: // untranslated legacy type
					default:
						return PrimitiveTypes::Unknown;
					case 8: return PrimitiveTypes::Hresult[modeField];
				}
			}
			// Signed integer
			case 1:
				switch (sizeField)
				{
					default: return PrimitiveTypes::Unknown;
					case 0: return PrimitiveTypes::Int8[modeField];
					case 1: return PrimitiveTypes::Int16[modeField];
					case 2: return PrimitiveTypes::Int32[modeField];
					case 3: return PrimitiveTypes::Int64[modeField];
				}
			// Unsigned integer
			case 2:
				switch (sizeField)
				{
					default: return PrimitiveTypes::Unknown;
					case 0: return PrimitiveTypes::Uint8[modeField];
					case 1: return PrimitiveTypes::Uint16[modeField];
					case 2: return PrimitiveTypes::Uint32[modeField];
					case 3: return PrimitiveTypes::Uint64[modeField];
				}
			// Boolean
			case 3:
				switch (sizeField)
				{
					default: return PrimitiveTypes::Unknown;
					case 0: return PrimitiveTypes::Bool8[modeField];
					case 1: return PrimitiveTypes::Bool16[modeField];
					case 2: return PrimitiveTypes::Bool32[modeField];
					case 3: return PrimitiveTypes::Bool64[modeField];
				}
			// real (float?)
			case 4:
				switch (sizeField)
				{
					default: return PrimitiveTypes::Unknown;
					case 0: return PrimitiveTypes::Float[modeField];
					case 1: return PrimitiveTypes::Double[modeField];
					case 2: // 80 bit 
					case 3: // 128 bit
					case 4: // 28 bit
						return PrimitiveTypes::Unknown;
				}
			// complex
			case 5:
				return PrimitiveTypes::Unknown;
			// special 2
			case 6:
				switch (sizeField)
				{
					default:
					case 0: // Bit
					case 1: // Pascal CHAR
						return PrimitiveTypes::Unknown;
				}
				break;
			// real int 
			case 7:
				switch (sizeField)
				{
					default: return PrimitiveTypes::Unknown;
					case 0: return PrimitiveTypes::Char[modeField];
					case 1: return PrimitiveTypes::Wchar[modeField];
					case 2: return PrimitiveTypes::Int16[modeField];
					case 3: return PrimitiveTypes::Uint16[modeField];
					case 4: return PrimitiveTypes::Int32[modeField];
					case 5: return PrimitiveTypes::Uint32[modeField];
					case 6: return PrimitiveTypes::Int64[modeField];
					case 7: return PrimitiveTypes::Uint64[modeField];
				}
		}
	}

	uint64_t GetPrimitiveTypeSize(uint32_t typeIndex)
	{
		return GetPrimitiveTypeBasicInfo(typeIndex).Size;
	}
}

struct PDBHandle
{
	PDBHandle(MemoryMappedFile::Handle handle, PDB::TPIStream tpiStream);
	~PDBHandle();

	MemoryMappedFile::Handle FileHandle;
	PDB::TPIStream TPIStream;
};

PDBHandle::PDBHandle(MemoryMappedFile::Handle handle, PDB::TPIStream tpiStream)
: FileHandle(handle)
, TPIStream(std::move(tpiStream))
{
}

PDBHandle::~PDBHandle()
{
	MemoryMappedFile::Close(FileHandle);
}

extern "C" PDBREADER_API PDBHandle* LoadPDB(const wchar_t* path)
{
	MemoryMappedFile::Handle pdbFile = MemoryMappedFile::Open(path);
	if (!pdbFile.baseAddress)
	{
		return nullptr;
	}

	if (PDB::ValidateFile(pdbFile.baseAddress) != PDB::ErrorCode::Success)
	{
		MemoryMappedFile::Close(pdbFile);
		return nullptr;
	}

	const PDB::RawFile rawPdbFile = PDB::CreateRawFile(pdbFile.baseAddress);
	if (PDB::HasValidDBIStream(rawPdbFile) != PDB::ErrorCode::Success)
	{
		MemoryMappedFile::Close(pdbFile);
		return nullptr;
	}

	const PDB::InfoStream infoStream(rawPdbFile);
	if (infoStream.UsesDebugFastLink())
	{
		MemoryMappedFile::Close(pdbFile);
		return nullptr;
	}

	PDB::TPIStream tpiStream = PDB::CreateTPIStream(rawPdbFile);
	if (PDB::HasValidTPIStream(rawPdbFile) != PDB::ErrorCode::Success)
	{
		MemoryMappedFile::Close(pdbFile);

		return nullptr;
	}

	return new PDBHandle(pdbFile, std::move(tpiStream));
}

extern "C" PDBREADER_API void ClosePDB(PDBHandle* handle)
{
	delete handle;
}

extern "C" PDBREADER_API int64_t CountStructures(PDBHandle* handle)
{
	int64_t count = 0;
	for (const PDB::CodeView::TPI::Record* record : handle->TPIStream.GetTypeRecords())
	{
		if (record->header.kind == PDB::CodeView::TPI::TypeRecordKind::LF_STRUCTURE)
		{
			if (record->data.LF_CLASS.property.fwdref)
				continue;

			const PDB::CodeView::TPI::Record* typeRecord = handle->TPIStream.GetTypeRecord(record->data.LF_CLASS.field);
			if (!typeRecord)
				continue;

			++count;
		}
	}
	return count;
}


static uint8_t GetLeafSize(PDB::CodeView::TPI::TypeRecordKind kind)
{
	if (kind < PDB::CodeView::TPI::TypeRecordKind::LF_NUMERIC)
	{
		// No leaf can have an index less than LF_NUMERIC (0x8000)
		// so word is the value...
		return sizeof(PDB::CodeView::TPI::TypeRecordKind);
	}

	switch (kind)
	{
	case PDB::CodeView::TPI::TypeRecordKind::LF_CHAR:
		return sizeof(PDB::CodeView::TPI::TypeRecordKind) + sizeof(uint8_t);

	case PDB::CodeView::TPI::TypeRecordKind::LF_USHORT:
	case PDB::CodeView::TPI::TypeRecordKind::LF_SHORT:
		return sizeof(PDB::CodeView::TPI::TypeRecordKind) + sizeof(uint16_t);

	case PDB::CodeView::TPI::TypeRecordKind::LF_LONG:
	case PDB::CodeView::TPI::TypeRecordKind::LF_ULONG:
		return sizeof(PDB::CodeView::TPI::TypeRecordKind) + sizeof(uint32_t);

	case PDB::CodeView::TPI::TypeRecordKind::LF_QUADWORD:
	case PDB::CodeView::TPI::TypeRecordKind::LF_UQUADWORD:
		return sizeof(PDB::CodeView::TPI::TypeRecordKind) + sizeof(uint64_t);

	default:
		printf("Error! 0x%04x bogus type encountered, aborting...\n", PDB_AS_UNDERLYING(kind));
	}
	return 0;
}

static size_t GetStructureSizeSize(PDB::CodeView::TPI::TypeRecordKind kind)
{
	if (kind < PDB::CodeView::TPI::TypeRecordKind::LF_NUMERIC)
	{
		// No leaf can have an index less than LF_NUMERIC (0x8000)
		// so word is the value...
		return 0;
	}

	switch (kind)
	{
	case PDB::CodeView::TPI::TypeRecordKind::LF_CHAR:
		return sizeof(uint8_t);

	case PDB::CodeView::TPI::TypeRecordKind::LF_USHORT:
	case PDB::CodeView::TPI::TypeRecordKind::LF_SHORT:
		return sizeof(uint16_t);

	case PDB::CodeView::TPI::TypeRecordKind::LF_LONG:
	case PDB::CodeView::TPI::TypeRecordKind::LF_ULONG:
		return sizeof(uint32_t);

	case PDB::CodeView::TPI::TypeRecordKind::LF_QUADWORD:
	case PDB::CodeView::TPI::TypeRecordKind::LF_UQUADWORD:
		return sizeof(uint64_t);

	default:
		printf("Error! 0x%04x bogus type encountered, aborting...\n", PDB_AS_UNDERLYING(kind));
	}
	return 0;
}

static uint64_t GetStructureSize(const char* data, PDB::CodeView::TPI::TypeRecordKind kind)
{
	size_t sizeSize = GetStructureSizeSize(kind);
	if (sizeSize == 0)
	{
		return *reinterpret_cast<const uint16_t*>(data);
	}

	const char* sizePtr = &data[GetLeafSize(kind) - sizeSize];
	switch (sizeSize)
	{
	case 1: return *reinterpret_cast<const uint8_t*>(sizePtr);
	case 2: return *reinterpret_cast<const uint16_t*>(sizePtr);
	case 4: return *reinterpret_cast<const uint32_t*>(sizePtr);
	case 8: return *reinterpret_cast<const uint64_t*>(sizePtr);
	default: return 0;
	}
}

static const char* GetLeafName(const char* data, PDB::CodeView::TPI::TypeRecordKind kind)
{
	return &data[GetLeafSize(kind)];
}

struct FNumericLeaf
{
	PDB::CodeView::TPI::TypeRecordKind Kind;
	union Value
	{
		uint64_t UnsignedInt;
		int64_t SignedInt;
	} Value;

	FNumericLeaf(PDB::CodeView::TPI::TypeRecordKind kind, uint64_t value)
	: Kind(kind)
	{
		Value.UnsignedInt = value;
	}

	size_t GetSkipSize() const
	{
		switch (Kind)
		{
		case PDB::CodeView::TPI::TypeRecordKind::LF_CHAR:
			return sizeof(uint8_t);

		case PDB::CodeView::TPI::TypeRecordKind::LF_USHORT:
		case PDB::CodeView::TPI::TypeRecordKind::LF_SHORT:
			return sizeof(uint16_t);

		case PDB::CodeView::TPI::TypeRecordKind::LF_LONG:
		case PDB::CodeView::TPI::TypeRecordKind::LF_ULONG:
			return sizeof(uint32_t);

		case PDB::CodeView::TPI::TypeRecordKind::LF_QUADWORD:
		case PDB::CodeView::TPI::TypeRecordKind::LF_UQUADWORD:
			return sizeof(uint64_t);
		}
		return 0;
	}
};


FNumericLeaf ReadNumericLeaf(const uint8_t*& address)
{
	using PDB::CodeView::TPI::TypeRecordKind;
	uint16_t size = *reinterpret_cast<const uint16_t*>(address);
	if (size < (uint16_t)TypeRecordKind::LF_NUMERIC)
	{
		address = reinterpret_cast<const uint8_t*>(address) + sizeof(uint16_t);
		return FNumericLeaf(TypeRecordKind::LF_NUMERIC, size);
	}
	address += sizeof(uint16_t);
	FNumericLeaf leaf(TypeRecordKind::LF_NUMERIC, size);
	TypeRecordKind kind = (TypeRecordKind)size;
	switch (kind)
	{
	default:
		PDB_ASSERT(false, "Unrecognized numeric leaf kind %X", size);
		return FNumericLeaf(TypeRecordKind::LF_NUMERIC, 0);
	case TypeRecordKind::LF_CHAR:
		leaf = FNumericLeaf(kind, *reinterpret_cast<const int8_t*>(address));
		break;
	case TypeRecordKind::LF_SHORT:
		leaf = FNumericLeaf(kind, *reinterpret_cast<const int16_t*>(address));
		break;
	case TypeRecordKind::LF_USHORT:
		leaf = FNumericLeaf(kind, *reinterpret_cast<const uint16_t*>(address));
		break;
	case TypeRecordKind::LF_LONG:
		leaf = FNumericLeaf(kind, *reinterpret_cast<const int32_t*>(address));
		break;
	case TypeRecordKind::LF_ULONG:
		leaf = FNumericLeaf(kind, *reinterpret_cast<const uint32_t*>(address));
		break;
	case TypeRecordKind::LF_QUADWORD:
		leaf = FNumericLeaf(kind, *reinterpret_cast<const int64_t*>(address));
		break;
	case TypeRecordKind::LF_UQUADWORD:
		leaf = FNumericLeaf(kind, *reinterpret_cast<const uint64_t*>(address));
		break;
	case TypeRecordKind::LF_REAL32:
	case TypeRecordKind::LF_REAL64:
	case TypeRecordKind::LF_REAL48:
	case TypeRecordKind::LF_REAL80:
	case TypeRecordKind::LF_REAL128:
		PDB_ASSERT(false, "Floating point leaves unimplemented");
		break;
	case TypeRecordKind::LF_COMPLEX32:
	case TypeRecordKind::LF_COMPLEX64:
	case TypeRecordKind::LF_COMPLEX80:
	case TypeRecordKind::LF_COMPLEX128:
		PDB_ASSERT(false, "Complex leaves unsupported");
		break;
	case TypeRecordKind::LF_VARSTRING:
		PDB_ASSERT(false, "Varstring leaves unsupported");
		break;
	}
	address += leaf.GetSkipSize();
	return leaf;
}

FNumericLeaf ReadNumericLeaf(const void*& address)
{
	return ReadNumericLeaf(reinterpret_cast<const uint8_t*&>(address));
}

extern "C" void GetTypeRecordsInfo(
	PDBHandle* handle,
	const void*** OutRecords,
	uint64_t* OutRecordCount,
	uint32_t* OutTypeIndexBegin,
	uint32_t* OutTypeIndexEnd
)
{
	PDB::TPIStream& stream = handle->TPIStream;
	*OutRecords = (const void**)stream.GetTypeRecords().begin();
	*OutRecordCount = stream.GetTypeRecords().GetLength();
	*OutTypeIndexBegin = stream.GetFirstTypeIndex();
	*OutTypeIndexEnd = stream.GetLastTypeIndex();
}

// Recursion to other referenced types can be problematic here as they may be forward declarations
// e.g. an LF_MODIFIER may point to the forward decl version of an LF_CLASS 
extern "C" void GetBasicTypeInfo(PDBHandle* handle, uint32_t index, FRecordBasicInfo* outInfo)
{
	using PDB::CodeView::TPI::TypeRecordKind;
	using namespace TPIHelpers;

	const PDB::CodeView::TPI::Record* record = handle->TPIStream.GetTypeRecord(index);
	if (!record)
	{
		*outInfo = GetPrimitiveTypeBasicInfo(index);
		return;
	}

	const PDB::CodeView::TPI::Record* underlyingRecord = nullptr;
	FRecordBasicInfo underlyingInfo;
	const uint8_t* data = nullptr;
	outInfo->Kind = record->header.kind;
	switch (record->header.kind)
	{
	default:
		PDB_ASSERT(false, "Unknown record kind %X", static_cast<unsigned int>(record->header.kind));
		return;
	case TypeRecordKind::LF_MFUNCTION:
		outInfo->Name = "MEMBER FUNCTION";
		return;
	case TypeRecordKind::LF_PROCEDURE:
		outInfo->Name = "FUNCTION"; // TODO: proper names for function ptrs 
		return;
	case TypeRecordKind::LF_VTSHAPE:
		return;
	case TypeRecordKind::LF_STRUCTURE:
	case TypeRecordKind::LF_CLASS:
		outInfo->Name = GetLeafName(record->data.LF_CLASS.data, record->data.LF_CLASS.lfEasy.kind);
		outInfo->Size = GetStructureSize(record->data.LF_CLASS.data, record->data.LF_CLASS.lfEasy.kind);
		outInfo->RelatedRecord = record->data.LF_CLASS.field;
		return;
	case TypeRecordKind::LF_POINTER:
		outInfo->RelatedRecord = record->data.LF_POINTER.utype;
		outInfo->Size = record->data.LF_POINTER.attr.size;
		return;
	case TypeRecordKind::LF_MODIFIER:
		outInfo->RelatedRecord = record->data.LF_MODIFIER.type;
		outInfo->Modifiers = *reinterpret_cast<const EModifierFlags*>(&record->data.LF_MODIFIER.attr);
		return;
	case TypeRecordKind::LF_ENUM:
		outInfo->Name = record->data.LF_ENUM.name;
		outInfo->RelatedRecord = record->data.LF_ENUM.utype;
		return;
	case TypeRecordKind::LF_ARRAY:
		outInfo->RelatedRecord = record->data.LF_ARRAY.elemtype;
		data = reinterpret_cast<const uint8_t*>(record->data.LF_ARRAY.data);
		outInfo->Size = ReadNumericLeaf(data).Value.UnsignedInt;
		outInfo->Name = reinterpret_cast<const char*>(data);
		return;
	case TypeRecordKind::LF_UNION:
		outInfo->Name = GetLeafName(record->data.LF_CLASS.data, record->data.LF_CLASS.lfEasy.kind);
		outInfo->Size = GetStructureSize(record->data.LF_CLASS.data, record->data.LF_CLASS.lfEasy.kind);
		outInfo->RelatedRecord = record->data.LF_CLASS.field;
		return;
	}
}

template<typename T>
const uint8_t* AlignUp(const uint8_t* address)
{
	uintptr_t ptr = (uintptr_t)address;
	ptr = (ptr + (sizeof(uint32_t) - 1)) & (0 - sizeof(uint32_t));
	return reinterpret_cast<const uint8_t*>(ptr);
}

const uint8_t* SkipFieldListEntry(const PDB::CodeView::TPI::FieldList& entry)
{
	using PDB::CodeView::TPI::TypeRecordKind;
	const uint8_t* Addr = nullptr;
	volatile static bool bLog = false;
	switch (entry.kind)
	{
	default:
		PDB_ASSERT(false, "Unknown record kind %X", static_cast<unsigned int>(entry.kind));
		return 0;
	case TypeRecordKind::LF_MEMBER:
		Addr = reinterpret_cast<const uint8_t*>(&entry.data.LF_MEMBER.offset);
		ReadNumericLeaf(Addr);
		Addr += strlen((const char*)Addr) + 1; // ? Name should be length prefixed 
		return AlignUp<uint32_t>(Addr);
	case TypeRecordKind::LF_NESTTYPE:
		Addr = reinterpret_cast<const uint8_t*>(entry.data.LF_NESTTYPE.name);
		Addr += strlen((const char*)Addr) + 1;
		return AlignUp<uint32_t>(Addr);
	case TypeRecordKind::LF_STMEMBER:
		Addr = reinterpret_cast<const uint8_t*>(entry.data.LF_STMEMBER.name);
		Addr += strlen((const char*)Addr) + 1;
		return AlignUp<uint32_t>(Addr);
	case TypeRecordKind::LF_METHOD:
		Addr = reinterpret_cast<const uint8_t*>(entry.data.LF_METHOD.name);
		Addr += strlen((const char*)Addr) + 1;
		return AlignUp<uint32_t>(Addr);
	case TypeRecordKind::LF_ONEMETHOD:
		Addr = reinterpret_cast<const uint8_t*>(&entry.data.LF_ONEMETHOD.vbaseoff);
		switch (static_cast<PDB::CodeView::TPI::MethodProperty>(entry.data.LF_ONEMETHOD.attributes.mprop))
		{
		case PDB::CodeView::TPI::MethodProperty::Intro:
		case PDB::CodeView::TPI::MethodProperty::PureIntro:
			Addr += sizeof(uint32_t);
		}
		Addr += strlen((const char*)Addr) + 1;
		return AlignUp<uint32_t>(Addr);
	case TypeRecordKind::LF_BCLASS:
		Addr = reinterpret_cast<const uint8_t*>(&entry.data.LF_BCLASS.offset);
		ReadNumericLeaf(Addr);
		return AlignUp<uint32_t>(Addr);
	case TypeRecordKind::LF_VFUNCTAB:
		Addr = reinterpret_cast<const uint8_t*>(&entry.data.LF_VFUNCTAB) + sizeof(entry.data.LF_VFUNCTAB);
		return AlignUp<uint32_t>(Addr);
	case TypeRecordKind::LF_IVBCLASS:
	case TypeRecordKind::LF_VBCLASS:
		Addr = reinterpret_cast<const uint8_t*>(&entry.data.LF_VBCLASS.lfEasy);
		ReadNumericLeaf(Addr);
		ReadNumericLeaf(Addr);
		return AlignUp<uint32_t>(Addr);

	}
}

struct FWalkFieldListOperations 
{
	template<typename T>
	void Visit(const T&){}
};

template<typename Operations = FWalkFieldListOperations>
void WalkFieldList(PDBHandle* handle, const PDB::CodeView::TPI::Record* record, Operations& op)
{
	const PDB::TPIStream& tpiStream = handle->TPIStream;

	using PDB::CodeView::TPI::TypeRecordKind;
	const uint8_t* fieldData = reinterpret_cast<const uint8_t*>(&record->data.LF_FIELD.list);
	const uint8_t* dataEnd = fieldData + (record->header.size - sizeof(uint16_t));
	while(fieldData < dataEnd)
	{
		const auto* fieldRecord = reinterpret_cast<const PDB::CodeView::TPI::FieldList*>(fieldData);
		if (fieldRecord->kind == TypeRecordKind::LF_INDEX)
		{
			const PDB::CodeView::TPI::Record* newRecord = handle->TPIStream.GetTypeRecord(fieldRecord->data.LF_INDEX.index);
			WalkFieldList(handle, newRecord, op);
			return;
		}

		fieldData = SkipFieldListEntry(*fieldRecord);
		switch (fieldRecord->kind)
		{
		default:
			PDB_ASSERT(false, "Unknown record kind %X", static_cast<unsigned int>(fieldRecord->kind));
			return;
		case TypeRecordKind::LF_MEMBER:
			op.Visit(fieldRecord, fieldRecord->data.LF_MEMBER);
			break;
		case TypeRecordKind::LF_NESTTYPE:
			op.Visit(fieldRecord, fieldRecord->data.LF_NESTTYPE);
			break;
		case TypeRecordKind::LF_STMEMBER:
			op.Visit(fieldRecord, fieldRecord->data.LF_STMEMBER);
			break;
		case TypeRecordKind::LF_METHOD:
			op.Visit(fieldRecord, fieldRecord->data.LF_METHOD);
			break;
		case TypeRecordKind::LF_ONEMETHOD:
			op.Visit(fieldRecord, fieldRecord->data.LF_ONEMETHOD);
			break;
		case TypeRecordKind::LF_BCLASS:
			op.Visit(fieldRecord, fieldRecord->data.LF_BCLASS);
			break;
		case TypeRecordKind::LF_IVBCLASS:
		case TypeRecordKind::LF_VBCLASS:
			op.Visit(fieldRecord, fieldRecord->data.LF_VBCLASS);
			break;
		case TypeRecordKind::LF_VFUNCTAB:
			op.Visit(fieldRecord, fieldRecord->data.LF_VFUNCTAB);
			break;
		}
	}
}

struct FStructFieldCounts
{
	uint32_t NumDataMembers = 0;
};

struct FFieldCountingOperations
{
	using LF_MEMBER = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_MEMBER);
	using LF_NESTTYPE = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_NESTTYPE);
	using LF_STMEMBER = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_STMEMBER);
	using LF_METHOD = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_METHOD);
	using LF_ONEMETHOD = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_ONEMETHOD);
	using LF_BCLASS = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_BCLASS);
	using LF_VBCLASS = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_VBCLASS);
	using LF_VFUNCTAB = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_VFUNCTAB);

	void Visit(const PDB::CodeView::TPI::FieldList*, const LF_MEMBER&) { ++Count.NumDataMembers; }
	void Visit(const PDB::CodeView::TPI::FieldList*, const LF_NESTTYPE&) {}
	void Visit(const PDB::CodeView::TPI::FieldList*, const LF_STMEMBER&) {}
	void Visit(const PDB::CodeView::TPI::FieldList*, const LF_METHOD&) {}
	void Visit(const PDB::CodeView::TPI::FieldList*, const LF_ONEMETHOD&) {}
	void Visit(const PDB::CodeView::TPI::FieldList*, const LF_BCLASS&) { ++Count.NumDataMembers; }
	void Visit(const PDB::CodeView::TPI::FieldList*, const LF_VBCLASS&) { }
	void Visit(const PDB::CodeView::TPI::FieldList*, const LF_VFUNCTAB&) { ++Count.NumDataMembers; }

	FStructFieldCounts Count;
};

extern "C" PDBREADER_API void CountNumFields(
	PDBHandle* handle,
	const void* fieldListRecord,
	FStructFieldCounts* outNumFields
)
{
	const PDB::CodeView::TPI::Record* record = reinterpret_cast<const PDB::CodeView::TPI::Record*>(fieldListRecord);
	FFieldCountingOperations op;
	WalkFieldList(handle, record, op);
	*outNumFields = op.Count;
}

struct FFieldAddressOperations
{
	using LF_MEMBER = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_MEMBER);
	using LF_NESTTYPE = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_NESTTYPE);
	using LF_STMEMBER = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_STMEMBER);
	using LF_METHOD = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_METHOD);
	using LF_ONEMETHOD = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_ONEMETHOD);
	using LF_BCLASS = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_BCLASS);
	using LF_VBCLASS = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_VBCLASS);
	using LF_VFUNCTAB = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_VFUNCTAB);

	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_MEMBER&) { *(Pointers++) = Entry; }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_NESTTYPE&) { *(Pointers++) = Entry; }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_STMEMBER&) { *(Pointers++) = Entry; }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_METHOD&) { *(Pointers++) = Entry; }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_ONEMETHOD&) { *(Pointers++) = Entry; }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_BCLASS&) { *(Pointers++) = Entry; }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_VBCLASS&) { *(Pointers++) = Entry; }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_VFUNCTAB&) { *(Pointers++) = Entry; }

	const void** Pointers;
	FFieldAddressOperations(const void** inOutPointers)
	: Pointers(inOutPointers)
	{
	}
};

extern "C" PDBREADER_API void GetFieldRecords(
	PDBHandle* handle,
	const void* fieldListRecord,
	const void** inOutPointers,
	int32_t numPointers
)
{
	const PDB::CodeView::TPI::Record* record = reinterpret_cast<const PDB::CodeView::TPI::Record*>(fieldListRecord);
	FFieldAddressOperations op(inOutPointers);
	WalkFieldList(handle, record, op);
}

enum class DataMemberFlags
{
	None = 0x0,
	IsVtable = 0x1,
	IsBitfield = 0x2,
	IsBaseClass = 0x4,
};

static DataMemberFlags operator|(DataMemberFlags a, DataMemberFlags b)
{
	return (DataMemberFlags)((int32_t)a | (int32_t)b);
}

struct FDataMemberInfo
{
	const char* Name;
	uint64_t Offset;
	uint32_t TypeIndex;
	DataMemberFlags Flags;
	uint32_t BitPosition;
	uint32_t BitSize;
};


struct FReadDataMemberOperations
{
	using LF_MEMBER = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_MEMBER);
	using LF_NESTTYPE = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_NESTTYPE);
	using LF_STMEMBER = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_STMEMBER);
	using LF_METHOD = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_METHOD);
	using LF_ONEMETHOD = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_ONEMETHOD);
	using LF_BCLASS = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_BCLASS);
	using LF_VBCLASS = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_VBCLASS);
	using LF_VFUNCTAB = decltype(std::declval<PDB::CodeView::TPI::FieldList>().data.LF_VFUNCTAB);

	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_NESTTYPE&) { }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_STMEMBER&) { }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_METHOD&) { }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_ONEMETHOD&) { }
	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_VBCLASS&) { }

	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_MEMBER& data) 
	{ 
		FDataMemberInfo& outInfo = *(MemberInfos++);

		const uint8_t* addr = reinterpret_cast<const uint8_t*>(data.offset);
		FNumericLeaf offset = ReadNumericLeaf(addr);
		outInfo.Offset = offset.Value.UnsignedInt; // Shouldn't need to deal with signed int64 offset?
		outInfo.Name = reinterpret_cast<const char*>(addr);
		outInfo.TypeIndex = data.index;
		outInfo.Flags = DataMemberFlags::None;

		if (data.index >= PDB->TPIStream.GetFirstTypeIndex())
		{
			using PDB::CodeView::TPI::TypeRecordKind;
			const PDB::CodeView::TPI::Record* typeRecord = PDB->TPIStream.GetTypeRecord(data.index);
			switch (typeRecord->header.kind)
			{
			// TODO: other type sizes 
			case TypeRecordKind::LF_BITFIELD:
				outInfo.Flags = outInfo.Flags | DataMemberFlags::IsBitfield;
				outInfo.BitPosition = typeRecord->data.LF_BITFIELD.position;
				outInfo.BitSize = typeRecord->data.LF_BITFIELD.length;
				outInfo.TypeIndex = typeRecord->data.LF_BITFIELD.type;
				break;
			}
		}
	}

	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_BCLASS& data) 
	{ 
		FDataMemberInfo& outInfo = *(MemberInfos++);

		const uint8_t* addr = reinterpret_cast<const uint8_t*>(data.offset);
		FNumericLeaf offset = ReadNumericLeaf(addr);
		outInfo.Offset = offset.Value.UnsignedInt; // Shouldn't need to deal with signed int64 offset?
		outInfo.Name = nullptr;
		outInfo.TypeIndex = data.index;
		outInfo.Flags = DataMemberFlags::IsBaseClass;
	}

	void Visit(const PDB::CodeView::TPI::FieldList* Entry, const LF_VFUNCTAB& data) 
	{
		FDataMemberInfo& outInfo = *(MemberInfos++);

		outInfo.Offset = 0;
		outInfo.Name = nullptr;
		outInfo.TypeIndex = data.type;
		outInfo.Flags = DataMemberFlags::IsVtable;
	}

	PDBHandle* PDB;
	FDataMemberInfo* MemberInfos;

	FReadDataMemberOperations(PDBHandle* pdbHandle, FDataMemberInfo* inOutMemberInfos)
	: PDB(pdbHandle)
	, MemberInfos(inOutMemberInfos)
	{
	}
};

extern "C" PDBREADER_API void ReadDataMembers(
	PDBHandle* handle,
	const void* fieldListRecord,
	FDataMemberInfo* outMembers
	)
{
	const PDB::CodeView::TPI::Record* record = reinterpret_cast<const PDB::CodeView::TPI::Record*>(fieldListRecord);
	FReadDataMemberOperations op(handle, outMembers);
	WalkFieldList(handle, record, op);
}
