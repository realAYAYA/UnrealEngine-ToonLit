// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/DataStream/DataStreamManager.h"
#include "Iris/DataStream/DataStreamDefinitions.h"
#include "Iris/Core/IrisLog.h"
#include "Net/Core/Misc/ResizableCircularQueue.h"
#include "Iris/PacketControl/PacketNotification.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/ReplicationSystem/NetExports.h"
#include "UObject/Package.h"

class UDataStreamManager::FImpl
{
public:
	FImpl();
	~FImpl();

	void Init(const FDataStreamManagerInitParams& InitParams);
	void Deinit();

	EWriteResult BeginWrite(const UDataStream::FBeginWriteParameters& Params);
	UDataStream::EWriteResult WriteData(UE::Net::FNetSerializationContext& context, FDataStreamRecord const*& OutRecord);
	void EndWrite();
	void ReadData(UE::Net::FNetSerializationContext& context);
	void ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus Status, FDataStreamRecord const* Record);

	ECreateDataStreamResult CreateStream(const FName StreamName);

	const UDataStream* GetStream(const FName StreamName) const;
	UDataStream* GetStream(const FName StreamName);

	void SetSendStatus(const FName StreamName, EDataStreamSendStatus Status);
	EDataStreamSendStatus GetSendStatus(const FName StreamName) const;

	UE::Net::Private::FNetExports& GetNetExports();

	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	struct FindStreamByName
	{
		inline FindStreamByName(FName InName) : Name(InName) {}
		
		inline bool operator()(const UDataStream* Stream) const { return Name.IsEqual(Stream->GetFName(), ENameCase::IgnoreCase, false); }
		inline bool operator()(const FDataStreamDefinition& Definition) const { return Name == Definition.DataStreamName; }

	private:
		FName Name;
	};

	struct FRecord : public FDataStreamRecord
	{
		TArray<const FDataStreamRecord*, TInlineAllocator<8>> DataStreamRecords;
		uint32 DataStreamMask;
	};

	void InitRecordStorage();
	void InitStreams();

private:
	static constexpr uint32 MaxStreamCount = 32U;
	static constexpr uint32 StreamCountBitCount = 5U; // Enough for 32 streams

	UE::Net::Private::FNetExports NetExports;

	// We can afford reserving space for a few pointers It's unlikely we will create anything close to 16 streams.
	TArray<TObjectPtr<UDataStream>> Streams;
	TArray<EDataStreamSendStatus> StreamSendStatus;
	TArray<FRecord> RecordStorage;
	TResizableCircularQueue<FRecord*> Records;
	uint32 PacketWindowSize = 0;
};

UDataStreamManager::UDataStreamManager()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Impl = MakePimpl<FImpl>();
	}
}

UDataStreamManager::~UDataStreamManager()
{
}

void UDataStreamManager::Init(const FDataStreamManagerInitParams& InitParams)
{
	Impl->Init(InitParams);
}

void UDataStreamManager::Deinit()
{
	Impl->Deinit();
}

UDataStream::EWriteResult UDataStreamManager::BeginWrite(const UDataStream::FBeginWriteParameters& Params)
{
	return Impl->BeginWrite(Params);
}

void UDataStreamManager::EndWrite()
{
	Impl->EndWrite();
}

UDataStream::EWriteResult UDataStreamManager::WriteData(UE::Net::FNetSerializationContext& Context, FDataStreamRecord const*& OutRecord)
{
	return Impl->WriteData(Context, OutRecord);
}

void UDataStreamManager::ReadData(UE::Net::FNetSerializationContext& Context)
{
	return Impl->ReadData(Context);
}

void UDataStreamManager::ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus Status, FDataStreamRecord const* Record)
{
	return Impl->ProcessPacketDeliveryStatus(Status, Record);
}

ECreateDataStreamResult UDataStreamManager::CreateStream(const FName StreamName)
{
	return Impl->CreateStream(StreamName);
}

UDataStream* UDataStreamManager::GetStream(const FName StreamName)
{
	return Impl->GetStream(StreamName);
}

void UDataStreamManager::SetSendStatus(const FName StreamName, EDataStreamSendStatus Status)
{
	return Impl->SetSendStatus(StreamName, Status);
}

EDataStreamSendStatus UDataStreamManager::GetSendStatus(const FName StreamName) const
{
	return Impl->GetSendStatus(StreamName);
}

UE::Net::Private::FNetExports& UDataStreamManager::GetNetExports()
{
	return Impl->GetNetExports();
}

