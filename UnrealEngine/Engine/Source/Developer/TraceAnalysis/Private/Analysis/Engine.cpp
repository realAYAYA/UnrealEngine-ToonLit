// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "Algo/BinarySearch.h"
#include "Algo/Sort.h"
#include "Algo/StableSort.h"
#include "Containers/ArrayView.h"
#include "CoreGlobals.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "StreamReader.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Analysis.h"
#include "Trace/Analyzer.h"
#include "Trace/Detail/Protocol.h"
#include "Trace/Detail/Transport.h"
#include "Transport/PacketTransport.h"
#include "Transport/Transport.h"
#include "Transport/TidPacketTransport.h"

namespace UE {
namespace Trace {

// {{{1 misc -------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
void SerializeToCborImpl(TArray<uint8>&, const IAnalyzer::FEventData&, uint32);

////////////////////////////////////////////////////////////////////////////////
class FFnv1aHash
{
public:
			FFnv1aHash() = default;
			FFnv1aHash(uint32 PrevResult)		{ Result = PrevResult; }
	void	Add(const ANSICHAR* String)			{ for (; *String; ++String) { Add(*String); } }
	void	Add(const uint8* Data, uint32 Size)	{ for (uint32 i = 0; i < Size; ++Data, ++i) { Add(*Data); } }
	void	Add(uint8 Value)					{ Result ^= Value; Result *= 0x01000193; }
	uint32	Get() const							{ return Result; }

private:
	uint32	Result = 0x811c9dc5;
	// uint32: bias=0x811c9dc5			prime=0x01000193
	// uint64: bias=0xcbf29ce484222325	prime=0x00000100000001b3;
};



// {{{1 aux-data ---------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
struct FAuxData
{
	const uint8*	Data;
	uint32			DataSize;
	uint16			FieldIndex;
	int8			FieldSizeAndType;
	uint8			bSigned		: 1;
	uint8			bFragmented	: 1;
	uint8			bOwnsData	: 1;
	uint8			_Unused		: 5;
};

////////////////////////////////////////////////////////////////////////////////
struct FAuxDataCollector
	: public TArray<FAuxData, TInlineAllocator<8>>
{
	using	Super = TArray<FAuxData, TInlineAllocator<8>>;

			~FAuxDataCollector() { Reset(); }
	void	Add(const FAuxData& Data);
	void	Defragment(FAuxData& Head);
	void	Reset();
};

////////////////////////////////////////////////////////////////////////////////
void FAuxDataCollector::Add(const FAuxData& Data)
{
	if (Num() == 0)
	{
		Super::Add(Data);
		return;
	}

	FAuxData* Prev = &Last();
	if (Prev->FieldIndex != Data.FieldIndex)
	{
		Super::Add(Data);
		return;
	}

	while (Prev > GetData())
	{
		if (Prev[-1].FieldIndex != Data.FieldIndex)
		{
			break;
		}

		--Prev;
	}

	Prev->bFragmented = 1;
	Super::Add(Data);
}

////////////////////////////////////////////////////////////////////////////////
void FAuxDataCollector::Defragment(FAuxData& Data)
{
	check(Data.bFragmented);

	uint32 Size = Data.DataSize;
	for (FAuxData* Read = &Data + 1;;)
	{
		Size += Read->DataSize;

		++Read;
		if (Read > &Last() || Read->FieldIndex != Data.FieldIndex)
		{
			break;
		}
	}

	uint8* Buffer = (uint8*)(FMemory::Malloc(Size));

	uint8* Write = Buffer;
	uint8* End = Buffer + Size;
	for (FAuxData* Read = &Data;; ++Read)
	{
		FMemory::Memcpy(Write, Read->Data, Read->DataSize);
		Write += Read->DataSize;
		if (Write >= End)
		{
			break;
		}
	}

	Data.Data = Buffer;
	Data.DataSize = Size;
	Data.bOwnsData = 1;
}

////////////////////////////////////////////////////////////////////////////////
void FAuxDataCollector::Reset()
{
	for (const FAuxData& AuxData : *this)
	{
		if (!AuxData.bOwnsData)
		{
			continue;
		}

		FMemory::Free((void*)(AuxData.Data));
	}

	Super::Reset();
}



// {{{1 threads ----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FThreads
{
public:
	struct FInfo
	{
		TArray<uint64>	ScopeRoutes;
		TArray<uint8>	Name;
		TArray<uint8>	GroupName;
		int64			PrevTimestamp = 0;
		uint32			ThreadId = ~0u;
		uint32			SystemId = 0;
		int32			SortHint = 0xff;
	};

						FThreads();
						~FThreads();
	FInfo*				GetInfo();
	FInfo*				GetInfo(uint32 ThreadId);
	void				SetGroupName(const ANSICHAR* InGroupName, uint32 Length);
	const TArray<uint8>*GetGroupName() const;

private:
	uint32				LastGetInfoId = ~0u;
	TArray<FInfo*>		Infos;
	TArray<uint8>		GroupName;
};

////////////////////////////////////////////////////////////////////////////////
FThreads::FThreads()
{
	Infos.Reserve(64);
}

////////////////////////////////////////////////////////////////////////////////
FThreads::~FThreads()
{
	for (FInfo* Info : Infos)
	{
		delete Info;
	}
}

////////////////////////////////////////////////////////////////////////////////
FThreads::FInfo* FThreads::GetInfo()
{
	return GetInfo(LastGetInfoId);
}

////////////////////////////////////////////////////////////////////////////////
FThreads::FInfo* FThreads::GetInfo(uint32 ThreadId)
{
	LastGetInfoId = ThreadId;

	if (ThreadId >= uint32(Infos.Num()))
	{
		Infos.SetNumZeroed(ThreadId + 1);
	}

	FInfo* Info = Infos[ThreadId];
	if (Info == nullptr)
	{
		Info = new FInfo();
		Info->ThreadId = ThreadId;
		Infos[ThreadId] = Info;
	}
	return Info;
}

////////////////////////////////////////////////////////////////////////////////
void FThreads::SetGroupName(const ANSICHAR* InGroupName, uint32 Length)
{
	if (InGroupName == nullptr || *InGroupName == '\0')
	{
		GroupName.SetNum(0);
		return;
	}

	GroupName.SetNumUninitialized(Length + 1);
	GroupName[Length] = '\0';
	FMemory::Memcpy(GroupName.GetData(), InGroupName, Length);
}

////////////////////////////////////////////////////////////////////////////////
const TArray<uint8>* FThreads::GetGroupName() const
{
	return (GroupName.Num() > 0) ? &GroupName : nullptr;
}




// {{{1 thread-info ------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
uint32 IAnalyzer::FThreadInfo::GetId() const
{
	const auto* Info = (const FThreads::FInfo*)this;
	return Info->ThreadId;
}

////////////////////////////////////////////////////////////////////////////////
uint32 IAnalyzer::FThreadInfo::GetSystemId() const
{
	const auto* Info = (const FThreads::FInfo*)this;
	return Info->SystemId;
}

////////////////////////////////////////////////////////////////////////////////
int32 IAnalyzer::FThreadInfo::GetSortHint() const
{
	const auto* Info = (const FThreads::FInfo*)this;
	return Info->SortHint;
}

////////////////////////////////////////////////////////////////////////////////
const ANSICHAR* IAnalyzer::FThreadInfo::GetName() const
{
	const auto* Info = (const FThreads::FInfo*)this;
	if (Info->Name.Num() <= 0)
	{
		return nullptr;
	}

	return (const ANSICHAR*)(Info->Name.GetData());
}

////////////////////////////////////////////////////////////////////////////////
const ANSICHAR* IAnalyzer::FThreadInfo::GetGroupName() const
{
	const auto* Info = (const FThreads::FInfo*)this;
	if (Info->GroupName.Num() <= 0)
	{
		return "";
	}

	return (const ANSICHAR*)(Info->GroupName.GetData());
}



// {{{1 timing -----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
struct FTiming
{
	uint64	BaseTimestamp = 0;
	uint64	TimestampHz = 0;
	double	InvTimestampHz = 0.0;
	uint64	EventTimestamp = 0;
};



////////////////////////////////////////////////////////////////////////////////
uint64 IAnalyzer::FEventTime::GetTimestamp() const
{
	const auto* Timing = (const FTiming*)this;
	return Timing->EventTimestamp;
}

////////////////////////////////////////////////////////////////////////////////
double IAnalyzer::FEventTime::AsSeconds() const
{
	const auto* Timing = (const FTiming*)this;
	return double(Timing->EventTimestamp) * Timing->InvTimestampHz;
}

////////////////////////////////////////////////////////////////////////////////
uint64 IAnalyzer::FEventTime::AsCycle64() const
{
	const auto* Timing = (const FTiming*)this;
	return Timing->BaseTimestamp + Timing->EventTimestamp;
}

////////////////////////////////////////////////////////////////////////////////
double IAnalyzer::FEventTime::AsSeconds(uint64 Cycles64) const
{
	const auto* Timing = (const FTiming*)this;
	return double(int64(Cycles64) - int64(Timing->BaseTimestamp)) * Timing->InvTimestampHz;
}

////////////////////////////////////////////////////////////////////////////////
double IAnalyzer::FEventTime::AsSecondsAbsolute(int64 DurationCycles64) const
{
	const auto* Timing = (const FTiming*)this;
	return double(DurationCycles64) * Timing->InvTimestampHz;
}



// {{{1 dispatch ---------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
struct FDispatch
{
	enum
	{
		Flag_Important		= 1 << 0,
		Flag_MaybeHasAux	= 1 << 1,
		Flag_NoSync			= 1 << 2,
		Flag_Definition		= 1 << 3,
	};

	struct FField
	{
		uint32		Hash;
		uint16		Offset;
		uint16		Size;
		uint16		NameOffset;			// From FField ptr
		int8		SizeAndType;		// value == byte_size, sign == float < 0 < int
		uint8		Class : 7;
		uint8		bArray : 1;
		uint32		RefUid;			// If reference field, uuid of ref type
	};

