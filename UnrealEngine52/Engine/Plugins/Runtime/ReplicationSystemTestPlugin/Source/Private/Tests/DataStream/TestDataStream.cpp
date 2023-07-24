// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "MockDataStream.h"
#include "Iris/DataStream/DataStreamManager.h"
#include "Iris/DataStream/DataStreamDefinitions.h"
#include "Iris/DataStream/DataStreamManager.h"
#include "Iris/PacketControl/PacketNotification.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"

namespace UE::Net::Private
{

// These test cannot run in parallel with other code accessing data streams, like DataStreamManager and DataStreamDefinitions.
class FTestDataStream : public FNetworkAutomationTestSuiteFixture
{
public:
	FTestDataStream();

	virtual void SetUp() override;
	virtual void TearDown() override;

private:
	class UMyDataStreamDefinitions : public UDataStreamDefinitions
	{
	public:
		void FixupDefinitions() { return UDataStreamDefinitions::FixupDefinitions(); }
	};

	void StoreDataStreamDefinitions();
	void RestoreDataStreamDefinitions();
	void CreateMockDataStreamDefinition(FDataStreamDefinition& Definition, bool bValid);

protected:
	void AddMockDataStreamDefinition(bool bValid = true);
	UMockDataStream* CreateMockStream(const UMockDataStream::FFunctionCallSetup* Setup = nullptr);
	FNetSerializationContext& CreateDataStreamContext();
	void InitBitStreamReaderFromWriter();

	UDataStreamManager* DataStreamManager;
	UMyDataStreamDefinitions* DataStreamDefinitions;
	TArray<FDataStreamDefinition>* CurrentDataStreamDefinitions;
	TArray<FDataStreamDefinition> PreviousDataStreamDefinitions;
	bool* PointerToFixupComplete;

	//
	FNetSerializationContext DataStreamContext;
	FNetBitStreamReader BitStreamReader;
	FNetBitStreamWriter BitStreamWriter;
	alignas(16) uint8 BitStreamStorage[128];
};

FTestMessage& operator<<(FTestMessage& Ar, const UDataStream::EWriteResult WriteResult);

//

UE_NET_TEST_FIXTURE(FTestDataStream, CanCreateDataStream)
{
	constexpr bool bAddValidDefinition = true;
	AddMockDataStreamDefinition(bAddValidDefinition);

	ECreateDataStreamResult Result = DataStreamManager->CreateStream("Mock");
	UE_NET_ASSERT_EQ(unsigned(Result), unsigned(ECreateDataStreamResult::Success));
}

UE_NET_TEST_FIXTURE(FTestDataStream, CannotCreateSameDataStreamTwice)
{
	constexpr bool bAddValidDefinition = true;
	AddMockDataStreamDefinition(bAddValidDefinition);

	DataStreamManager->CreateStream("Mock");
	ECreateDataStreamResult Result = DataStreamManager->CreateStream("Mock");
	UE_NET_ASSERT_EQ(unsigned(Result), unsigned(ECreateDataStreamResult::Error_Duplicate));
}

UE_NET_TEST_FIXTURE(FTestDataStream, CannotCreateInvalidDataStream)
{
	constexpr bool bAddValidDefinition = false;
	AddMockDataStreamDefinition(bAddValidDefinition);

	ECreateDataStreamResult Result = DataStreamManager->CreateStream("Mock");
	UE_NET_ASSERT_EQ(unsigned(Result), unsigned(ECreateDataStreamResult::Error_InvalidDefinition));
}

UE_NET_TEST_FIXTURE(FTestDataStream, DataStreamGetsWriteDataCall)
{
	UMockDataStream::FFunctionCallSetup MockSetup = {};
	MockSetup.WriteDataBitCount = 0;
	MockSetup.WriteDataReturnValue = UDataStream::EWriteResult::NoData;

	UMockDataStream* Mock = CreateMockStream(&MockSetup);

	FNetSerializationContext& Context = CreateDataStreamContext();
	const FDataStreamRecord* Record = nullptr;
	UDataStream::EWriteResult Result = DataStreamManager->WriteData(Context, Record);

	// Make sure WriteData was called.
	{
		const UMockDataStream::FFunctionCallStatus& CallStatus = Mock->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(CallStatus.WriteDataCallCount, 1U);
	}

	// Even though our data stream isn't writing any data doesn't prevent the manager itself from doing so.
	if (Result != UDataStream::EWriteResult::NoData)
	{
		DataStreamManager->ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Discard, Record);

		// Our stream didn't write anything so should not be called.
		const UMockDataStream::FFunctionCallStatus& CallStatus = Mock->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(CallStatus.ProcessPacketDeliveryStatusCallCount, 0U);
	}
}