void UDataStreamManager::AddReferencedObjects(UObject* Object, FReferenceCollector& Collector)
{
	UDataStreamManager* StreamManager = CastChecked<UDataStreamManager>(Object);
	if (FImpl* Impl = StreamManager->Impl.Get())
	{
		Impl->AddReferencedObjects(Collector);
	}
}

// FImpl
UDataStreamManager::FImpl::FImpl()
{
}

UDataStreamManager::FImpl::~FImpl()
{
}

void UDataStreamManager::FImpl::Init(const FDataStreamManagerInitParams& InitParams)
{
	PacketWindowSize = InitParams.PacketWindowSize;
	InitRecordStorage();
	InitStreams();
}

void UDataStreamManager::FImpl::Deinit()
{
	// Discard all records
	for (SIZE_T RecordIt = 0, RecordEndIt = Records.Count(); RecordIt != RecordEndIt; ++RecordIt)
	{
		const FDataStreamRecord* const Record = Records.Peek();
		ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus::Discard, Record);
	}

	for (UDataStream* Stream : Streams)
	{
		if (IsValid(Stream))
		{
			Stream->MarkAsGarbage();
		}
	}

	Streams.Reset();
	StreamSendStatus.Reset();
}

UDataStreamManager::EWriteResult UDataStreamManager::FImpl::BeginWrite(const UDataStream::FBeginWriteParameters& Params)
{
	const SIZE_T StreamCount = Streams.Num();
	if (StreamCount == 0)
	{
		return EWriteResult::NoData;
	}

	TObjectPtr<UDataStream>* StreamData = Streams.GetData();
	const EDataStreamSendStatus* SendStatusData = StreamSendStatus.GetData();
	UDataStream::EWriteResult CombinedWriteResult = UDataStream::EWriteResult::NoData;

	for (SIZE_T StreamIt = 0, StreamEndIt = StreamCount; StreamIt != StreamEndIt; ++StreamIt)
	{
		UDataStream* Stream = StreamData[StreamIt];
		const EDataStreamSendStatus SendStatus = SendStatusData[StreamIt];
		if (SendStatus == EDataStreamSendStatus::Pause)
		{
			continue;
		}

		const EWriteResult WriteResult = Stream->BeginWrite(Params);
		CombinedWriteResult = (CombinedWriteResult == EWriteResult::HasMoreData || WriteResult == EWriteResult::HasMoreData) ? EWriteResult::HasMoreData : WriteResult;
	}

	return CombinedWriteResult;
}

void UDataStreamManager::FImpl::EndWrite()
{
	const SIZE_T StreamCount = Streams.Num();

	TObjectPtr<UDataStream>* StreamData = Streams.GetData();
	const EDataStreamSendStatus* SendStatusData = StreamSendStatus.GetData();

	for (SIZE_T StreamIt = 0, StreamEndIt = StreamCount; StreamIt != StreamEndIt; ++StreamIt)
	{
		UDataStream* Stream = StreamData[StreamIt];
		const EDataStreamSendStatus SendStatus = SendStatusData[StreamIt];
		if (SendStatus == EDataStreamSendStatus::Pause)
		{
			continue;
		}

		Stream->EndWrite();
	}
}

