#pragma once

#ifdef PDBREADER_EXPORTS
	#define PDBREADER_API __declspec(dllexport)
#else
	#define PDBREADER_API __declspec(dllimport)
#endif

#include <cstdint>

namespace PDB::CodeView::TPI
{
	struct Record;
}

struct PDBHandle;

struct FStructFieldCounts;
struct FDataMemberInfo;
struct FRecordBasicInfo;

extern "C" PDBREADER_API PDBHandle* LoadPDB(const wchar_t* path);
extern "C" PDBREADER_API void ClosePDB(PDBHandle* handle);

extern "C" PDBREADER_API void GetTypeRecordsInfo(
	PDBHandle* handle,
	const void*** OutRecords,
	uint64_t* OutRecordCount,
	uint32_t* OutTypeIndexBegin,
	uint32_t* OutTypeIndexEnd
	);

extern "C" PDBREADER_API void GetBasicTypeInfo(
	PDBHandle* handle,
	uint32_t index,
	FRecordBasicInfo* outInfo
	);

extern "C" PDBREADER_API void CountNumFields(
	PDBHandle* handle,
	const void* fieldListRecord,
	FStructFieldCounts* outNumFields
	);

extern "C" PDBREADER_API void ReadDataMembers(
	PDBHandle* handle,
	const void* fieldListRecord,
	FDataMemberInfo* outMembers
	);