UE_NET_TEST_FIXTURE(FTestDataStream, DataStreamGetsProcessPacketDeliveryStatusCall)
{
	UMockDataStream* Mock = CreateMockStream();

	// Make sure the right records are supplied in the PacketDeliveryStatusCall as well

	const uint32 MagicValues[] = {0x35373931U, 0x32312D, 0x36312D};
	constexpr uint32 MagicValueCount = UE_ARRAY_COUNT(MagicValues);
	const FDataStreamRecord* Records[MagicValueCount] = {};
	for (SIZE_T It = 0, EndIt = MagicValueCount; It != EndIt; ++It)
	{
		FNetSerializationContext& Context = CreateDataStreamContext();

		UMockDataStream::FFunctionCallSetup MockSetup = {};
		MockSetup.WriteDataBitCount = 3;
		MockSetup.WriteDataReturnValue = UDataStream::EWriteResult::Ok;
		MockSetup.WriteDataRecordMagicValue = MagicValues[It];
		Mock->SetFunctionCallSetup(MockSetup);
		UDataStream::EWriteResult Result = DataStreamManager->WriteData(Context, Records[It]);
	}

	// Make sure WriteData was called.
	{
		const UMockDataStream::FFunctionCallStatus& CallStatus = Mock->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(CallStatus.WriteDataCallCount, MagicValueCount);
	}

	for (SIZE_T It = 0, EndIt = MagicValueCount; It != EndIt; ++It)
	{
		DataStreamManager->ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Discard, Records[It]);
		const UMockDataStream::FFunctionCallStatus& CallStatus = Mock->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(CallStatus.ProcessPacketDeliveryStatusCallCount, uint32(It + 1));
		UE_NET_ASSERT_EQ(CallStatus.ProcessPacketDeliveryStatusMagicValue, MagicValues[It]);
	}
}

UE_NET_TEST_FIXTURE(FTestDataStream, DataStreamGetsReadDataCall)
{
	UMockDataStream::FFunctionCallSetup MockSetup = {};
	MockSetup.WriteDataBitCount = 15;
	MockSetup.WriteDataReturnValue = UDataStream::EWriteResult::Ok;
	MockSetup.ReadDataBitCount = MockSetup.WriteDataBitCount;

	UMockDataStream* Mock = CreateMockStream(&MockSetup);

	FNetSerializationContext& Context = CreateDataStreamContext();
	const FDataStreamRecord* Record = nullptr;
	DataStreamManager->WriteData(Context, Record);

	// Make sure ReadData was called and all bits have been read.
	{
		const uint32 WriterBitStreamPos = Context.GetBitStreamWriter()->GetPosBits();
		InitBitStreamReaderFromWriter();
		DataStreamManager->ReadData(Context);

		const UMockDataStream::FFunctionCallStatus& CallStatus = Mock->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(CallStatus.ReadDataCallCount, 1U);

		UE_NET_ASSERT_FALSE(Context.HasErrorOrOverflow());

		const uint32 ReaderBitStreamPos = Context.GetBitStreamReader()->GetPosBits();
		UE_NET_ASSERT_EQ(WriterBitStreamPos, ReaderBitStreamPos);
	}

	// Cleanup
	DataStreamManager->ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Discard, Record);
}