UDataStreamManager::EWriteResult UDataStreamManager::FImpl::WriteData(UE::Net::FNetSerializationContext& Context, FDataStreamRecord const*& OutRecord)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	const int32 StreamCount = Streams.Num();
	if (StreamCount <= 0)
	{
		return EWriteResult::NoData;
	}

	// Is the packet window full? Unexpected.
	if (Records.Count() == Records.AllocatedCapacity())
	{
		ensureMsgf(false, TEXT("DataStreamManager record storage is full."));
		return EWriteResult::NoData;
	}

	// Init export record
	NetExports.InitExportRecordForPacket();

	// Setup export context for this packet
	FNetExportContext::FBatchExports CurrentPacketBatchExports;
	FNetExports::FExportScope ExportScope = NetExports.MakeExportScope(Context, CurrentPacketBatchExports);

	FRecord TempRecord;
	TempRecord.DataStreamRecords.SetNumZeroed(StreamCount);
	FDataStreamRecord const** TempStreamRecords = TempRecord.DataStreamRecords.GetData();

	FNetBitStreamWriter ManagerStream = Context.GetBitStreamWriter()->CreateSubstream();
	ManagerStream.WriteBits(0U, StreamCountBitCount);
	ManagerStream.WriteBits(0U, StreamCount);
	// If we can't fit our header we can't fit anything else either.
	if (ManagerStream.IsOverflown())
	{
		Context.GetBitStreamWriter()->DiscardSubstream(ManagerStream);
		return EWriteResult::NoData;
	}

	TObjectPtr<UDataStream>* StreamData = Streams.GetData();
	const EDataStreamSendStatus* SendStatusData = StreamSendStatus.GetData();
	UDataStream::EWriteResult CombinedWriteResult = UDataStream::EWriteResult::NoData;

	uint32 DataStreamMask = 0;
	uint32 CurrentStreamMask = 1;
	for (SIZE_T StreamIt = 0, StreamEndIt = StreamCount; StreamIt != StreamEndIt; ++StreamIt, CurrentStreamMask += CurrentStreamMask)
	{
		UDataStream* Stream = StreamData[StreamIt];
		const EDataStreamSendStatus SendStatus = SendStatusData[StreamIt];
		if (SendStatus == EDataStreamSendStatus::Pause)
		{
			continue;
		}

		FNetBitStreamWriter SubBitStream = ManagerStream.CreateSubstream();
		FNetSerializationContext SubContext = Context.MakeSubContext(&SubBitStream);

		const FDataStreamRecord* SubRecord = nullptr;
		const EWriteResult WriteResult = Stream->WriteData(SubContext, SubRecord);
		if (WriteResult == EWriteResult::NoData || SubContext.HasError())
		{
			checkf(SubRecord == nullptr, TEXT("DataStream '%s' provided a record despite errors or returning NoData."), ToCStr(Stream->GetFName().GetPlainNameString()));
			ManagerStream.DiscardSubstream(SubBitStream);

			if (SubContext.HasError())
			{
				Context.SetError(SubContext.GetError(), false);
				break;
			}
			else
			{
				continue;
			}
		}

		DataStreamMask |= CurrentStreamMask;

		ManagerStream.CommitSubstream(SubBitStream);
		TempStreamRecords[StreamIt] = SubRecord;
		// Set CombinedWriteResult to HasMoreData if any of the result variables is HasMoreData, otherwise take the WriteResult, which will be 'Ok'.
		CombinedWriteResult = (CombinedWriteResult == EWriteResult::HasMoreData || WriteResult == EWriteResult::HasMoreData) ? EWriteResult::HasMoreData : WriteResult;
	}

	if (CombinedWriteResult == EWriteResult::NoData)
	{
		Context.GetBitStreamWriter()->DiscardSubstream(ManagerStream);
	}
	else
	{
		// Fixup manager header
		{
			const uint32 CurrentBitPos = ManagerStream.GetPosBits();
			ManagerStream.Seek(0);
			ManagerStream.WriteBits(StreamCount - 1U, StreamCountBitCount);
			ManagerStream.WriteBits(DataStreamMask, StreamCount);
			ManagerStream.Seek(CurrentBitPos);
			Context.GetBitStreamWriter()->CommitSubstream(ManagerStream);
		}

		// Fixup and store record
		TempRecord.DataStreamMask = DataStreamMask;

		FRecord*& Record = Records.Enqueue_GetRef();
		*Record = MoveTemp(TempRecord);

		OutRecord = Record;

		// Push exports and update export record
		NetExports.CommitExportsToRecord(ExportScope);
		NetExports.PushExportRecordForPacket();
	}

	return CombinedWriteResult;
}

void UDataStreamManager::FImpl::ReadData(UE::Net::FNetSerializationContext& Context)
{
	using namespace UE::Net;

	FNetBitStreamReader* Stream = Context.GetBitStreamReader();
	const uint32 StreamCount = 1U + Stream->ReadBits(StreamCountBitCount);
	const uint32 DataStreamMask = Stream->ReadBits(StreamCount);
	// Validate the received information
	if (StreamCount > uint32(Streams.Num()) || DataStreamMask == 0U)
	{
		Context.SetError(GNetError_BitStreamError);
		return;
	}

	TObjectPtr<UDataStream>* StreamData = Streams.GetData();
	for (uint32 StreamIt = 0, StreamEndIt = StreamCount, Mask = 1U; StreamIt != StreamEndIt; ++StreamIt, Mask += Mask)
	{
		if (DataStreamMask & Mask)
		{
			UDataStream* DataStream = StreamData[StreamIt];
			DataStream->ReadData(Context);
			// If something went wrong we should stop deserializing immediately.
			if (Context.HasErrorOrOverflow())
			{
				break;
			}
		}
	}
}