	int32			GetFieldIndex(const ANSICHAR* Name) const;
	uint32			Hash				= 0;
	uint16			Uid					= 0;
	uint8			FieldCount			= 0;
	uint8			Flags				= 0;
	uint16			EventSize			= 0;
	uint16			LoggerNameOffset	= 0;	// From FDispatch ptr
	uint16			EventNameOffset		= 0;	// From FDispatch ptr
	FField			Fields[];
};

////////////////////////////////////////////////////////////////////////////////
int32 FDispatch::GetFieldIndex(const ANSICHAR* Name) const
{
	FFnv1aHash NameHash;
	NameHash.Add(Name);

	for (int32 i = 0, n = FieldCount; i < n; ++i)
	{
		if (Fields[i].Hash == NameHash.Get())
		{
			return i;
		}
	}

	return -1;
}



// {{{1 dispatch-builder -------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FDispatchBuilder
{
public:
						FDispatchBuilder();
	void				SetUid(uint16 Uid);
	void				SetLoggerName(const ANSICHAR* Name, int32 NameSize=-1);
	void				SetEventName(const ANSICHAR* Name, int32 NameSize=-1);
	void				SetImportant();
	void				SetMaybeHasAux();
	void				SetNoSync();
	void				SetDefinition();
	FDispatch::FField&	AddField(const ANSICHAR* Name, int32 NameSize, uint16 Size);
	FDispatch*			Finalize();

private:
	uint32				AppendName(const ANSICHAR* Name, int32 NameSize);
	TArray<uint8>		Buffer;
	TArray<uint8>		NameBuffer;
};

////////////////////////////////////////////////////////////////////////////////
FDispatchBuilder::FDispatchBuilder()
{
	Buffer.SetNum(sizeof(FDispatch));

	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	new (Dispatch) FDispatch();
}

////////////////////////////////////////////////////////////////////////////////
FDispatch* FDispatchBuilder::Finalize()
{
	int32 Size = Buffer.Num() + NameBuffer.Num();
	auto* Dispatch = (FDispatch*)FMemory::Malloc(Size);
	memcpy(Dispatch, Buffer.GetData(), Buffer.Num());
	memcpy(Dispatch->Fields + Dispatch->FieldCount, NameBuffer.GetData(), NameBuffer.Num());

	// Fix up name offsets
	for (int i = 0, n = Dispatch->FieldCount; i < n; ++i)
	{
		auto* Field = Dispatch->Fields + i;
		Field->NameOffset += (uint16)(Buffer.Num() - uint32(UPTRINT(Field) - UPTRINT(Dispatch)));
	}

	// Calculate this dispatch's hash.
	if (Dispatch->LoggerNameOffset || Dispatch->EventNameOffset)
	{
		Dispatch->LoggerNameOffset += (uint16)Buffer.Num();
		Dispatch->EventNameOffset += (uint16)Buffer.Num();

		FFnv1aHash Hash;
		Hash.Add((const ANSICHAR*)Dispatch + Dispatch->LoggerNameOffset);
		Hash.Add((const ANSICHAR*)Dispatch + Dispatch->EventNameOffset);
		Dispatch->Hash = Hash.Get();
	}

	return Dispatch;
}

////////////////////////////////////////////////////////////////////////////////
void FDispatchBuilder::SetUid(uint16 Uid)
{
	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	Dispatch->Uid = Uid;
}

////////////////////////////////////////////////////////////////////////////////
void FDispatchBuilder::SetLoggerName(const ANSICHAR* Name, int32 NameSize)
{
	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	Dispatch->LoggerNameOffset += (uint16)AppendName(Name, NameSize);
}

////////////////////////////////////////////////////////////////////////////////
void FDispatchBuilder::SetEventName(const ANSICHAR* Name, int32 NameSize)
{
	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	Dispatch->EventNameOffset = (uint16)AppendName(Name, NameSize);
}

////////////////////////////////////////////////////////////////////////////////
void FDispatchBuilder::SetImportant()
{
	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	Dispatch->Flags |= FDispatch::Flag_Important;
}

////////////////////////////////////////////////////////////////////////////////
void FDispatchBuilder::SetMaybeHasAux()
{
	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	Dispatch->Flags |= FDispatch::Flag_MaybeHasAux;
}

////////////////////////////////////////////////////////////////////////////////
void FDispatchBuilder::SetNoSync()
{
	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	Dispatch->Flags |= FDispatch::Flag_NoSync;
}

////////////////////////////////////////////////////////////////////////////////
void FDispatchBuilder::SetDefinition()
{
	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	Dispatch->Flags |= FDispatch::Flag_Definition;
}

////////////////////////////////////////////////////////////////////////////////
FDispatch::FField& FDispatchBuilder::AddField(const ANSICHAR* Name, int32 NameSize, uint16 Size)
{
	int32 Bufoff = Buffer.AddUninitialized(sizeof(FDispatch::FField));
	auto* Field = (FDispatch::FField*)(Buffer.GetData() + Bufoff);
	Field->NameOffset = (uint16)AppendName(Name, NameSize);
	Field->Size = Size;
	Field->RefUid = 0;

	FFnv1aHash Hash;
	Hash.Add((const uint8*)Name, NameSize);
	Field->Hash = Hash.Get();
	
	auto* Dispatch = (FDispatch*)(Buffer.GetData());
	Dispatch->FieldCount++;
	Dispatch->EventSize += Field->Size;

	return *Field;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FDispatchBuilder::AppendName(const ANSICHAR* Name, int32 NameSize)
{
	if (NameSize < 0)
	{
		NameSize = int32(FCStringAnsi::Strlen(Name));
	}

	int32 Ret = NameBuffer.AddUninitialized(NameSize + 1);
	uint8* Out = NameBuffer.GetData() + Ret;
	memcpy(Out, Name, NameSize);
	Out[NameSize] = '\0';
	return Ret;
}



// {{{1 event-type-info --------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
uint32 IAnalyzer::FEventTypeInfo::GetId() const
{
	const auto* Inner = (const FDispatch*)this;
	return Inner->Uid;
}

////////////////////////////////////////////////////////////////////////////////
const ANSICHAR* IAnalyzer::FEventTypeInfo::GetName() const
{
	const auto* Inner = (const FDispatch*)this;
	return (const ANSICHAR*)Inner + Inner->EventNameOffset;
}

////////////////////////////////////////////////////////////////////////////////
const ANSICHAR* IAnalyzer::FEventTypeInfo::GetLoggerName() const
{
	const auto* Inner = (const FDispatch*)this;
	return (const ANSICHAR*)Inner + Inner->LoggerNameOffset;
}

////////////////////////////////////////////////////////////////////////////////
uint32 IAnalyzer::FEventTypeInfo::GetSize() const
{
	const auto* Inner = (const FDispatch*)this;
	return Inner->EventSize;
}

////////////////////////////////////////////////////////////////////////////////
uint32 IAnalyzer::FEventTypeInfo::GetFieldCount() const
{
	const auto* Inner = (const FDispatch*)this;
	return Inner->FieldCount;
}

////////////////////////////////////////////////////////////////////////////////
const IAnalyzer::FEventFieldInfo* IAnalyzer::FEventTypeInfo::GetFieldInfo(uint32 Index) const
{
	if (Index >= GetFieldCount())
	{
		return nullptr;
	}

	const auto* Inner = (const FDispatch*)this;
	return (const IAnalyzer::FEventFieldInfo*)(Inner->Fields + Index);
}

////////////////////////////////////////////////////////////////////////////////
IAnalyzer::FEventFieldHandle IAnalyzer::FEventTypeInfo::GetFieldHandleUnchecked(uint32 Index) const
{
	const auto* Inner = (const FDispatch*)this;
	if (Index >= Inner->FieldCount)
	{
		return FEventFieldHandle { -1 };
	}
	return FEventFieldHandle {Inner->Fields[Index].Offset };
}

////////////////////////////////////////////////////////////////////////////////
IAnalyzer::FEventFieldHandle IAnalyzer::FEventTypeInfo::GetFieldHandleImpl(
	const ANSICHAR* FieldName,
	int16& SizeAndType) const
{
	const auto* Inner = (const FDispatch*)this;
	int32 Index = Inner->GetFieldIndex(FieldName);
	if (Index < 0)
	{
		return { -1 };
	}

	const FDispatch::FField& Field = Inner->Fields[Index];
	SizeAndType = Field.SizeAndType;
	return { Field.Offset };
}



// {{{1 field-info -------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
const ANSICHAR* IAnalyzer::FEventFieldInfo::GetName() const
{
	const auto* Inner = (const FDispatch::FField*)this;
	return (const ANSICHAR*)(UPTRINT(Inner) + Inner->NameOffset);
}

////////////////////////////////////////////////////////////////////////////////
IAnalyzer::FEventFieldInfo::EType IAnalyzer::FEventFieldInfo::GetType() const
{
	const auto* Inner = (const FDispatch::FField*)this;

	if (Inner->RefUid != 0)
	{
		switch(Inner->Size)
		{
			case sizeof(uint8): return EType::Reference8;
			case sizeof(uint16): return EType::Reference16;
			case sizeof(uint32): return EType::Reference32;
			case sizeof(uint64): return EType::Reference64;
			default: check(false); // Unsupported width
		}
	}
	
	if (Inner->Class == UE::Trace::Protocol0::Field_String)
	{
		return (Inner->SizeAndType == 1) ? EType::AnsiString : EType::WideString;
	}

	if (Inner->SizeAndType > 0)
	{
		return EType::Integer;
	}

	if (Inner->SizeAndType < 0)
	{
		return EType::Float;
	}

	return EType::None;
}

////////////////////////////////////////////////////////////////////////////////
bool IAnalyzer::FEventFieldInfo::IsArray() const
{
	const auto* Inner = (const FDispatch::FField*)this;
	return Inner->bArray;
}

////////////////////////////////////////////////////////////////////////////////
bool IAnalyzer::FEventFieldInfo::IsSigned() const
{
	const auto* Inner = (const FDispatch::FField*)this;
	return (Inner->Class & UE::Trace::Protocol0::Field_Signed) != 0;
}

////////////////////////////////////////////////////////////////////////////////
uint8 IAnalyzer::FEventFieldInfo::GetSize() const
{
	const auto* Inner = (const FDispatch::FField*)this;
	return uint8(Inner->Size);
}

// {{{1 array-reader -----------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
uint32 IAnalyzer::FArrayReader::Num() const
{
	const auto* Inner = (const FAuxData*)this;
	int32 SizeAndType = Inner->FieldSizeAndType;
	SizeAndType = (SizeAndType < 0) ? -SizeAndType : SizeAndType;
	return (SizeAndType == 0) ? SizeAndType : (Inner->DataSize / SizeAndType);
}

////////////////////////////////////////////////////////////////////////////////
const void* IAnalyzer::FArrayReader::GetImpl(uint32 Index, int8& SizeAndType) const
{
	const auto* Inner = (const FAuxData*)this;
	SizeAndType = Inner->FieldSizeAndType;
	uint32 Count = Num();
	if (Index >= Count)
	{
		return nullptr;
	}

	SizeAndType = (SizeAndType < 0) ? -SizeAndType : SizeAndType;
	return Inner->Data + (Index * SizeAndType);
}



// {{{1 event-data -------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
struct FEventDataInfo
{
	const FAuxData*		GetAuxData(uint32 FieldIndex) const;
	const uint8*		Ptr;
	const FDispatch&	Dispatch;
	FAuxDataCollector*	AuxCollector;
	uint16				Size;
};

////////////////////////////////////////////////////////////////////////////////
const FAuxData* FEventDataInfo::GetAuxData(uint32 FieldIndex) const
{
	const auto* Info = (const FEventDataInfo*)this;
	if (Info->AuxCollector == nullptr)
	{
		return nullptr;
	}

	for (FAuxData& Data : *(Info->AuxCollector))
	{
		if (Data.FieldIndex == FieldIndex)
		{
			if (Data.bFragmented)
			{
				Info->AuxCollector->Defragment(Data);
			}

			const FDispatch::FField& Field = Info->Dispatch.Fields[FieldIndex];
			Data.FieldSizeAndType = Field.SizeAndType;
			Data.bSigned = (Field.Class & UE::Trace::Protocol0::Field_Signed) != 0;
			return &Data;
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
const IAnalyzer::FEventTypeInfo& IAnalyzer::FEventData::GetTypeInfo() const
{
	const auto* Info = (const FEventDataInfo*)this;
	return (const FEventTypeInfo&)(Info->Dispatch);
}

////////////////////////////////////////////////////////////////////////////////
const IAnalyzer::FArrayReader* IAnalyzer::FEventData::GetArrayImpl(const ANSICHAR* FieldName) const
{
	const auto* Info = (const FEventDataInfo*)this;

	int32 Index = Info->Dispatch.GetFieldIndex(FieldName);
	if (Index >= 0)
	{
		if (const FAuxData* Data = Info->GetAuxData(Index))
		{
			return (IAnalyzer::FArrayReader*)Data;
		}
	}

	static const FAuxData EmptyAuxData = {};
	return (const IAnalyzer::FArrayReader*)&EmptyAuxData;
}

////////////////////////////////////////////////////////////////////////////////
const void* IAnalyzer::FEventData::GetValueImpl(const ANSICHAR* FieldName, int8& SizeAndType) const
{
	const auto* Info = (const FEventDataInfo*)this;
	const auto& Dispatch = Info->Dispatch;

	int32 Index = Dispatch.GetFieldIndex(FieldName);
	if (Index < 0)
	{
		return nullptr;
	}

	const auto& Field = Dispatch.Fields[Index];
	SizeAndType = Field.SizeAndType;
	return (Info->Ptr + Field.Offset);
}

////////////////////////////////////////////////////////////////////////////////
bool IAnalyzer::FEventData::GetString(const ANSICHAR* FieldName, FAnsiStringView& Out) const
{
	const auto* Info = (const FEventDataInfo*)this;
	const auto& Dispatch = Info->Dispatch;

	int32 Index = Dispatch.GetFieldIndex(FieldName);
	if (Index < 0)
	{
		return false;
	}

	const auto& Field = Dispatch.Fields[Index];
	if (Field.Class != UE::Trace::Protocol0::Field_String || Field.SizeAndType != sizeof(ANSICHAR))
	{
		return false;
	}

	if (const FAuxData* Data = Info->GetAuxData(Index))
	{
		Out = FAnsiStringView((const ANSICHAR*)(Data->Data), Data->DataSize);
		return true;
	}

	Out = FAnsiStringView();
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool IAnalyzer::FEventData::GetString(const ANSICHAR* FieldName, FWideStringView& Out) const
{
	const auto* Info = (const FEventDataInfo*)this;
	const auto& Dispatch = Info->Dispatch;

	int32 Index = Dispatch.GetFieldIndex(FieldName);
	if (Index < 0)
	{
		return false;
	}

	const auto& Field = Dispatch.Fields[Index];
	if (Field.Class != UE::Trace::Protocol0::Field_String || Field.SizeAndType != sizeof(WIDECHAR))
	{
		return false;
	}

	if (const FAuxData* Data = Info->GetAuxData(Index))
	{
		Out = FWideStringView((const WIDECHAR*)(Data->Data), Data->DataSize / sizeof(WIDECHAR));
		return true;
	}

	Out = FWideStringView();
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool IAnalyzer::FEventData::GetString(const ANSICHAR* FieldName, FString& Out) const
{
	const auto* Info = (const FEventDataInfo*)this;
	const auto& Dispatch = Info->Dispatch;

	int32 Index = Dispatch.GetFieldIndex(FieldName);
	if (Index < 0)
	{
		return false;
	}

	const auto& Field = Dispatch.Fields[Index];
	if (Field.Class == UE::Trace::Protocol0::Field_String)
	{
		const FAuxData* Data = Info->GetAuxData(Index);
		if (Data == nullptr)
		{
			Out = FString();
			return true;
		}

		if (Field.SizeAndType == sizeof(ANSICHAR))
		{
			Out = FString(Data->DataSize, (const ANSICHAR*)(Data->Data));
			return true;
		}

		if (Field.SizeAndType == sizeof(WIDECHAR))
		{
			Out = FWideStringView((const WIDECHAR*)(Data->Data), Data->DataSize / sizeof(WIDECHAR));
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
uint32 IAnalyzer::FEventData::GetSize() const
{
	const auto* Info = (const FEventDataInfo*)this;
	uint32 Size = Info->Size;
	if (Info->AuxCollector)
	{
		for (const FAuxData& Data : *(Info->AuxCollector))
		{
			Size += Data.DataSize;
		}
	}
	return Size;
}

////////////////////////////////////////////////////////////////////////////////
void IAnalyzer::FEventData::SerializeToCbor(TArray<uint8>& Out) const
{
	const auto* Info = (const FEventDataInfo*)this;
	uint32 Size = Info->Size;
	if (Info->AuxCollector != nullptr)
	{
		for (FAuxData& Data : *(Info->AuxCollector))
		{
			Size += Data.DataSize;
		}
	}
	SerializeToCborImpl(Out, *this, Size);
}

////////////////////////////////////////////////////////////////////////////////
const void* IAnalyzer::FEventData::GetValueRaw(FEventFieldHandle Handle) const
{
	const auto* Info = (const FEventDataInfo*)this;
	if (!Handle.IsValid())
	{
		return nullptr;
	}
	return Info->Ptr + Handle.Detail;
}

////////////////////////////////////////////////////////////////////////////////
const uint8* IAnalyzer::FEventData::GetAttachment() const
{
	const auto* Info = (const FEventDataInfo*)this;
	return Info->Ptr + Info->Dispatch.EventSize;
}

////////////////////////////////////////////////////////////////////////////////
uint32 IAnalyzer::FEventData::GetAttachmentSize() const
{
	const auto* Info = (const FEventDataInfo*)this;
	return Info->Size - Info->Dispatch.EventSize;
}

////////////////////////////////////////////////////////////////////////////////
bool IAnalyzer::FEventData::IsDefinitionImpl(uint32& OutTypeId) const
{
	const auto* Info = (const FEventDataInfo*)this;
	OutTypeId = Info->Dispatch.Uid;
	return (Info->Dispatch.Flags & FDispatch::Flag_Definition) != 0;
}

////////////////////////////////////////////////////////////////////////////////
const void* IAnalyzer::FEventData::GetReferenceValueImpl(const char* FieldName, uint16& OutSizeType, uint32& OutTypeUid) const
{
	const auto* Info = (const FEventDataInfo*)this;
	const auto& Dispatch = Info->Dispatch;

	const int32 Index = Dispatch.GetFieldIndex(FieldName);
	if (Index < 0)
	{
		return nullptr;
	}

	const auto& Field = Dispatch.Fields[Index];
	if (Field.RefUid)
	{
		OutSizeType = Field.SizeAndType;
		OutTypeUid = Field.RefUid;
		return (Info->Ptr + Field.Offset);
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
const void* IAnalyzer::FEventData::GetReferenceValueImpl(uint32 FieldIndex, uint32& OutTypeUid) const
{
	const auto* Info = (const FEventDataInfo*)this;
	const auto& Dispatch = Info->Dispatch;
	const auto& Field = Dispatch.Fields[FieldIndex];
	if (Field.RefUid)
	{
		OutTypeUid = Field.RefUid;
		return (Info->Ptr + Field.Offset);
	}
	return nullptr;
}

// }}}



// {{{1 type-registry ----------------------------------------------------------
class FTypeRegistry
{
public:
	typedef FDispatch FTypeInfo;

						~FTypeRegistry();
	const FTypeInfo*	Add(const void* TraceData, uint32 Version);
	const FTypeInfo*	AddVersion4(const void* TraceData);
	const FTypeInfo*	AddVersion6(const void* TraceData);
	void				Add(FTypeInfo* TypeInfo);
	const FTypeInfo*	Get(uint32 Uid) const;
	bool				IsUidValid(uint32 Uid) const;

private:
	TArray<FTypeInfo*>	TypeInfos;
};

////////////////////////////////////////////////////////////////////////////////
FTypeRegistry::~FTypeRegistry()
{
	for (FTypeInfo* TypeInfo : TypeInfos)
	{
		FMemory::Free(TypeInfo);
	}
}

////////////////////////////////////////////////////////////////////////////////
const FTypeRegistry::FTypeInfo* FTypeRegistry::Add(const void* TraceData, uint32 Version)
{
	switch(Version)
	{
	case 0: return AddVersion4(TraceData);
	case 4: return AddVersion4(TraceData);
	case 6: return AddVersion6(TraceData);
	default:
		check(false); // Unknown version
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
const FTypeRegistry::FTypeInfo* FTypeRegistry::Get(uint32 Uid) const
{
	if (Uid >= uint32(TypeInfos.Num()))
	{
		return nullptr;
	}

	return TypeInfos[Uid];
}

////////////////////////////////////////////////////////////////////////////////
const FTypeRegistry::FTypeInfo* FTypeRegistry::AddVersion4(const void* TraceData)
{
	FDispatchBuilder Builder;

	const auto& NewEvent = *(Protocol4::FNewEventEvent*)(TraceData);

	const auto* NameCursor = (const ANSICHAR*)(NewEvent.Fields + NewEvent.FieldCount);

	Builder.SetLoggerName(NameCursor, NewEvent.LoggerNameSize);
	NameCursor += NewEvent.LoggerNameSize;

	Builder.SetEventName(NameCursor, NewEvent.EventNameSize);
	NameCursor += NewEvent.EventNameSize;
	Builder.SetUid(NewEvent.EventUid);

	// Fill out the fields
	for (int32 i = 0, n = NewEvent.FieldCount; i < n; ++i)
	{
		const auto& Field = NewEvent.Fields[i];

		int8 TypeSize = 1 << (Field.TypeInfo & Protocol0::Field_Pow2SizeMask);
		if (Field.TypeInfo & Protocol0::Field_Float)
		{
			TypeSize = -TypeSize;
		}

		auto& OutField = Builder.AddField(NameCursor, Field.NameSize, Field.Size);
		OutField.Offset = Field.Offset;
		OutField.SizeAndType = TypeSize;

		OutField.Class = Field.TypeInfo & Protocol0::Field_SpecialMask;
		OutField.bArray = !!(Field.TypeInfo & Protocol0::Field_Array);

		NameCursor += Field.NameSize;
	}

	if (NewEvent.Flags & uint32(Protocol4::EEventFlags::Important))
	{
		Builder.SetImportant();
	}

	if (NewEvent.Flags & uint32(Protocol4::EEventFlags::MaybeHasAux))
	{
		Builder.SetMaybeHasAux();
	}

	if (NewEvent.Flags & uint32(Protocol4::EEventFlags::NoSync))
	{
		Builder.SetNoSync();
	}

	FTypeInfo* TypeInfo = Builder.Finalize();
	Add(TypeInfo);

	return TypeInfo;
}

////////////////////////////////////////////////////////////////////////////////
const FTypeRegistry::FTypeInfo* FTypeRegistry::AddVersion6(const void* TraceData)
{
	using namespace Protocol6;
	FDispatchBuilder Builder;

	const auto& NewEvent = *(FNewEventEvent*)(TraceData);

	const auto* NameCursor = (const ANSICHAR*)(NewEvent.Fields + NewEvent.FieldCount);

	Builder.SetLoggerName(NameCursor, NewEvent.LoggerNameSize);
	NameCursor += NewEvent.LoggerNameSize;

	Builder.SetEventName(NameCursor, NewEvent.EventNameSize);
	NameCursor += NewEvent.EventNameSize;
	Builder.SetUid(NewEvent.EventUid);

	// Fill out the fields
	for (int32 i = 0, n = NewEvent.FieldCount; i < n; ++i)
	{
		const auto& Field = NewEvent.Fields[i];
		if (Field.FieldType == EFieldFamily::Regular)
		{
			int8 TypeSize = 1 << (Field.Regular.TypeInfo & Protocol0::Field_Pow2SizeMask);
			if (Field.Regular.TypeInfo & Protocol0::Field_Float)
			{
				TypeSize = -TypeSize;
			}

			auto& OutField = Builder.AddField(NameCursor, Field.Regular.NameSize, Field.Regular.Size);
			OutField.Offset = Field.Regular.Offset;
			OutField.SizeAndType = TypeSize;

			OutField.Class = Field.Regular.TypeInfo & Protocol0::Field_SpecialMask;
			OutField.bArray = !!(Field.Regular.TypeInfo & Protocol0::Field_Array);

			NameCursor += Field.Regular.NameSize;
		}
		else if (Field.FieldType == EFieldFamily::Reference)
		{
			check((Field.Reference.TypeInfo & Protocol0::Field_CategoryMask) == Protocol0::Field_Integer);
			const int8 TypeSize = 1 << (Field.Reference.TypeInfo & Protocol0::Field_Pow2SizeMask);

			auto& OutField = Builder.AddField(NameCursor, Field.Reference.NameSize, TypeSize);
			OutField.Offset = Field.Reference.Offset;
			OutField.SizeAndType = TypeSize;
			OutField.RefUid = uint32(Field.Reference.RefUid);
			OutField.Class = 0;
			OutField.bArray = false;

			NameCursor += Field.Reference.NameSize;
		}
		else if (Field.FieldType == EFieldFamily::DefinitionId)
		{
			check((Field.DefinitionId.TypeInfo & Protocol0::Field_CategoryMask) == Protocol0::Field_Integer);
			const int8 TypeSize = 1 << (Field.DefinitionId.TypeInfo & Protocol0::Field_Pow2SizeMask);
			
			auto DefinitionIdFieldName = ANSITEXTVIEW("DefinitionId");
			auto& OutField = Builder.AddField(DefinitionIdFieldName.GetData(), DefinitionIdFieldName.Len(), TypeSize);
			OutField.Offset = Field.DefinitionId.Offset;
			OutField.SizeAndType = TypeSize;
			OutField.RefUid = NewEvent.EventUid;
			OutField.Class = 0;
			OutField.bArray = false;
		}
		else
		{
			// Error!
			continue;
		}
	}

	if (NewEvent.Flags & uint32(EEventFlags::Important))
	{
		Builder.SetImportant();
	}

	if (NewEvent.Flags & uint32(EEventFlags::MaybeHasAux))
	{
		Builder.SetMaybeHasAux();
	}

	if (NewEvent.Flags & uint32(EEventFlags::NoSync))
	{
		Builder.SetNoSync();
	}

	if (NewEvent.Flags & uint32(EEventFlags::Definition))
	{
		Builder.SetDefinition();
	}

	FTypeInfo* TypeInfo = Builder.Finalize();
	Add(TypeInfo);

	return TypeInfo;
}

////////////////////////////////////////////////////////////////////////////////
void FTypeRegistry::Add(FTypeInfo* TypeInfo)
{
	// Add the type to the type-infos table. Usually duplicates are an error
	// but due to backwards compatibility we'll override existing types.
	uint16 Uid = TypeInfo->Uid;
	if (Uid < uint32(TypeInfos.Num()))
 	{
		if (TypeInfos[Uid] != nullptr)
 		{
			FMemory::Free(TypeInfos[Uid]);
			TypeInfos[Uid] = nullptr;
 		}
 	}
	else
 	{
		TypeInfos.SetNum(Uid + 1);
 	}

	TypeInfos[Uid] = TypeInfo;
}

////////////////////////////////////////////////////////////////////////////////
bool FTypeRegistry::IsUidValid(uint32 Uid) const
{
	return (Uid < uint32(TypeInfos.Num())) && (TypeInfos[Uid] != nullptr);
}

// {{{1 analyzer-hub -----------------------------------------------------------
class FAnalyzerHub
{
public:
	void				End();
	void				SetAnalyzers(TArray<IAnalyzer*>&& InAnalyzers);
	uint32				GetActiveAnalyzerNum() const;
	void				OnNewType(const FTypeRegistry::FTypeInfo* TypeInfo);
	void				OnEvent(const FTypeRegistry::FTypeInfo& TypeInfo, IAnalyzer::EStyle Style, const IAnalyzer::FOnEventContext& Context);
	void				OnThreadInfo(const FThreads::FInfo& ThreadInfo);

private:
	void				BuildRoutes();
	void				AddRoute(uint16 AnalyzerIndex, uint16 Id, const ANSICHAR* Logger, const ANSICHAR* Event, bool bScoped);
	int32				GetRouteIndex(const FTypeRegistry::FTypeInfo& TypeInfo);
	void				RetireAnalyzer(IAnalyzer* Analyzer);
	template <typename ImplType>
	void				ForEachRoute(uint32 RouteIndex, bool bScoped, ImplType&& Impl) const;

	struct FRoute
	{
		uint32			Hash;
		uint16			AnalyzerIndex : 15;
		uint16			bScoped : 1;
		uint16			Id;
		union
		{
			uint32		ParentHash;
			struct
			{
				int16	Count;
				int16	Parent;
			};
		};
	};

	typedef TArray<uint16, TInlineAllocator<96>> TypeToRouteArray;

	TArray<IAnalyzer*>	Analyzers;
	TArray<FRoute>		Routes;
	TypeToRouteArray	TypeToRoute; // biases by one so zero represents no route
};

////////////////////////////////////////////////////////////////////////////////
void FAnalyzerHub::End()
{
	for (IAnalyzer* Analyzer : Analyzers)
	{
		if (Analyzer != nullptr)
		{
			Analyzer->OnAnalysisEnd();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAnalyzerHub::SetAnalyzers(TArray<IAnalyzer*>&& InAnalyzers)
{
	Analyzers = MoveTemp(InAnalyzers);
	BuildRoutes();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAnalyzerHub::GetActiveAnalyzerNum() const
{
	uint32 Count = 0;
	for (IAnalyzer* Analyzer : Analyzers)
	{
		Count += (Analyzer != nullptr);
	}
	return Count;
}

////////////////////////////////////////////////////////////////////////////////
int32 FAnalyzerHub::GetRouteIndex(const FTypeRegistry::FTypeInfo& TypeInfo)
{
	if (TypeInfo.Uid >= uint32(TypeToRoute.Num()))
	{
		return -1;
	}

	return int32(TypeToRoute[TypeInfo.Uid]) - 1;
}

////////////////////////////////////////////////////////////////////////////////
template <typename ImplType>
void FAnalyzerHub::ForEachRoute(uint32 RouteIndex, bool bScoped, ImplType&& Impl) const
{
	uint32 RouteCount = Routes.Num();
	if (RouteIndex >= RouteCount)
	{
		return;
	}

	const FRoute* FirstRoute = Routes.GetData();
	const FRoute* Route = FirstRoute + RouteIndex;
	do
	{
		const FRoute* NextRoute = FirstRoute + Route->Parent;
		for (uint32 n = Route->Count; n--; ++Route)
		{
			if (Route->bScoped != (bScoped == true))
			{
				continue;
			}

			IAnalyzer* Analyzer = Analyzers[Route->AnalyzerIndex];
			if (Analyzer != nullptr)
			{
				Impl(Analyzer, Route->Id);
			}
		}
		Route = NextRoute;
	}
	while (Route >= FirstRoute);
}

////////////////////////////////////////////////////////////////////////////////
void FAnalyzerHub::OnNewType(const FTypeRegistry::FTypeInfo* TypeInfo)
{
	// Find routes that have subscribed to this event.
	auto FindRoute = [this] (uint32 Hash)
	{
		int32 Index = Algo::LowerBoundBy(Routes, Hash, [] (const FRoute& Route) { return Route.Hash; });
		return (Index < Routes.Num() && Routes[Index].Hash == Hash) ? Index : -1;
	};

	int32 FirstRoute = FindRoute(TypeInfo->Hash);
	if (FirstRoute < 0)
	{
		FFnv1aHash LoggerHash;
		LoggerHash.Add((const ANSICHAR*)TypeInfo + TypeInfo->LoggerNameOffset);
		if ((FirstRoute = FindRoute(LoggerHash.Get())) < 0)
		{
			FirstRoute = FindRoute(FFnv1aHash().Get());
		}
	}

	uint16 Uid = TypeInfo->Uid;
	if (Uid >= uint16(TypeToRoute.Num()))
 	{
		TypeToRoute.SetNumZeroed(Uid + 32);
 	}

	TypeToRoute[Uid] = uint16(FirstRoute + 1);

	// Inform routes that a new event has been declared.
	if (FirstRoute >= 0)
	{
		ForEachRoute(FirstRoute, false, [&] (IAnalyzer* Analyzer, uint16 RouteId)
		{
			if (!Analyzer->OnNewEvent(RouteId, *(IAnalyzer::FEventTypeInfo*)TypeInfo))
			{
				RetireAnalyzer(Analyzer);
			}
		});
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAnalyzerHub::OnEvent(
	const FTypeRegistry::FTypeInfo& TypeInfo,
	const IAnalyzer::EStyle Style,
	const IAnalyzer::FOnEventContext& Context)
{
	int32 RouteIndex = GetRouteIndex(TypeInfo);
	if (RouteIndex < 0)
	{
		return;
	}

	bool bScoped = (Style != IAnalyzer::EStyle::Normal);
	ForEachRoute(RouteIndex, bScoped, [&] (IAnalyzer* Analyzer, uint16 RouteId)
	{
		if (!Analyzer->OnEvent(RouteId, Style, Context))
		{
			RetireAnalyzer(Analyzer);
		}
	});
}

////////////////////////////////////////////////////////////////////////////////
void FAnalyzerHub::OnThreadInfo(const FThreads::FInfo& ThreadInfo)
{
	const auto& OuterThreadInfo = (IAnalyzer::FThreadInfo&)ThreadInfo;
	for (IAnalyzer* Analyzer : Analyzers)
	{
		if (Analyzer != nullptr)
		{
			Analyzer->OnThreadInfo(OuterThreadInfo);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAnalyzerHub::BuildRoutes()
{
	// Call out to all registered analyzers to have them register event interest
	struct : public IAnalyzer::FInterfaceBuilder
	{
		virtual void RouteEvent(uint16 RouteId, const ANSICHAR* Logger, const ANSICHAR* Event, bool bScoped) override
		{
			Self->AddRoute(AnalyzerIndex, RouteId, Logger, Event, bScoped);
		}

		virtual void RouteLoggerEvents(uint16 RouteId, const ANSICHAR* Logger, bool bScoped) override
		{
			Self->AddRoute(AnalyzerIndex, RouteId, Logger, "", bScoped);
		}

		virtual void RouteAllEvents(uint16 RouteId, bool bScoped) override
		{
			Self->AddRoute(AnalyzerIndex, RouteId, "", "", bScoped);
		}

		FAnalyzerHub* Self;
		uint16 AnalyzerIndex;
	} Builder;
	Builder.Self = this;

	IAnalyzer::FOnAnalysisContext OnAnalysisContext = { Builder };
	for (uint16 i = 0, n = uint16(Analyzers.Num()); i < n; ++i)
	{
		uint32 RouteCount = Routes.Num();

		Builder.AnalyzerIndex = i;
		IAnalyzer* Analyzer = Analyzers[i];
		Analyzer->OnAnalysisBegin(OnAnalysisContext);

		// If the analyzer didn't add any routes we'll retire it immediately
		if (RouteCount == Routes.Num())
		{
			RetireAnalyzer(Analyzer);
		}
	}

	// Sort routes by their hashes.
	auto RouteProjection = [] (const FRoute& Route) { return Route.Hash; };
	Algo::SortBy(Routes, RouteProjection);

	auto FindRoute = [this, &RouteProjection] (uint32 Hash)
	{
		int32 Index = Algo::LowerBoundBy(Routes, Hash, RouteProjection);
		return (Index < Routes.Num() && Routes[Index].Hash == Hash) ? Index : -1;
	};

	int32 AllEventsIndex = FindRoute(FFnv1aHash().Get());
	auto FixupRoute = [this, &FindRoute, AllEventsIndex] (FRoute* Route) 
	{
		if (Route->ParentHash)
		{
			int32 ParentIndex = FindRoute(Route->ParentHash);
			Route->Parent = int16((ParentIndex < 0) ? AllEventsIndex : ParentIndex);
		}
		else
		{
			Route->Parent = -1;
		}
		Route->Count = 1;
		return Route;
	};

	FRoute* Cursor = FixupRoute(Routes.GetData());
	for (uint16 i = 1, n = uint16(Routes.Num()); i < n; ++i)
	{
		FRoute* Route = Routes.GetData() + i;
		if (Route->Hash == Cursor->Hash)
		{
			Cursor->Count++;
		}
		else
		{
			Cursor = FixupRoute(Route);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAnalyzerHub::RetireAnalyzer(IAnalyzer* Analyzer)
{
	for (uint32 i = 0, n = Analyzers.Num(); i < n; ++i)
	{
		if (Analyzers[i] != Analyzer)
		{
			continue;
		}

		Analyzer->OnAnalysisEnd();
		Analyzers[i] = nullptr;
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAnalyzerHub::AddRoute(
	uint16 AnalyzerIndex,
	uint16 Id,
	const ANSICHAR* Logger,
	const ANSICHAR* Event,
	bool bScoped)
{
	check(AnalyzerIndex < Analyzers.Num());

	uint32 ParentHash = 0;
	if (Logger[0] && Event[0])
	{
		FFnv1aHash Hash;
		Hash.Add(Logger);
		ParentHash = Hash.Get();
	}

	FFnv1aHash Hash;
	Hash.Add(Logger);
	Hash.Add(Event);

	FRoute& Route = Routes.Emplace_GetRef();
	Route.Id = Id;
	Route.Hash = Hash.Get();
	Route.ParentHash = ParentHash;
	Route.AnalyzerIndex = AnalyzerIndex;
	Route.bScoped = (bScoped == true);
}

// {{{1 state ------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
struct FAnalysisState
{
	struct FSerial
	{
		uint32	Value = 0;
		uint32	Mask = 0;
	};

	FThreads	Threads;
	FTiming		Timing;
	FSerial		Serial;
	uint32		UserUidBias = Protocol4::EKnownEventUids::User;
};



// {{{1 thread-info-cb ---------------------------------------------------------
class FThreadInfoCallback
{
public:
	virtual			~FThreadInfoCallback() {}
	virtual void	OnThreadInfo(const FThreads::FInfo& Info) = 0;
};



// {{{1 analyzer ---------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
enum
{
	RouteId_NewTrace,
	RouteId_SerialSync,
	RouteId_Timing,
	RouteId_ThreadTiming,
	RouteId_ThreadInfo,
	RouteId_ThreadGroupBegin,
	RouteId_ThreadGroupEnd,
};

////////////////////////////////////////////////////////////////////////////////
class FTraceAnalyzer
	: public IAnalyzer
{
public:
							FTraceAnalyzer(FAnalysisState& InState, FThreadInfoCallback& InCallback);
	virtual bool			OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;
	virtual void			OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	void					OnNewTrace(const FOnEventContext& Context);
	void					OnSerialSync(const FOnEventContext& Context);
	void					OnThreadTiming(const FOnEventContext& Context);
	void					OnThreadInfoInternal(const FOnEventContext& Context);
	void					OnThreadGroupBegin(const FOnEventContext& Context);
	void					OnThreadGroupEnd();
	void					OnTiming(const FOnEventContext& Context);

private:
	FAnalysisState&			State;
	FThreadInfoCallback&	ThreadInfoCallback;
};

////////////////////////////////////////////////////////////////////////////////
FTraceAnalyzer::FTraceAnalyzer(FAnalysisState& InState, FThreadInfoCallback& InCallback)
: State(InState)
, ThreadInfoCallback(InCallback)
{
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;
	Builder.RouteEvent(RouteId_NewTrace,		"$Trace", "NewTrace");
	Builder.RouteEvent(RouteId_SerialSync,		"$Trace", "SerialSync");
	Builder.RouteEvent(RouteId_Timing,			"$Trace", "Timing");
	Builder.RouteEvent(RouteId_ThreadTiming,	"$Trace", "ThreadTiming");
	Builder.RouteEvent(RouteId_ThreadInfo,		"$Trace", "ThreadInfo");
	Builder.RouteEvent(RouteId_ThreadGroupBegin,"$Trace", "ThreadGroupBegin");
	Builder.RouteEvent(RouteId_ThreadGroupEnd,	"$Trace", "ThreadGroupEnd");
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	switch (RouteId)
	{
	case RouteId_NewTrace:			OnNewTrace(Context);			break;
	case RouteId_SerialSync:		OnSerialSync(Context);			break;
	case RouteId_Timing:			OnTiming(Context);				break;
	case RouteId_ThreadTiming:		OnThreadTiming(Context);		break;
	case RouteId_ThreadInfo:		OnThreadInfoInternal(Context);	break;
	case RouteId_ThreadGroupBegin:	OnThreadGroupBegin(Context);	break;
	case RouteId_ThreadGroupEnd:	OnThreadGroupEnd();				break;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalyzer::OnNewTrace(const FOnEventContext& Context)
{
	const FEventData& EventData = Context.EventData;

	// "Serial" will tell us approximately where we've started in the log serial
	// range. We'll bias it by half so we won't accept any serialised events and
	// mark the MSB to indicate that the current serial should be corrected.
	auto& Serial = State.Serial;
	uint32 Hint = EventData.GetValue<uint32>("Serial");
	Hint -= (Serial.Mask + 1) >> 1;
	Hint &= Serial.Mask;
	Serial.Value = Hint;
	Serial.Value |= 0xc0000000;

	// Later traces will have an explicit "SerialSync" trace event to indicate
	// when there is enough data to establish the correct log serial
	if ((EventData.GetValue<uint8>("FeatureSet") & 1) == 0)
	{
		OnSerialSync(Context);
	}

	State.UserUidBias = EventData.GetValue<uint32>("UserUidBias", uint32(UE::Trace::Protocol3::EKnownEventUids::User));

	OnTiming(Context);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalyzer::OnSerialSync(const FOnEventContext& Context)
{
	State.Serial.Value &= ~0x40000000;
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalyzer::OnTiming(const FOnEventContext& Context)
{
	uint64 StartCycle = Context.EventData.GetValue<uint64>("StartCycle");
	uint64 CycleFrequency = Context.EventData.GetValue<uint64>("CycleFrequency");

	State.Timing.BaseTimestamp = StartCycle;
	State.Timing.TimestampHz = CycleFrequency;
	State.Timing.InvTimestampHz = 1.0 / double(CycleFrequency);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalyzer::OnThreadTiming(const FOnEventContext& Context)
{
	uint64 BaseTimestamp = Context.EventData.GetValue<uint64>("BaseTimestamp");
	if (FThreads::FInfo* Info = State.Threads.GetInfo())
	{
		Info->PrevTimestamp = BaseTimestamp;

		// We can springboard of this event as a way to know a thread has just
		// started (or at least is about to send its first event). Notify analyzers
		// so they're aware of threads that never get explicitly registered.
		ThreadInfoCallback.OnThreadInfo(*Info);
	}
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalyzer::OnThreadInfoInternal(const FOnEventContext& Context)
{
	const FEventData& EventData = Context.EventData;

	FThreads::FInfo* ThreadInfo;
	uint32 ThreadId = EventData.GetValue<uint32>("ThreadId", ~0u);
	if (ThreadId != ~0u)
	{
		// Post important-events; the thread-info event is not on the thread it
		// represents anymore. Fortunately the thread-id is traced now.
		ThreadInfo = State.Threads.GetInfo(ThreadId);
	}
	else
	{
		ThreadInfo = State.Threads.GetInfo();
	}

	if (ThreadInfo == nullptr)
	{
		return;
	}

	ThreadInfo->SystemId = EventData.GetValue<uint32>("SystemId");
	ThreadInfo->SortHint = EventData.GetValue<int32>("SortHint");
	
	FAnsiStringView Name;
	EventData.GetString("Name", Name);
	ThreadInfo->Name.SetNumUninitialized(Name.Len() + 1);
	ThreadInfo->Name[Name.Len()] = '\0';
	FMemory::Memcpy(ThreadInfo->Name.GetData(), Name.GetData(), Name.Len());

	if (ThreadInfo->GroupName.Num() <= 0)
	{
		if (const TArray<uint8>* GroupName = State.Threads.GetGroupName())
		{
			ThreadInfo->GroupName = *GroupName;
		}
	}

	ThreadInfoCallback.OnThreadInfo(*ThreadInfo);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalyzer::OnThreadGroupBegin(const FOnEventContext& Context)
{
	const FEventData& EventData = Context.EventData;

	FAnsiStringView Name;
	EventData.GetString("Name", Name);
	State.Threads.SetGroupName(Name.GetData(), Name.Len());
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalyzer::OnThreadGroupEnd()
{
	State.Threads.SetGroupName("", 0);
}

// {{{1 bridge -----------------------------------------------------------------
class FAnalysisBridge
	: public FThreadInfoCallback
{
public:
	typedef FAnalysisState::FSerial FSerial;

						FAnalysisBridge(TArray<IAnalyzer*>&& Analyzers);
	bool				IsStillAnalyzing() const;
	void				Reset();
	uint32				GetUserUidBias() const;
	FSerial&			GetSerial();
	void				SetActiveThread(uint32 ThreadId);
	void				OnNewType(const FTypeRegistry::FTypeInfo* TypeInfo);
	void				OnEvent(const FEventDataInfo& EventDataInfo);
	virtual void		OnThreadInfo(const FThreads::FInfo& InThreadInfo) override;
	void				EnterScope();
	void				EnterScope(uint64 Timestamp);
	void				LeaveScope();
	void				LeaveScope(uint64 Timestamp);

private:
	void				DispatchLeaveScope();
	FAnalysisState		State;
	FTraceAnalyzer		TraceAnalyzer = { State, *this };
	FAnalyzerHub		AnalyzerHub;
	FThreads::FInfo*	ThreadInfo = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
FAnalysisBridge::FAnalysisBridge(TArray<IAnalyzer*>&& Analyzers)
{
	TArray<IAnalyzer*> TempAnalyzers(MoveTemp(Analyzers));
	TempAnalyzers.Add(&TraceAnalyzer);
	AnalyzerHub.SetAnalyzers(MoveTemp(TempAnalyzers));
}

////////////////////////////////////////////////////////////////////////////////
bool FAnalysisBridge::IsStillAnalyzing() const
{
	// "> 1" because TraceAnalyzer is always present but shouldn't be considered
	return AnalyzerHub.GetActiveAnalyzerNum() > 1;
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::Reset()
{
	AnalyzerHub.End();

	State.~FAnalysisState();
	new (&State) FAnalysisState();
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::SetActiveThread(uint32 ThreadId)
{
	ThreadInfo = State.Threads.GetInfo(ThreadId);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAnalysisBridge::GetUserUidBias() const
{
	return State.UserUidBias;
}

////////////////////////////////////////////////////////////////////////////////
FAnalysisBridge::FSerial& FAnalysisBridge::GetSerial()
{
	return State.Serial;
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::OnNewType(const FTypeRegistry::FTypeInfo* TypeInfo)
{
	AnalyzerHub.OnNewType(TypeInfo);
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::OnEvent(const FEventDataInfo& EventDataInfo)
{
	// TODO "Dispatch" should be renamed "EventTypeInfo" or similar.
	const FTypeRegistry::FTypeInfo* TypeInfo = &(EventDataInfo.Dispatch);

	IAnalyzer::EStyle Style = IAnalyzer::EStyle::Normal;
	if (ThreadInfo->ScopeRoutes.Num() > 0 && int64(ThreadInfo->ScopeRoutes.Last()) < 0)
	{
		Style = IAnalyzer::EStyle::EnterScope;
		State.Timing.EventTimestamp = ~(ThreadInfo->ScopeRoutes.Last());
		ThreadInfo->ScopeRoutes.Last() = PTRINT(TypeInfo);
	}
	else
	{
		State.Timing.EventTimestamp = 0;
	}

	IAnalyzer::FOnEventContext Context = {
		*(const IAnalyzer::FThreadInfo*)ThreadInfo,
		(const IAnalyzer::FEventTime&)(State.Timing),
		(const IAnalyzer::FEventData&)EventDataInfo,
	};
	AnalyzerHub.OnEvent(*TypeInfo, Style, Context);
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::OnThreadInfo(const FThreads::FInfo& InThreadInfo)
{
	// Note that InThreadInfo might not equal the bridge's ThreadInfo because
	// information about threads comes from trace and could have been traced on
	// a different thread to the one it is describing (or no thread at all in
	// the case of important events).
	AnalyzerHub.OnThreadInfo(InThreadInfo);
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::EnterScope()
{
	ThreadInfo->ScopeRoutes.Push(~0);
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::EnterScope(uint64 Timestamp)
{
	Timestamp = ThreadInfo->PrevTimestamp += Timestamp;
	ThreadInfo->ScopeRoutes.Push(~Timestamp);
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::LeaveScope()
{
	State.Timing.EventTimestamp = 0;
	DispatchLeaveScope();
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::LeaveScope(uint64 Timestamp)
{
	Timestamp = ThreadInfo->PrevTimestamp += Timestamp;
	State.Timing.EventTimestamp = Timestamp;
	DispatchLeaveScope();
	State.Timing.EventTimestamp = 0;
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisBridge::DispatchLeaveScope()
{
	if (ThreadInfo->ScopeRoutes.Num() <= 0)
	{
		// Leave scope without a corresponding enter
		return;
	}

	int64 ScopeValue = int64(ThreadInfo->ScopeRoutes.Pop(false));
	if (ScopeValue < 0)
	{
		// enter/leave pair without an event inbetween.
		return;
	}

	const auto* TypeInfo = (FTypeRegistry::FTypeInfo*)PTRINT(ScopeValue);

	FEventDataInfo EmptyEventInfo = {
		nullptr,
		*TypeInfo
	};

	IAnalyzer::FOnEventContext Context = {
		*(const IAnalyzer::FThreadInfo*)ThreadInfo,
		(const IAnalyzer::FEventTime&)(State.Timing),
		(const IAnalyzer::FEventData&)EmptyEventInfo,
	};

	AnalyzerHub.OnEvent(*TypeInfo, IAnalyzer::EStyle::LeaveScope, Context);
}



// {{{1 machine ----------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
class FAnalysisMachine
{
public:
	enum class EStatus
	{
		Error,
		Abort,
		NotEnoughData,
		EndOfStream,
		Continue,
		Sync,
	};

	struct FMachineContext
	{
		FAnalysisMachine&	Machine;
		FAnalysisBridge&	Bridge;
	};

	class FStage
	{
	public:
		typedef FAnalysisMachine::FMachineContext	FMachineContext;
		typedef FAnalysisMachine::EStatus			EStatus;

		virtual				~FStage() {}
		virtual EStatus		OnData(FStreamReader& Reader, const FMachineContext& Context) = 0;
		virtual void		EnterStage(const FMachineContext& Context) {};
		virtual void		ExitStage(const FMachineContext& Context) {};
	};

							FAnalysisMachine(FAnalysisBridge& InBridge);
							~FAnalysisMachine();
	EStatus					OnData(FStreamReader& Reader);
	void					Transition();
	template <class StageType, typename... ArgsType>
	StageType*				QueueStage(ArgsType... Args);

private:
	void					CleanUp();
	FAnalysisBridge&		Bridge;
	FStage*					ActiveStage = nullptr;
	TArray<FStage*>			StageQueue;
	TArray<FStage*>			DeadStages;
};

////////////////////////////////////////////////////////////////////////////////
FAnalysisMachine::FAnalysisMachine(FAnalysisBridge& InBridge)
: Bridge(InBridge)
{
}

////////////////////////////////////////////////////////////////////////////////
FAnalysisMachine::~FAnalysisMachine()
{
	CleanUp();
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisMachine::CleanUp()
{
	for (FStage* Stage : DeadStages)
	{
		delete Stage;
	}
	DeadStages.Reset();
}

////////////////////////////////////////////////////////////////////////////////
template <class StageType, typename... ArgsType>
StageType* FAnalysisMachine::QueueStage(ArgsType... Args)
{
	StageType* Stage = new StageType(Args...);
	StageQueue.Insert(Stage, 0);
	return Stage;
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisMachine::Transition()
{
	if (ActiveStage != nullptr)
	{
		FMachineContext Context = { *this, Bridge };
		ActiveStage->ExitStage(Context);

		DeadStages.Add(ActiveStage);
	}

	ActiveStage = (StageQueue.Num() > 0) ? StageQueue.Pop() : nullptr;

	if (ActiveStage != nullptr)
	{
		FMachineContext Context = { *this, Bridge };
		ActiveStage->EnterStage(Context);
	}
}

////////////////////////////////////////////////////////////////////////////////
FAnalysisMachine::EStatus FAnalysisMachine::OnData(FStreamReader& Reader)
{
	FMachineContext Context = { *this, Bridge };
	EStatus Ret;
	do
	{
		CleanUp();
		check(ActiveStage != nullptr);
		Ret = ActiveStage->OnData(Reader, Context);
	}
	while (Ret == EStatus::Continue);
	return Ret;
}



// {{{1 protocol-0 -------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FProtocol0Stage
	: public FAnalysisMachine::FStage
{
public:
						FProtocol0Stage(FTransport* InTransport);
						~FProtocol0Stage();
	virtual EStatus		OnData(FStreamReader& Reader, const FMachineContext& Context) override;
	virtual void		EnterStage(const FMachineContext& Context) override;

private:
	FTypeRegistry		TypeRegistry;
	FTransport*			Transport;
};

////////////////////////////////////////////////////////////////////////////////
FProtocol0Stage::FProtocol0Stage(FTransport* InTransport)
: Transport(InTransport)
{
}

////////////////////////////////////////////////////////////////////////////////
FProtocol0Stage::~FProtocol0Stage()
{
	delete Transport;
}

////////////////////////////////////////////////////////////////////////////////
void FProtocol0Stage::EnterStage(const FMachineContext& Context)
{
	Context.Bridge.SetActiveThread(0);
}

////////////////////////////////////////////////////////////////////////////////
FProtocol0Stage::EStatus FProtocol0Stage::OnData(
	FStreamReader& Reader,
	const FMachineContext& Context)
{
	Transport->SetReader(Reader);

	while (true)
	{
		const auto* Header = Transport->GetPointer<Protocol0::FEventHeader>();
		if (Header == nullptr)
		{
			break;
		}

		uint32 BlockSize = Header->Size + sizeof(Protocol0::FEventHeader);
		Header = Transport->GetPointer<Protocol0::FEventHeader>(BlockSize);
		if (Header == nullptr)
		{
			break;
		}

		uint32 Uid = uint32(Header->Uid) & uint32(Protocol0::EKnownEventUids::UidMask);

		if (Uid == uint32(Protocol0::EKnownEventUids::NewEvent))
		{
			// There is no need to check size here as the runtime never builds
			// packets that fragment new-event events.
			const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Add(Header->EventData, 0);
			Context.Bridge.OnNewType(TypeInfo);
			Transport->Advance(BlockSize);
			continue;
		}

		const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Get(Uid);
		if (TypeInfo == nullptr)
		{
			return EStatus::Error;
		}

		FEventDataInfo EventDataInfo = {
			Header->EventData,
			*TypeInfo,
			nullptr,
			Header->Size
		};

		Context.Bridge.OnEvent(EventDataInfo);

		Transport->Advance(BlockSize);
	}

	return Reader.IsEmpty() ? EStatus::EndOfStream : EStatus::NotEnoughData;
}



// {{{1 protocol-2 -------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FProtocol2Stage
	: public FAnalysisMachine::FStage
{
public:
								FProtocol2Stage(uint32 Version, FTransport* InTransport);
								~FProtocol2Stage();
	virtual EStatus 			OnData(FStreamReader& Reader, const FMachineContext& Context) override;
	virtual void				EnterStage(const FMachineContext& Context) override;

protected:
	virtual int32				OnData(FStreamReader& Reader, FAnalysisBridge& Bridge);
	int32						OnDataAux(FStreamReader& Reader, FAuxDataCollector& Collector);
	FTypeRegistry				TypeRegistry;
	FTransport*					Transport;
	uint32						ProtocolVersion;
	uint32						SerialInertia = ~0u;
};

////////////////////////////////////////////////////////////////////////////////
FProtocol2Stage::FProtocol2Stage(uint32 Version, FTransport* InTransport)
: Transport(InTransport)
, ProtocolVersion(Version)
{
	switch (ProtocolVersion)
	{
	case Protocol0::EProtocol::Id:
	case Protocol1::EProtocol::Id:
	case Protocol2::EProtocol::Id:
		{
			FDispatchBuilder Dispatch;
			Dispatch.SetUid(uint16(Protocol2::EKnownEventUids::NewEvent));
			Dispatch.SetLoggerName("$Trace");
			Dispatch.SetEventName("NewEvent");
			TypeRegistry.Add(Dispatch.Finalize());
		}
		break;

	case Protocol3::EProtocol::Id:
		{
			FDispatchBuilder Dispatch;
			Dispatch.SetUid(uint16(Protocol3::EKnownEventUids::NewEvent));
			Dispatch.SetLoggerName("$Trace");
			Dispatch.SetEventName("NewEvent");
			Dispatch.SetNoSync();
			TypeRegistry.Add(Dispatch.Finalize());
		}
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
FProtocol2Stage::~FProtocol2Stage()
{
	delete Transport;
}


////////////////////////////////////////////////////////////////////////////////
void FProtocol2Stage::EnterStage(const FMachineContext& Context)
{
	auto& Serial = Context.Bridge.GetSerial();

	if (ProtocolVersion == Protocol1::EProtocol::Id)
	{
		Serial.Mask = 0x0000ffff;
	}
	else
	{
		Serial.Mask = 0x00ffffff;
	}
}

////////////////////////////////////////////////////////////////////////////////
FProtocol2Stage::EStatus FProtocol2Stage::OnData(
	FStreamReader& Reader,
	const FMachineContext& Context)
{
	auto* InnerTransport = (FTidPacketTransport*)Transport;
	InnerTransport->SetReader(Reader);
	InnerTransport->Update();

	struct FRotaItem
	{
		uint32				Serial;
		uint32				ThreadId;
		FStreamReader*		Reader;

		bool operator < (const FRotaItem& Rhs) const
		{
			int32 Delta = Rhs.Serial - Serial;
			int32 Wrapped = uint32(Delta + 0x007f'fffe) >= uint32(0x00ff'fffd);
			return (Wrapped ^ (Delta > 0)) != 0;
		}
	};
	TArray<FRotaItem> Rota;

	for (uint32 i = 0, n = InnerTransport->GetThreadCount(); i < n; ++i)
	{
		FStreamReader* ThreadReader = InnerTransport->GetThreadStream(i);
		uint32 ThreadId = InnerTransport->GetThreadId(i);
		Rota.Add({~0u, ThreadId, ThreadReader});
	}

	auto& Serial = Context.Bridge.GetSerial();
	while (true)
	{
		uint32 ActiveCount = uint32(Rota.Num());

		for (uint32 i = 0; i < ActiveCount;)
		{
			FRotaItem& RotaItem = Rota[i];

			if (int32(RotaItem.Serial) > int32(Serial.Value & Serial.Mask))
			{
				++i;
				continue;
			}

			Context.Bridge.SetActiveThread(RotaItem.ThreadId);

			uint32 AvailableSerial = OnData(*(RotaItem.Reader), Context.Bridge);
			if (int32(AvailableSerial) >= 0)
			{
				RotaItem.Serial = AvailableSerial;
				if (Rota[0].Serial > AvailableSerial)
				{
					Swap(Rota[0], RotaItem);
				}
				++i;
			}
			else
			{
				FRotaItem TempItem = RotaItem;
				TempItem.Serial = ~0u;

				for (uint32 j = i, m = ActiveCount - 1; j < m; ++j)
				{
					Rota[j] = Rota[j + 1];
				}

				Rota[ActiveCount - 1] = TempItem;
				--ActiveCount;
			}

			if (((Rota[0].Serial - Serial.Value) & Serial.Mask) == 0)
			{
				i = 0;
			}
		}

		if (ActiveCount < 1)
		{
			break;
		}

		TArrayView<FRotaItem> ActiveRota(Rota.GetData(), ActiveCount);
		Algo::Sort(ActiveRota);

		int32 MinLogSerial = Rota[0].Serial;
		if (ActiveCount > 1)
		{
			int32 MaxLogSerial = Rota[ActiveCount - 1].Serial;

			if ((uint32(MinLogSerial - Serial.Value) & Serial.Mask) == 0)
			{
				continue;
			}

			// If min/max are more than half the serial range apart consider them
			// as having wrapped.
			int32 HalfRange = int32(Serial.Mask >> 1);
			if ((MaxLogSerial - MinLogSerial) >= HalfRange)
			{
				for (uint32 i = 0; i < ActiveCount; ++i)
				{
					FRotaItem& RotaItem = Rota[i];
					if (int32(RotaItem.Serial) >= HalfRange)
					{
						MinLogSerial = RotaItem.Serial;
						break;
					}
				}
			}
		}

		// If the current serial has its MSB set we're currently in a mode trying
		// to derive the best starting serial.
		if (int32(Serial.Value) < int32(0xc0000000))
		{
			Serial.Value = (MinLogSerial & Serial.Mask);
			continue;
		}

		// If we didn't stumble across the next serialised event we have done all
		// we can for now.
		if ((uint32(MinLogSerial - Serial.Value) & Serial.Mask) != 0)
		{
			break;
		}
	}

	// Patch the serial value to try and recover from gaps. Note that the bits
	// used to wait for the serial-sync event are ignored as that event may never
	// be reached if leading events are synchronised. Some inertia is added as
	// the missing range of events can be in subsequent packets. (Maximum inertia
	// need only be the size of the tail / io-read-size).
	if (Rota.Num() > 0)
	{
		int32 LowestSerial = int32(Rota[0].Serial);
		if (LowestSerial >= 0)
		{
			enum {
				InertiaLen		= 0x20,
				InertiaStart	= 0x7f - InertiaLen,
				InertiaBase		= InertiaStart << 25,	// leaves bit 24 unset so
				InertiaInc		= 1 << 25,				// ~0u can never happen
			};
			if (SerialInertia == ~0u)
			{
				SerialInertia = LowestSerial + InertiaBase;
			}
			else
			{
				SerialInertia += InertiaInc;
				if (int32(SerialInertia) >= 0)
				{
					if (SerialInertia == LowestSerial)
					{
						SerialInertia = ~0u;
						Serial.Value = LowestSerial;
						return OnData(Reader, Context);
					}
					SerialInertia = ~0u;
				}
			}
		}
	}

	return Reader.IsEmpty() ? EStatus::EndOfStream : EStatus::NotEnoughData;
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol2Stage::OnData(FStreamReader& Reader, FAnalysisBridge& Bridge)
{
	auto& Serial = Bridge.GetSerial();

	while (true)
	{
		auto Mark = Reader.SaveMark();

		const auto* Header = Reader.GetPointer<Protocol2::FEventHeader>();
		if (Header == nullptr)
		{
			break;
		}

		uint32 Uid = uint32(Header->Uid) & uint32(Protocol2::EKnownEventUids::UidMask);

		const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Get(Uid);
		if (TypeInfo == nullptr)
		{
			// Event-types may not to be discovered in Uid order.
			break;
		}

		uint32 BlockSize = Header->Size;

		// Make sure we consume events in the correct order
		if ((TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_NoSync) == 0)
		{
			switch (ProtocolVersion)
			{
			case Protocol1::EProtocol::Id:
				{
					const auto* HeaderV1 = (Protocol1::FEventHeader*)Header;
					if (HeaderV1->Serial != (Serial.Value & Serial.Mask))
					{
						return HeaderV1->Serial;
					}
					BlockSize += sizeof(*HeaderV1);
				}
				break;

			case Protocol2::EProtocol::Id:
			case Protocol3::EProtocol::Id:
				{
					const auto* HeaderSync = (Protocol2::FEventHeaderSync*)Header;
					uint32 EventSerial = HeaderSync->SerialLow|(uint32(HeaderSync->SerialHigh) << 16);
					if (EventSerial != (Serial.Value & Serial.Mask))
					{
						return EventSerial;
					}
					BlockSize += sizeof(*HeaderSync);
				}
				break;
			}
		}
		else
		{
			BlockSize += sizeof(*Header);
		}

		if (Reader.GetPointer(BlockSize) == nullptr)
		{
			break;
		}

		Reader.Advance(BlockSize);

		FAuxDataCollector AuxCollector;
		if (TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_MaybeHasAux)
		{
			int AuxStatus = OnDataAux(Reader, AuxCollector);
			if (AuxStatus == 0)
			{
				Reader.RestoreMark(Mark);
				break;
			}
		}

		if ((TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_NoSync) == 0)
		{
			Serial.Value += 1;
			Serial.Value &= 0x3fffffff; // Don't set MSBs. They have other uses
		}

		auto* EventData = (const uint8*)Header + BlockSize - Header->Size;
		if (Uid == uint32(Protocol2::EKnownEventUids::NewEvent))
		{
			// There is no need to check size here as the runtime never builds
			// packets that fragment new-event events.
			TypeInfo = TypeRegistry.Add(EventData, 0);
			Bridge.OnNewType(TypeInfo);
		}
		else
		{
			FEventDataInfo EventDataInfo = {
				EventData,
				*TypeInfo,
				&AuxCollector,
				Header->Size,
			};

			Bridge.OnEvent(EventDataInfo);
		}
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol2Stage::OnDataAux(FStreamReader& Reader, FAuxDataCollector& Collector)
{
	while (true)
	{
		const uint8* NextByte = Reader.GetPointer<uint8>();
		if (NextByte == nullptr)
		{
			return 0;
		}

		// Is the following sequence a blob of auxilary data or the null
		// terminator byte?
		if (NextByte[0] == 0)
		{
			Reader.Advance(1);
			return 1;
		}

		// Get header and the auxilary blob's size
		const auto* Header = Reader.GetPointer<Protocol1::FAuxHeader>();
		if (Header == nullptr)
		{
			return 0;
		}

		// Check it exists
		uint32 BlockSize = (Header->Size >> 8) + sizeof(*Header);
		if (Reader.GetPointer(BlockSize) == nullptr)
		{
			return 0;
		}

		// Attach to event
		FAuxData AuxData = {};
		AuxData.Data = Header->Data;
		AuxData.DataSize = uint32(BlockSize - sizeof(*Header));
		AuxData.FieldIndex = uint16(Header->FieldIndex & Protocol1::FAuxHeader::FieldMask);
		Collector.Push(AuxData);

		Reader.Advance(BlockSize);
	}
}



// {{{1 protocol-4 -------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FProtocol4Stage
	: public FProtocol2Stage
{
public:
					FProtocol4Stage(uint32 Version, FTransport* InTransport);

private:
	virtual int32	OnData(FStreamReader& Reader, FAnalysisBridge& Bridge) override;
	int32			OnDataImpl(FStreamReader& Reader, FAnalysisBridge& Bridge);
	int32			OnDataKnown(uint32 Uid, FStreamReader& Reader, FAnalysisBridge& Bridge);
};

////////////////////////////////////////////////////////////////////////////////
FProtocol4Stage::FProtocol4Stage(uint32 Version, FTransport* InTransport)
: FProtocol2Stage(Version, InTransport)
{
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol4Stage::OnData(FStreamReader& Reader, FAnalysisBridge& Bridge)
{
	while (true)
	{
		if (int32 TriResult = OnDataImpl(Reader, Bridge))
		{
			return (TriResult < 0) ? ~TriResult : -1;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol4Stage::OnDataImpl(FStreamReader& Reader, FAnalysisBridge& Bridge)
{
	auto& Serial = Bridge.GetSerial();

	/* Returns 0 if an event was successfully processed, 1 if there's not enough
	 * data available, or ~AvailableLogSerial if the pending event is in the future */

	auto Mark = Reader.SaveMark();

	const auto* UidCursor = Reader.GetPointer<uint8>();
	if (UidCursor == nullptr)
	{
		return 1;
	}

	uint32 UidBytes = 1 + !!(*UidCursor & Protocol4::EKnownEventUids::Flag_TwoByteUid);
	if (UidBytes > 1 && Reader.GetPointer(UidBytes) == nullptr)
	{
		return 1;
	}

	uint32 Uid = ~0u;
	switch (UidBytes)
	{
		case 1:	Uid = *UidCursor;			break;
		case 2:	Uid = *(uint16*)UidCursor;	break;
	}
	Uid >>= Protocol4::EKnownEventUids::_UidShift;

	if (Uid < Bridge.GetUserUidBias())
	{
		Reader.Advance(UidBytes);
		if (!OnDataKnown(Uid, Reader, Bridge))
		{
			Reader.RestoreMark(Mark);
			return 1;
		}
		return 0;
	}

	// Do we know about this event type yet?
	const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Get(Uid);
	if (TypeInfo == nullptr)
	{
		return 1;
	}

	// Parse the header
	const auto* Header = Reader.GetPointer<Protocol4::FEventHeader>();
	if (Header == nullptr)
	{
		return 1;
	}

	uint32 BlockSize = Header->Size;

	// Make sure we consume events in the correct order
	if ((TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_NoSync) == 0)
	{
		if (Reader.GetPointer<Protocol4::FEventHeaderSync>() == nullptr)
		{
			return 1;
		}
		
		const auto* HeaderSync = (Protocol4::FEventHeaderSync*)Header;
		uint32 EventSerial = HeaderSync->SerialLow|(uint32(HeaderSync->SerialHigh) << 16);
		if (EventSerial != (Serial.Value & Serial.Mask))
		{
			return ~EventSerial;
		}
		BlockSize += sizeof(*HeaderSync);
	}
	else
	{
		BlockSize += sizeof(*Header);
	}

	// Is all the event's data available?
	if (Reader.GetPointer(BlockSize) == nullptr)
	{
		return 1;
	}

	Reader.Advance(BlockSize);

	// Collect auxiliary data
	FAuxDataCollector AuxCollector;
	if (TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_MaybeHasAux)
	{
		// Important events' size may include their array data so we need to backtrack
		auto NextMark = Reader.SaveMark();
		if (TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_Important)
		{
			Reader.RestoreMark(Mark);
			Reader.Advance(sizeof(Protocol4::FEventHeader) + TypeInfo->EventSize);
		}

		int AuxStatus = OnDataAux(Reader, AuxCollector);
		if (AuxStatus == 0)
		{
			Reader.RestoreMark(Mark);
			return 1;
		}

		// User error could have resulted in less space being used that was
		// allocated for important events. So we can't assume that aux data
		// reading has read all the way up to the next event. So we use marks
		if (TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_Important)
		{
			Reader.RestoreMark(NextMark);
		}
	}

	// Maintain sync
	if ((TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_NoSync) == 0)
	{
		Serial.Value += 1;
		Serial.Value &= 0x7fffffff; // don't set msb. that has other uses
	}

	// Sent the event to subscribed analyzers
	FEventDataInfo EventDataInfo = {
		(const uint8*)Header + BlockSize - Header->Size,
		*TypeInfo,
		&AuxCollector,
		Header->Size,
	};

	Bridge.OnEvent(EventDataInfo);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol4Stage::OnDataKnown(
	uint32 Uid,
	FStreamReader& Reader,
	FAnalysisBridge& Bridge)
{
	switch (Uid)
	{
	case Protocol4::EKnownEventUids::NewEvent:
		{
			const auto* Size = Reader.GetPointer<uint16>();
			const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Add(Size + 1, 4);
			Bridge.OnNewType(TypeInfo);
			Reader.Advance(sizeof(*Size) + *Size);
			return 1;
		}
		break;

	case Protocol4::EKnownEventUids::EnterScope:
	case Protocol4::EKnownEventUids::EnterScope_T:
		if (Uid > Protocol4::EKnownEventUids::EnterScope)
		{
			const uint8* Stamp = Reader.GetPointer(sizeof(uint64) - 1);
			if (Stamp == nullptr)
			{
				break;
			}

			uint64 Timestamp = *(uint64*)(Stamp - 1) >> 8;
			Bridge.EnterScope(Timestamp);

			Reader.Advance(sizeof(uint64) - 1);
		}
		else
		{
			Bridge.EnterScope();
		}
		return 1;

	case Protocol4::EKnownEventUids::LeaveScope:
	case Protocol4::EKnownEventUids::LeaveScope_T:
		if (Uid > Protocol4::EKnownEventUids::LeaveScope)
		{
			const uint8* Stamp = Reader.GetPointer(sizeof(uint64) - 1);
			if (Stamp == nullptr)
			{
				break;
			}

			uint64 Timestamp = *(uint64*)(Stamp - 1) >> 8;
			Bridge.LeaveScope(Timestamp);

			Reader.Advance(sizeof(uint64) - 1);
		}
		else
		{
			Bridge.LeaveScope();
		}
		return 1;
	};

	return 0;
}



// {{{1 protocol-5 -------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
DEFINE_LOG_CATEGORY_STATIC(LogTraceAnalysis, Log, All)

#if 0
#	define TRACE_ANALYSIS_DEBUG(Format, ...) \
		do { UE_LOG(LogTraceAnalysis, Log, TEXT(Format), __VA_ARGS__) } while (0)
#else
#	define TRACE_ANALYSIS_DEBUG(...)
#endif

////////////////////////////////////////////////////////////////////////////////
class FProtocol5Stage
	: public FAnalysisMachine::FStage
{
public:
							FProtocol5Stage(FTransport* InTransport);

protected:
	struct alignas(16) FEventDesc
	{
		union
		{
			struct
			{
				int32		Serial;
				uint16		Uid			: 14;
				uint16		bTwoByteUid	: 1;
				uint16		bHasAux		: 1;
				uint16		AuxKey;
			};
			uint64			Meta = 0;
		};
		union
		{
			uint32			GapLength;
			const uint8*	Data;
		};
	};
	static_assert(sizeof(FEventDesc) == 16, "");

	struct alignas(16) FEventDescStream
	{
		uint32					ThreadId;
		uint32					TransportIndex;
		union
		{
			uint32				ContainerIndex;
			const FEventDesc*	EventDescs;
		};

		enum { GapThreadId = ~0u };

		bool operator < (const FEventDescStream& Rhs) const;
	};
	static_assert(sizeof(FEventDescStream) == 16, "");

	enum ESerial : int32 
	{
		Bits		= 24,
		Mask		= (1 << Bits) - 1,
		Range		= 1 << Bits,
		HalfRange	= Range >> 1,
		Ignored		= Range << 2, // far away so proper serials always compare less-than
		Terminal,
	};

	using EventDescArray	= TArray<FEventDesc>;
	using EKnownUids		= Protocol5::EKnownEventUids;


	virtual EStatus			OnData(FStreamReader& Reader, const FMachineContext& Context) override;
	EStatus					OnDataNewEvents(const FMachineContext& Context);
	EStatus					OnDataImportant(const FMachineContext& Context);
	EStatus					OnDataNonCachedImportant(const FMachineContext& Context);
	EStatus					OnDataNormal(const FMachineContext& Context);
	int32					ParseImportantEvents(FStreamReader& Reader, EventDescArray& OutEventDescs);
	int32					ParseEvents(FStreamReader& Reader, EventDescArray& OutEventDescs);
	int32					ParseEventsWithAux(FStreamReader& Reader, EventDescArray& OutEventDescs);
	int32					ParseEvent(FStreamReader& Reader, FEventDesc& OutEventDesc);
	int32					DispatchEvents(FAnalysisBridge& Bridge, const FEventDesc* EventDesc, uint32 Count);
	int32					DispatchEvents(FAnalysisBridge& Bridge, TArray<FEventDescStream>& EventDescHeap);
	void					DetectSerialGaps(TArray<FEventDescStream>& EventDescHeap);
	template <typename Callback>
	void					ForEachSerialGap(const TArray<FEventDescStream>& EventDescHeap, Callback&& InCallback);
	FTypeRegistry			TypeRegistry;
	FTidPacketTransport&	Transport;
	EventDescArray			EventDescs;
	EventDescArray			SerialGaps;
	uint32					NextSerial = ~0u;
	uint32					SyncCount;
	uint32					EventVersion = 4; //Protocol version 5 uses the event version from protocol 4
};

////////////////////////////////////////////////////////////////////////////////
bool FProtocol5Stage::FEventDescStream::operator < (const FEventDescStream& Rhs) const
{
	int32 Delta = Rhs.EventDescs->Serial - EventDescs->Serial;
	int32 Wrapped = uint32(Delta + ESerial::HalfRange - 1) >= uint32(ESerial::Range - 2);
	return (Wrapped ^ (Delta > 0)) != 0;
}

////////////////////////////////////////////////////////////////////////////////
FProtocol5Stage::FProtocol5Stage(FTransport* InTransport)
: Transport(*(FTidPacketTransport*)InTransport)
, SyncCount(Transport.GetSyncCount())
{
	EventDescs.Reserve(8 << 10);
}

////////////////////////////////////////////////////////////////////////////////
FProtocol5Stage::EStatus FProtocol5Stage::OnData(
	FStreamReader& Reader,
	const FMachineContext& Context)
{
	Transport.SetReader(Reader);
	Transport.Update();

	// New-events. They must be processed before anything else otherwise events
	// can not be interpreted.
	EStatus Ret = OnDataNewEvents(Context);
	if (Ret == EStatus::Error)
	{
		return Ret;
	}

	// Important events
	Ret = OnDataImportant(Context);
	if (Ret == EStatus::Error)
	{
		return Ret;
	}
	bool bNotEnoughData = (Ret == EStatus::NotEnoughData);

	// Normal events
	Ret = OnDataNormal(Context);
	if (Ret == EStatus::Error)
	{
		return Ret;
	}
	if (Ret == EStatus::Sync)
	{
		// After processing a SYNC packet, we need to read data once more.
		return OnData(Reader, Context);
	}
	bNotEnoughData |= (Ret == EStatus::NotEnoughData);

	if (bNotEnoughData)
	{
		return EStatus::NotEnoughData;
	}

	return Reader.CanMeetDemand() ? EStatus::Continue : EStatus::EndOfStream;
}

////////////////////////////////////////////////////////////////////////////////
FProtocol5Stage::EStatus FProtocol5Stage::OnDataNewEvents(const FMachineContext& Context)
{
	EventDescs.Reset();

	FStreamReader* ThreadReader = Transport.GetThreadStream(ETransportTid::Events);
	if (ThreadReader->IsEmpty())
	{
		return EStatus::EndOfStream;
	}

	if (ParseImportantEvents(*ThreadReader, EventDescs) < 0)
	{
		return EStatus::Error;
	}

	for (const FEventDesc& EventDesc : EventDescs)
	{
		const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Add(EventDesc.Data, EventVersion);
		Context.Bridge.OnNewType(TypeInfo);
	}

	return EStatus::EndOfStream;
}

////////////////////////////////////////////////////////////////////////////////
FProtocol5Stage::EStatus FProtocol5Stage::OnDataImportant(const FMachineContext& Context)
{
	static_assert(ETransportTid::Importants == ETransportTid::Internal, "It is assumed there is only one 'important' thread stream");

	EventDescs.Reset();

	FStreamReader* ThreadReader = Transport.GetThreadStream(ETransportTid::Importants);
	if (ThreadReader->IsEmpty())
	{
		return EStatus::EndOfStream;
	}

	if (ParseImportantEvents(*ThreadReader, EventDescs) < 0)
	{
		return EStatus::Error;
	}

	bool bNotEnoughData = !ThreadReader->IsEmpty();

	if (EventDescs.Num() <= 0)
	{
		return bNotEnoughData ? EStatus::NotEnoughData : EStatus::EndOfStream;
	}

	// Dispatch looks ahead to the next desc looking for runs of aux blobs. As
	// such we should add a terminal desc for it to read. Note the "- 1" too.
	FEventDesc& EventDesc = EventDescs.Emplace_GetRef();
	EventDesc.Serial = ESerial::Terminal;

	Context.Bridge.SetActiveThread(ETransportTid::Importants);
	if (DispatchEvents(Context.Bridge, EventDescs.GetData(), EventDescs.Num() - 1) < 0)
	{
		return EStatus::Error;
	}

	return bNotEnoughData ? EStatus::NotEnoughData : EStatus::EndOfStream;
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol5Stage::ParseImportantEvents(FStreamReader& Reader, EventDescArray& OutEventDescs)
{
	using namespace Protocol5;

	while (true)
	{
		int32 Remaining = Reader.GetRemaining();
		if (Remaining < sizeof(FImportantEventHeader))
		{
			return 1;
		}

		const auto* Header = Reader.GetPointerUnchecked<FImportantEventHeader>();
		if (Remaining < int32(Header->Size) + sizeof(FImportantEventHeader))
		{
			return 1;
		}

		uint32 Uid = Header->Uid;

		FEventDesc EventDesc;
		EventDesc.Serial = ESerial::Ignored;
		EventDesc.Uid = Uid;
		EventDesc.Data = Header->Data;

		// Special case for new events. It would work to add a 0 type to the
		// registry but this way avoid raveling things together.
		if (Uid == EKnownUids::NewEvent)
		{
			OutEventDescs.Add(EventDesc);
			Reader.Advance(sizeof(*Header) + Header->Size);
			continue;
		}

		const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Get(Uid);
		if (TypeInfo == nullptr)
		{
			UE_LOG(LogTraceAnalysis, Log, TEXT("Unknown event UID: %08x"), Uid);
			return 1;
		}

		if (TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_MaybeHasAux)
		{
			EventDesc.bHasAux = 1;
		}

		OutEventDescs.Add(EventDesc);

		if (TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_MaybeHasAux)
		{
			const uint8* Cursor = Header->Data + TypeInfo->EventSize;
			const uint8* End = Header->Data + Header->Size;
			while (Cursor <= End)
			{
				if (Cursor[0] == uint8(EKnownUids::AuxDataTerminal))
				{
					break;
				}

				const auto* AuxHeader = (FAuxHeader*)Cursor;

				FEventDesc& AuxDesc = OutEventDescs.Emplace_GetRef();
				AuxDesc.Uid = uint8(EKnownUids::AuxData);
				AuxDesc.Data = AuxHeader->Data;
				AuxDesc.Serial = ESerial::Ignored;

				Cursor = AuxHeader->Data + (AuxHeader->Pack >> FAuxHeader::SizeShift);
			}

			if (Cursor[0] != uint8(EKnownUids::AuxDataTerminal))
			{
				UE_LOG(LogTraceAnalysis, Warning, TEXT("Expecting AuxDataTerminal event"));
				return -1;
			}
		}

		Reader.Advance(sizeof(*Header) + Header->Size);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
FProtocol5Stage::EStatus FProtocol5Stage::OnDataNormal(const FMachineContext& Context)
{
	// Ordinary events

	EventDescs.Reset();
	bool bNotEnoughData = false;

	TArray<FEventDescStream> EventDescHeap;
	EventDescHeap.Reserve(Transport.GetThreadCount());

	for (uint32 i = ETransportTid::Bias, n = Transport.GetThreadCount(); i < n; ++i)
	{
		uint32 NumEventDescs = EventDescs.Num();

		TRACE_ANALYSIS_DEBUG("Thread: %03d Id:%04x", i, Transport.GetThreadId(i));
		
		// Extract all the events in the stream for this thread
		FStreamReader* ThreadReader = Transport.GetThreadStream(i);
		if (ParseEvents(*ThreadReader, EventDescs) < 0)
		{
			return EStatus::Error;
		}

		bNotEnoughData |= !ThreadReader->IsEmpty();

		if (uint32(EventDescs.Num()) != NumEventDescs)
		{
			// Add a dummy event to delineate the end of this thread's events
			FEventDesc& EventDesc = EventDescs.Emplace_GetRef();
			EventDesc.Serial = ESerial::Terminal;

			FEventDescStream Out;
			Out.ThreadId = Transport.GetThreadId(i);
			Out.TransportIndex = i;
			Out.ContainerIndex = NumEventDescs;
			EventDescHeap.Add(Out);
		}

		TRACE_ANALYSIS_DEBUG("Thread: %03d bNotEnoughData:%d", i, bNotEnoughData);
	}

	// Now EventDescs is stable we can convert the indices into pointers
	for (FEventDescStream& Stream : EventDescHeap)
	{
		Stream.EventDescs = EventDescs.GetData() + Stream.ContainerIndex;
	}

	// Process leading unsynchronised events so that each stream starts with a
	// sychronised event.
	for (FEventDescStream& Stream : EventDescHeap)
	{
		// Extract a run of consecutive unsynchronised events
		const FEventDesc* EndDesc = Stream.EventDescs;
		for (; EndDesc->Serial == ESerial::Ignored; ++EndDesc);

		// Dispatch.
		const FEventDesc* StartDesc = Stream.EventDescs;
		int32 DescNum = int32(UPTRINT(EndDesc - StartDesc));
		if (DescNum > 0)
		{
			Context.Bridge.SetActiveThread(Stream.ThreadId);

			if (DispatchEvents(Context.Bridge, StartDesc, DescNum) < 0)
			{
				return EStatus::Error;
			}

			Stream.EventDescs = EndDesc;
		}
	}

	// Trim off empty streams
	EventDescHeap.RemoveAllSwap([] (const FEventDescStream& Stream)
	{
		return (Stream.EventDescs->Serial == ESerial::Terminal);
	});

	// Early out if there isn't any events available.
	if (UNLIKELY(EventDescHeap.IsEmpty()))
	{
		return bNotEnoughData ? EStatus::NotEnoughData : EStatus::EndOfStream;
	}

	// Provided that less than approximately "SerialRange * BytesPerSerial"
	// is buffered there should never be more that "SerialRange / 2" serial
	// numbers. Thus if the distance between any two serial numbers is larger
	// than half the serial space, they have wrapped.

	// A min-heap is used to peel off groups of events by lowest serial
	EventDescHeap.Heapify();

	// Events must be consumed contiguously.
	if (NextSerial == ~0u && Transport.GetSyncCount())
	{
		NextSerial = EventDescHeap.HeapTop().EventDescs[0].Serial;
	}

	const bool bSync = (SyncCount != Transport.GetSyncCount());
	DetectSerialGaps(EventDescHeap);

	if (DispatchEvents(Context.Bridge, EventDescHeap) < 0)
	{
		return EStatus::Error;
	}

	if (bSync)
	{
		return EStatus::Sync;
	}

	return bNotEnoughData ? EStatus::NotEnoughData : EStatus::EndOfStream;
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol5Stage::DispatchEvents(
	FAnalysisBridge& Bridge,
	TArray<FEventDescStream>& EventDescHeap)
{
	auto UpdateHeap = [&] (const FEventDescStream& Stream, const FEventDesc* EventDesc)
	{
		if (EventDesc->Serial != ESerial::Terminal)
		{
			FEventDescStream Next = Stream;
			Next.EventDescs = EventDesc;
			EventDescHeap.Add(Next);
		}

		EventDescHeap.HeapPopDiscard();
	};

	do
	{
		const FEventDescStream& Stream = EventDescHeap.HeapTop();
		const FEventDesc* StartDesc = Stream.EventDescs;
		const FEventDesc* EndDesc = StartDesc;

		// DetectSerialGaps() will add a special stream that communicates gaps
		// in in serial numbers, gaps that will never be resolved. Thread IDs
		// are uint16 everywhere else so they will never collide with GapThreadId.
		if (Stream.ThreadId == FEventDescStream::GapThreadId)
		{
			NextSerial = EndDesc->Serial + EndDesc->GapLength;
			NextSerial &= ESerial::Mask;
			UpdateHeap(Stream, EndDesc + 1);
			continue;
		}

		// Extract a run of consecutive events (plus runs of unsynchronised ones)
		if (EndDesc->Serial == NextSerial)
		{
			do
			{
				NextSerial = (NextSerial + 1) & ESerial::Mask;

				do
				{
					++EndDesc;
				}
				while (EndDesc->Serial == ESerial::Ignored);
			}
			while (EndDesc->Serial == NextSerial);
		}
		else
		{
			// The lowest known serial number is not low enough so we are unable
			// to proceed any further.
			break;
		}

		// Dispatch.
		Bridge.SetActiveThread(Stream.ThreadId);
		int32 DescNum = int32(UPTRINT(EndDesc - StartDesc));
		check(DescNum > 0);
		if (DispatchEvents(Bridge, StartDesc, DescNum) < 0)
		{
			return -1;
		}

		UpdateHeap(Stream, EndDesc);
	}
	while (!EventDescHeap.IsEmpty());

	// If there are any streams left in the heap then we are unable to proceed
	// until more data is received. We'll rewind the streams until more data is
	// available. It is an efficient way to do things, but it is simple way.
	for (FEventDescStream& Stream : EventDescHeap)
	{
		const FEventDesc& EventDesc = Stream.EventDescs[0];
		uint32 HeaderSize = 1 + EventDesc.bTwoByteUid + (ESerial::Bits / 8);

		FStreamReader* Reader = Transport.GetThreadStream(Stream.TransportIndex);
		Reader->Backtrack(EventDesc.Data - HeaderSize);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol5Stage::ParseEvents(FStreamReader& Reader, EventDescArray& OutEventDescs)
{
	while (!Reader.IsEmpty())
	{
		FEventDesc EventDesc;
		int32 Size = ParseEvent(Reader, EventDesc);
		if (Size <= 0)
		{
			return Size;
		}

		TRACE_ANALYSIS_DEBUG("Event: %04d Uid:%04x Serial:%08x", OutEventDescs.Num(), EventDesc.Uid, EventDesc.Serial);
		OutEventDescs.Add(EventDesc);

		if (EventDesc.bHasAux)
		{
			uint32 RewindDescsNum = OutEventDescs.Num() - 1;
			auto RewindMark = Reader.SaveMark();

			Reader.Advance(Size);

			int Ok = ParseEventsWithAux(Reader, OutEventDescs);
			if (Ok < 0)
			{
				return Ok;
			}

			if (Ok == 0)
			{
				OutEventDescs.SetNum(RewindDescsNum);
				Reader.RestoreMark(RewindMark);
				break;
			}

			continue;
		}

		Reader.Advance(Size);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol5Stage::ParseEventsWithAux(FStreamReader& Reader, EventDescArray& OutEventDescs)
{
	// We are now "in" the scope of an event with zero or more aux-data blocks.
	// We will consume events until we leave this scope (a aux-data-terminal).
	// A running key is assigned to each event with a gap left following events
	// that may have aux-data blocks. Aux-data blocks are assigned a key that
	// fits in these gaps. Once sorted by this key, events maintain their order
	// while aux-data blocks are moved to directly follow their owners.

	TArray<uint16, TInlineAllocator<8>> AuxKeyStack = { 0 };
	uint32 AuxKey = 2;

	uint32 FirstDescIndex = OutEventDescs.Num();
	bool bUnsorted = false;

	while (!Reader.IsEmpty())
	{
		FEventDesc EventDesc;
		EventDesc.Serial = ESerial::Ignored;

		int32 Size = ParseEvent(Reader, EventDesc);
		if (Size <= 0)
		{
			return Size;
		}

		TRACE_ANALYSIS_DEBUG("Event: %04d Uid:%04x Serial:%08x", OutEventDescs.Num(), EventDesc.Uid, EventDesc.Serial);

		Reader.Advance(Size);

		if (EventDesc.Uid == EKnownUids::AuxDataTerminal)
		{
			// Leave the scope of an aux-owning event.
			if (AuxKeyStack.Pop() == 0)
			{
				break;
			}
			continue;
		}
		else if (EventDesc.Uid == EKnownUids::AuxData)
		{
			// Move an aux-data block to follow its owning event
			EventDesc.AuxKey = AuxKeyStack.Last() + 1;
		}
		else
		{
			EventDesc.AuxKey = uint16(AuxKey);

			// Maybe it is time to create a new aux-data owner scope
			if (EventDesc.bHasAux)
			{
				AuxKeyStack.Add(uint16(AuxKey));
			}

			// This event maybe in the middle of an earlier event's aux data blocks.
			bUnsorted = true;
		}

		OutEventDescs.Add(EventDesc);

		++AuxKey;
	}

	if (AuxKeyStack.Num() > 0)
	{
		// There was not enough data available to complete the outer most scope
		return 0;
	}

	checkf((AuxKey & 0xffff0000) == 0, TEXT("AuxKey overflow (%08x)"), AuxKey);

	// Sort to get all aux-blocks contiguous with their owning event
	if (bUnsorted)
	{
		uint32 NumDescs = OutEventDescs.Num() - FirstDescIndex;
		TArrayView<FEventDesc> DescsView(OutEventDescs.GetData() + FirstDescIndex, NumDescs);
		Algo::StableSort(
			DescsView,
			[] (const FEventDesc& Lhs, const FEventDesc& Rhs)
			{
				return Lhs.AuxKey < Rhs.AuxKey;
			}
		);
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol5Stage::ParseEvent(FStreamReader& Reader, FEventDesc& EventDesc)
{
	using namespace Protocol5;

	// No need to aggressively bounds check here. Events are never fragmented
	// due to the way that data is transported (aux payloads can be though).
	const uint8* Cursor = Reader.GetPointerUnchecked<uint8>();

	// Parse the event's ID
	uint32 Uid = *Cursor;
	if (Uid & EKnownUids::Flag_TwoByteUid)
	{
		EventDesc.bTwoByteUid = 1;
		Uid = *(uint16*)Cursor;
		++Cursor;
	}
	Uid >>= EKnownUids::_UidShift;
	++Cursor;

	// Calculate the size of the event
	uint32 Serial = uint32(ESerial::Ignored);
	uint32 EventSize = 0;
	if (Uid < EKnownUids::User)
	{
		/* Well-known event */

		if (Uid == Protocol5::EKnownEventUids::AuxData)
		{
			--Cursor; // FAuxHeader includes the one-byte Uid
			const auto* AuxHeader = (FAuxHeader*)Cursor;

			uint32 Remaining = Reader.GetRemaining();
			uint32 Size = AuxHeader->Pack >> FAuxHeader::SizeShift;
			if (Remaining < Size + sizeof(FAuxHeader))
			{
				return 0;
			}

			EventSize = Size;
			Cursor += sizeof(FAuxHeader);
		}
		else
		{
			switch (Uid)
			{
			case EKnownUids::EnterScope_T:
			case EKnownUids::LeaveScope_T:
				EventSize = 7;
			};
		}

		EventDesc.bHasAux = 0;
	}
	else
	{
		/* Ordinary events */

		const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Get(Uid);
		if (TypeInfo == nullptr)
		{
			UE_LOG(LogTraceAnalysis, Log, TEXT("Unknown event UID: %08x"), Uid);
			return 0;
		}

		EventSize = TypeInfo->EventSize;
		EventDesc.bHasAux = !!(TypeInfo->Flags & FTypeRegistry::FTypeInfo::Flag_MaybeHasAux);

		if ((TypeInfo->Flags & FDispatch::Flag_NoSync) == 0)
		{
			memcpy(&Serial, Cursor, sizeof(int32));
			Serial &= ESerial::Mask;
			Cursor += 3;
		}
	}

	EventDesc.Serial = Serial;
	EventDesc.Uid = Uid;
	EventDesc.Data = Cursor;

	uint32 HeaderSize = uint32(UPTRINT(Cursor - Reader.GetPointer<uint8>()));
	return HeaderSize + EventSize;
}

////////////////////////////////////////////////////////////////////////////////
template <typename Callback>
void FProtocol5Stage::ForEachSerialGap(
	const TArray<FEventDescStream>& EventDescHeap,
	Callback&& InCallback)
{
	TArray<FEventDescStream> HeapCopy(EventDescHeap);
	int Serial = HeapCopy.HeapTop().EventDescs[0].Serial;

	// There might be a gap at the beginning of the heap if some events have
	// already been consumed.
	if (NextSerial != Serial)
	{
		if (!InCallback(NextSerial, Serial))
		{
			return;
		}
	}

	// A min-heap is used to peel off each stream (thread) with the lowest serial
	// numbered event.
	do
	{
		const FEventDescStream& Stream = HeapCopy.HeapTop();
		const FEventDesc* EventDesc = Stream.EventDescs;

		// If the next lowest serial number doesn't match where we got up to in
		// the previous stream we have found a gap. Celebration ensues.
		if (Serial != EventDesc->Serial)
		{
			if (!InCallback(Serial, EventDesc->Serial))
			{
				return;
			}
		}

		// Consume consecutive events (including unsynchronised ones).
		Serial = EventDesc->Serial;
		do
		{
			do
			{
				++EventDesc;
			}
			while (EventDesc->Serial == ESerial::Ignored);

			Serial = (Serial + 1) & ESerial::Mask;
		}
		while (EventDesc->Serial == Serial);

		// Update the heap
		if (EventDesc->Serial != ESerial::Terminal)
		{
			auto& Out = HeapCopy.Add_GetRef({Stream.ThreadId, Stream.TransportIndex});
			Out.EventDescs = EventDesc;
		}
		HeapCopy.HeapPopDiscard();
	}
	while (!HeapCopy.IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
void FProtocol5Stage::DetectSerialGaps(TArray<FEventDescStream>& EventDescHeap)
{
	// Events that should be synchronised across threads are assigned serial
	// numbers so they can be analysed in the correct order. Gaps in the
	// serials can occur under two scenarios; 1) when packets are dropped from
	// the trace tail to make space for new trace events, and 2) when Trace's
	// worker thread ticks, samples all the trace buffers and sends their data.
	// In late-connect scenarios these gaps need to be skipped over in order to
	// successfully reserialise events in the data stream. To further complicate
	// matters, most of the gaps from (2) will get filled by the following update,
	// leading to initial false positive gaps. By embedding sync points in the
	// stream we can reliably differentiate genuine gaps from temporary ones.
	//
	// Note that this could be done without sync points but it is an altogether
	// more complex solution. So unsightly embedded syncs it is...

	if (SyncCount == Transport.GetSyncCount())
	{
		return;
	}

	SyncCount = Transport.GetSyncCount();

	if (SyncCount == 1)
	{
		// On the first update we will just collect gaps.
		auto GatherGap = [this] (int32 Lhs, int32 Rhs)
		{
			FEventDesc& Gap = SerialGaps.Emplace_GetRef();
			Gap.Serial = Lhs;
			Gap.GapLength = (Rhs - Lhs) & ESerial::Mask;
			return true;
		};
		ForEachSerialGap(EventDescHeap, GatherGap);
	}
	else
	{
		// On the second update we detect where gaps from the previous update
		// start getting filled in. Any gaps preceding that point are genuine.
		uint32 GapCount = 0;
		auto RecordGap = [this, &GapCount] (int32 Lhs, int32 Rhs)
		{
			if (SerialGaps.IsEmpty() || GapCount >= (uint32)SerialGaps.Num())
			{
				return false;
			}
			
			const FEventDesc& SerialGap = SerialGaps[GapCount];

			if (SerialGap.Serial == Lhs)
			{
				/* This is the expected case */
				++GapCount;
				return true;
			}

			if (SerialGap.Serial > Lhs)
			{
				/* We've started receiving new gaps that are exist because not all
				* data has been received yet. They're false positives. No need to process
				* any further */
				return false;
			}

			// If we're here something's probably gone wrong
			return false;
		};

		ForEachSerialGap(EventDescHeap, RecordGap);

		if (GapCount == 0) //-V547
		{
			SerialGaps.Empty();
			return;
		}

		// Turn the genuine gaps into a stream that DispatchEvents() can handle
		// and use to skip over them.

		if (GapCount == uint32(SerialGaps.Num()))
		{
			SerialGaps.Emplace();
		}
		FEventDesc& Terminator = SerialGaps[GapCount];
		Terminator.Serial = ESerial::Terminal;

		FEventDescStream Out = EventDescHeap[0];
		Out.ThreadId = FEventDescStream::GapThreadId;
		Out.EventDescs = SerialGaps.GetData();
		EventDescHeap.HeapPush(Out);
	}
}

////////////////////////////////////////////////////////////////////////////////
int32 FProtocol5Stage::DispatchEvents(
	FAnalysisBridge& Bridge,
	const FEventDesc* EventDesc, uint32 Count)
{
	using namespace Protocol5;

	FAuxDataCollector AuxCollector;

	for (const FEventDesc *Cursor = EventDesc, *End = EventDesc + Count; Cursor < End; ++Cursor)
	{
		// Maybe this is a "well-known" event that is handled a little different?
		switch (Cursor->Uid)
		{
		case EKnownUids::EnterScope_T:
		{
			uint64 Timestamp = *(uint64*)(Cursor->Data - 1) >> 8;
			Bridge.EnterScope(Timestamp);
			continue;
		}

		case EKnownUids::LeaveScope_T:
		{
			uint64 Timestamp = *(uint64*)(Cursor->Data - 1) >> 8;
			Bridge.LeaveScope(Timestamp);
			continue;
		}

		case EKnownUids::EnterScope: Bridge.EnterScope(); continue;
		case EKnownUids::LeaveScope: Bridge.LeaveScope(); continue;

		case EKnownUids::AuxData:
		case EKnownUids::AuxDataTerminal:
			continue;

		default:
			if (!TypeRegistry.IsUidValid(Cursor->Uid))
			{
				UE_LOG(LogTraceAnalysis, Warning, TEXT("Unexpected event UID: %08x"), Cursor->Uid);
				return -1;
			}
			break;
		}

		// It is a normal event.
		const FTypeRegistry::FTypeInfo* TypeInfo = TypeRegistry.Get(Cursor->Uid);
		FEventDataInfo EventDataInfo = {
			Cursor->Data,
			*TypeInfo,
			&AuxCollector,
			TypeInfo->EventSize,
		};

		// Gather its auxiliary data blocks into a collector.
		if (Cursor->bHasAux)
		{
			while (true)
			{
				++Cursor;

				if (Cursor->Uid != EKnownUids::AuxData)
				{
					--Cursor; // Read off too much. Put it back.
					break;
				}

				const auto* AuxHeader = ((FAuxHeader*)(Cursor->Data)) - 1;

				FAuxData AuxData = {};
				AuxData.Data = AuxHeader->Data;
				AuxData.DataSize = (AuxHeader->Pack >> FAuxHeader::SizeShift);
				AuxData.FieldIndex = AuxHeader->FieldIndex_Size & FAuxHeader::FieldMask;
				// AuxData.FieldSizeAndType = ... - this is assigned on demand in GetData()
				AuxCollector.Add(AuxData);
			}
		}

		Bridge.OnEvent(EventDataInfo);

		AuxCollector.Reset();
	}

	return 0;
}


// {{{1 protocol-6 -------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FProtocol6Stage : public FProtocol5Stage
{
public:
							FProtocol6Stage(FTransport* InTransport);
protected:	
	using EKnownUids		= Protocol6::EKnownEventUids;
};

////////////////////////////////////////////////////////////////////////////////
FProtocol6Stage::FProtocol6Stage(FTransport* InTransport)
	: FProtocol5Stage(InTransport)
{
	EventVersion = 6;
}

// {{{1 est.-transport ---------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FEstablishTransportStage
	: public FAnalysisMachine::FStage
{
public:
	virtual EStatus OnData(FStreamReader& Reader, const FMachineContext& Context) override;
};

////////////////////////////////////////////////////////////////////////////////
FEstablishTransportStage::EStatus FEstablishTransportStage::OnData(
	FStreamReader& Reader,
	const FMachineContext& Context)
{
	using namespace UE::Trace;

	const struct {
		uint8 TransportVersion;
		uint8 ProtocolVersion;
	}* Header = decltype(Header)(Reader.GetPointer(sizeof(*Header)));
	if (Header == nullptr)
	{
		return EStatus::NotEnoughData;
	}

	FTransport* Transport = nullptr;
	switch (Header->TransportVersion)
	{
	case ETransport::Raw:			Transport = new FTransport(); break;
	case ETransport::Packet:		Transport = new FPacketTransport(); break;
	case ETransport::TidPacket:		Transport = new FTidPacketTransport(); break;
	case ETransport::TidPacketSync:	Transport = new FTidPacketTransportSync(); break;
	default:						return EStatus::Error;
	}

	uint32 ProtocolVersion = Header->ProtocolVersion;
	switch (ProtocolVersion)
	{
	case Protocol0::EProtocol::Id:
		Context.Machine.QueueStage<FProtocol0Stage>(Transport);
		Context.Machine.Transition();
		break;

	case Protocol1::EProtocol::Id:
	case Protocol2::EProtocol::Id:
	case Protocol3::EProtocol::Id:
		Context.Machine.QueueStage<FProtocol2Stage>(ProtocolVersion, Transport);
		Context.Machine.Transition();
		break;

	case Protocol4::EProtocol::Id:
		Context.Machine.QueueStage<FProtocol4Stage>(ProtocolVersion, Transport);
		Context.Machine.Transition();
		break;

	case Protocol5::EProtocol::Id:
		Context.Machine.QueueStage<FProtocol5Stage>(Transport);
		Context.Machine.Transition();
		break;

	case Protocol6::EProtocol::Id:
		Context.Machine.QueueStage<FProtocol6Stage>(Transport);
		Context.Machine.Transition();
		break;

	default:
		return EStatus::Error;
	}

	Reader.Advance(sizeof(*Header));
	return EStatus::Continue;
}



// {{{1 metadata ---------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FMetadataStage
	: public FAnalysisMachine::FStage
{
public:
	virtual EStatus OnData(FStreamReader& Reader, const FMachineContext& Context) override;
};

////////////////////////////////////////////////////////////////////////////////
FMetadataStage::EStatus FMetadataStage::OnData(
	FStreamReader& Reader,
	const FMachineContext& Context)
{
	const auto* MetadataSize = Reader.GetPointer<uint16>();
	if (MetadataSize == nullptr)
	{
		return EStatus::NotEnoughData;
	}

	const uint8* Metadata = Reader.GetPointer(sizeof(*MetadataSize) + *MetadataSize);
	if (Metadata == nullptr)
	{
		return EStatus::NotEnoughData;
	}

	Reader.Advance(sizeof(*MetadataSize) + *MetadataSize);

	Context.Machine.QueueStage<FEstablishTransportStage>();
	Context.Machine.Transition();
	return EStatus::Continue;
}



// {{{1 magic ------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FMagicStage
	: public FAnalysisMachine::FStage
{
public:
	virtual EStatus OnData(FStreamReader& Reader, const FMachineContext& Context) override;
};

////////////////////////////////////////////////////////////////////////////////
FMagicStage::EStatus FMagicStage::OnData(
	FStreamReader& Reader,
	const FMachineContext& Context)
{
	const auto* MagicPtr = Reader.GetPointer<uint32>();
	if (MagicPtr == nullptr)
	{
		return EStatus::NotEnoughData;
	}

	uint32 Magic = *MagicPtr;

	if (Magic == 'ECRT' || Magic == '2CRT')
	{
		// Source is big-endian which we don't currently support
		return EStatus::Error;
	}

	if (Magic == 'TRCE')
	{
		Reader.Advance(sizeof(*MagicPtr));
		Context.Machine.QueueStage<FEstablishTransportStage>();
		Context.Machine.Transition();
		return EStatus::Continue;
	}

	if (Magic == 'TRC2')
	{
		Reader.Advance(sizeof(*MagicPtr));
		Context.Machine.QueueStage<FMetadataStage>();
		Context.Machine.Transition();
		return EStatus::Continue;
	}

	// There was no header on early traces so they went straight into declaring
	// protocol and transport versions.
	if (Magic == 0x00'00'00'01) // protocol 0, transport 1
	{
		Context.Machine.QueueStage<FEstablishTransportStage>();
		Context.Machine.Transition();
		return EStatus::Continue;
	}

	return EStatus::Error;
}



// {{{1 engine -----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
class FAnalysisEngine::FImpl
{
public:
						FImpl(TArray<IAnalyzer*>&& Analyzers);
	void				Begin();
	void				End();
	bool				OnData(FStreamReader& Reader);
	FAnalysisBridge		Bridge;
	FAnalysisMachine	Machine = Bridge;
};

////////////////////////////////////////////////////////////////////////////////
FAnalysisEngine::FImpl::FImpl(TArray<IAnalyzer*>&& Analyzers)
: Bridge(Forward<TArray<IAnalyzer*>>(Analyzers))
{
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisEngine::FImpl::Begin()
{
	Machine.QueueStage<FMagicStage>();
	Machine.Transition();
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisEngine::FImpl::End()
{
	Machine.Transition();
	Bridge.Reset();
}

////////////////////////////////////////////////////////////////////////////////
bool FAnalysisEngine::FImpl::OnData(FStreamReader& Reader)
{
	bool bRet = (Machine.OnData(Reader) != FAnalysisMachine::EStatus::Error);
	bRet &= Bridge.IsStillAnalyzing();
	return bRet;
}



////////////////////////////////////////////////////////////////////////////////
FAnalysisEngine::FAnalysisEngine(TArray<IAnalyzer*>&& Analyzers)
: Impl(new FImpl(Forward<TArray<IAnalyzer*>>(Analyzers)))
{
}

////////////////////////////////////////////////////////////////////////////////
FAnalysisEngine::~FAnalysisEngine()
{
	delete Impl;
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisEngine::Begin()
{
	Impl->Begin();
}

////////////////////////////////////////////////////////////////////////////////
void FAnalysisEngine::End()
{
	Impl->End();
}

////////////////////////////////////////////////////////////////////////////////
bool FAnalysisEngine::OnData(FStreamReader& Reader)
{
	return Impl->OnData(Reader);
}

  // }}}
} // namespace Trace
} // namespace UE

/* vim: set foldlevel=1 : */