// FTestDataStream implementation
FTestDataStream::FTestDataStream()
: DataStreamManager(nullptr)
, DataStreamDefinitions(nullptr)
, CurrentDataStreamDefinitions(nullptr)
, DataStreamContext(&BitStreamReader, &BitStreamWriter)
{
}

void FTestDataStream::SetUp()
{
	FDataStreamManagerInitParams InitParams;
	InitParams.PacketWindowSize = 256;
	DataStreamManager = NewObject<UDataStreamManager>();
	DataStreamManager->Init(InitParams);
	StoreDataStreamDefinitions();
}

void FTestDataStream::TearDown()
{
	RestoreDataStreamDefinitions();
	DataStreamManager->MarkAsGarbage();
	DataStreamManager = nullptr;
}

void FTestDataStream::StoreDataStreamDefinitions()
{
	DataStreamDefinitions = static_cast<UMyDataStreamDefinitions*>(GetMutableDefault<UDataStreamDefinitions>());
	check(DataStreamDefinitions != nullptr);
	CurrentDataStreamDefinitions = &DataStreamDefinitions->ReadWriteDataStreamDefinitions();
	PointerToFixupComplete = &DataStreamDefinitions->ReadWriteFixupComplete();

	PreviousDataStreamDefinitions.Empty();
	Swap(*CurrentDataStreamDefinitions, PreviousDataStreamDefinitions);
	*PointerToFixupComplete = false;
}

void FTestDataStream::RestoreDataStreamDefinitions()
{
	Swap(*CurrentDataStreamDefinitions, PreviousDataStreamDefinitions);
	*PointerToFixupComplete = false;
}

void FTestDataStream::CreateMockDataStreamDefinition(FDataStreamDefinition& Definition, bool bValid)
{
	Definition.DataStreamName = FName("Mock");
	Definition.ClassName = bValid ? FName("/Script/ReplicationSystemTestPlugin.MockDataStream") : FName();
	Definition.Class = nullptr;
	Definition.DefaultSendStatus = EDataStreamSendStatus::Send;
	Definition.bAutoCreate = false;
}

void FTestDataStream::AddMockDataStreamDefinition(bool bValid)
{
	FDataStreamDefinition Definition = {};
	CreateMockDataStreamDefinition(Definition, bValid);
	CurrentDataStreamDefinitions->Add(Definition);
	DataStreamDefinitions->FixupDefinitions();
}

UMockDataStream* FTestDataStream::CreateMockStream(const UMockDataStream::FFunctionCallSetup* Setup)
{
	AddMockDataStreamDefinition(true);
	DataStreamManager->CreateStream("Mock");
	UMockDataStream* Stream = StaticCast<UMockDataStream*>(DataStreamManager->GetStream("Mock"));
	if (Stream != nullptr && Setup != nullptr)
	{
		Stream->SetFunctionCallSetup(*Setup);
	}

	return Stream;
}

FNetSerializationContext& FTestDataStream::CreateDataStreamContext()
{
	BitStreamWriter.InitBytes(BitStreamStorage, sizeof(BitStreamStorage));
	// Reset reader
	BitStreamReader.InitBits(BitStreamStorage, 0U);

	return DataStreamContext;
}

void FTestDataStream::InitBitStreamReaderFromWriter()
{
	BitStreamWriter.CommitWrites();
	BitStreamReader.InitBits(BitStreamStorage, BitStreamWriter.GetPosBits());
}

FTestMessage& operator<<(FTestMessage& TestMessage, const UDataStream::EWriteResult WriteResult)
{
	switch (WriteResult)
	{
	case UDataStream::EWriteResult::NoData:
		return TestMessage << TEXT("");
	case UDataStream::EWriteResult::Ok:
		return TestMessage << TEXT("Ok");
	case UDataStream::EWriteResult::HasMoreData:
		return TestMessage << TEXT("HasMoreData");
	default:
		check(false);
		return TestMessage << TEXT("");
	}
}

}