void UDataStreamManager::FImpl::ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus Status, FDataStreamRecord const* InRecord)
{
	const FRecord* Record = Records.Peek();
	check(Record == InRecord);

	// Process delivery notifications for our NetExports
	NetExports.ProcessPacketDeliveryStatus(Status);

	// Forward the call to each DataStream that was included in the record.
	const uint32 DataStreamMask = Record->DataStreamMask;
	const uint32 StreamCount = Streams.Num();

	TObjectPtr<UDataStream>* StreamData = Streams.GetData();
	for (uint32 StreamIt = 0, StreamEndIt = StreamCount, Mask = 1U; StreamIt != StreamEndIt; ++StreamIt, Mask += Mask)
	{
		if (DataStreamMask & Mask)
		{
			UDataStream* DataStream = StreamData[StreamIt];
			DataStream->ProcessPacketDeliveryStatus(Status, Record->DataStreamRecords[StreamIt]);
		}
	}

	Records.Pop();
}

ECreateDataStreamResult UDataStreamManager::FImpl::CreateStream(const FName StreamName)
{
	// Bumping MaxStreamCount may require modifying the FRecord and WriteData/ReadData.
	if (Streams.Num() >= MaxStreamCount)
	{
		return ECreateDataStreamResult::Error_TooManyStreams;
	}

	if (Streams.ContainsByPredicate(FindStreamByName(StreamName)))
	{
		UE_LOG(LogIris, Warning, TEXT("A DataStream with name '%s' already exists."), ToCStr(StreamName.GetPlainNameString()));
		return ECreateDataStreamResult::Error_Duplicate;
	}

	const UDataStreamDefinitions* StreamDefinitions = GetDefault<UDataStreamDefinitions>();
	if (const FDataStreamDefinition* Definition = StreamDefinitions->FindDefinition(StreamName))
	{
		if (Definition->Class == nullptr)
		{
			return ECreateDataStreamResult::Error_InvalidDefinition;
		}

		UDataStream* Stream = NewObject<UDataStream>(GetTransientPackage(), ToRawPtr(Definition->Class), MakeUniqueObjectName(nullptr, Definition->Class, StreamName));

		Streams.Add(Stream);
		StreamSendStatus.Add(Definition->DefaultSendStatus);
		return ECreateDataStreamResult::Success;
	}

	return ECreateDataStreamResult::Error_MissingDefinition;
}

inline const UDataStream* UDataStreamManager::FImpl::GetStream(const FName StreamName) const
{
	const TObjectPtr<UDataStream>* Stream = Streams.FindByPredicate(FindStreamByName(StreamName));
	return Stream != nullptr ? *Stream : nullptr;
}

inline UDataStream* UDataStreamManager::FImpl::GetStream(const FName StreamName)
{
	TObjectPtr<UDataStream>* Stream = Streams.FindByPredicate(FindStreamByName(StreamName));
	return Stream != nullptr ? *Stream : nullptr;
}

void UDataStreamManager::FImpl::SetSendStatus(const FName StreamName, EDataStreamSendStatus Status)
{
	const int32 Index = Streams.IndexOfByPredicate(FindStreamByName(StreamName));
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogIris, Display, TEXT("Cannot set send status for DataStream '%s' that hasn't been created."), ToCStr(StreamName.GetPlainNameString()));
		return;
	}

	StreamSendStatus[Index] = Status;
}

EDataStreamSendStatus UDataStreamManager::FImpl::GetSendStatus(const FName StreamName) const
{
	const int32 Index = Streams.IndexOfByPredicate(FindStreamByName(StreamName));
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogIris, Display, TEXT("Cannot retrieve send status for DataStream '%s' that hasn't been created. Returnning Pause."), ToCStr(StreamName.GetPlainNameString()));
		return EDataStreamSendStatus::Pause;
	}

	return StreamSendStatus[Index];
}

void UDataStreamManager::FImpl::InitRecordStorage()
{
	RecordStorage.SetNum(PacketWindowSize);

	Records = TResizableCircularQueue<FRecord*>(PacketWindowSize);
	for (uint32 It = 0, EndIt = PacketWindowSize; It != EndIt; ++It)
	{
		FRecord*& Record = Records.Enqueue();
		Record = &RecordStorage[It];
	}

	// Note: The circular queue will not modify the contents of its storage for POD types.
	Records.Reset();
}

void UDataStreamManager::FImpl::InitStreams()
{
	UDataStreamDefinitions* StreamDefinitions = GetMutableDefault<UDataStreamDefinitions>();
	StreamDefinitions->FixupDefinitions();

	TArray<FName> AutoCreateStreams;
	AutoCreateStreams.Reserve(MaxStreamCount);
	StreamDefinitions->GetAutoCreateStreamNames(AutoCreateStreams);
	for (const FName& StreamName : AutoCreateStreams)
	{
		CreateStream(StreamName);
	}
}

UE::Net::Private::FNetExports& UDataStreamManager::FImpl::GetNetExports()
{
	return NetExports;
}

void UDataStreamManager::FImpl::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(Streams);
}
