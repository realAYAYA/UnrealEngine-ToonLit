// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Net/NetPacketNotify.h"
#include "Logging/LogScopedVerbosityOverride.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNetPacketNotifyTest, "Net.PacketNotifyTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

struct FNetPacketNotifyTestUtil
{
	FNetPacketNotify DefaultNotify;
	FNetPacketNotifyTestUtil()
	{
		DefaultNotify.Init(FNetPacketNotify::SequenceNumberT(-1),  FNetPacketNotify::SequenceNumberT(0));
	}

	// Helper to fill in SequenceHistory with expected result
	template <typename T>
	static void InitHistory(FNetPacketNotify::SequenceHistoryT& History, const T& DataToSet)
	{
		const SIZE_T Count = sizeof(T) / sizeof(DataToSet[0]);
		static_assert(Count < FNetPacketNotify::SequenceHistoryT::WordCount, "DataToSet must be smaller than HistoryBuffer");

		for (SIZE_T It=0; It < Count; ++It)
		{
			History.Data()[It] = DataToSet[It];
		}
	}

	// Pretend to receive and acknowledge incoming packet to generate ackdata
	static int32 PretendReceiveSeq(FNetPacketNotify& PacketNotify, FNetPacketNotify::SequenceNumberT Seq, bool Ack = true)
	{
		FNetPacketNotify::FNotificationHeader Data;
		Data.Seq = Seq;
		Data.AckedSeq = PacketNotify.GetOutAckSeq();
		Data.History = FNetPacketNotify::SequenceHistoryT(0);
		Data.HistoryWordCount = 1;
		
		FNetPacketNotify::SequenceNumberT::DifferenceT SeqDelta = PacketNotify.Update(Data, [](FNetPacketNotify::SequenceNumberT AckedSequence, bool delivered) {});
		if (SeqDelta > 0)
		{
			if (Ack)
				PacketNotify.AckSeq(Seq);
		}

		return SeqDelta;
	}

	// Pretend to send packet
	static void PretendSendSeq(FNetPacketNotify& PacketNotify, FNetPacketNotify::SequenceNumberT LastAckSeq)
	{
		// set last InAcqSeq that we know that the remote end knows that we know (AckAck)
		PacketNotify.WrittenHistoryWordCount = 1;
		PacketNotify.WrittenInAckSeq = LastAckSeq;

		// Store data
		PacketNotify.CommitAndIncrementOutSeq();
	}

	// pretend to ack array of sequence numbers
	template<typename T>
	static void PretendAckSequenceNumbers(FNetPacketNotify& PacketNotify, const T& InSequenceNumbers)
	{
		SIZE_T SequenceNumberCount = sizeof(InSequenceNumbers) / sizeof(InSequenceNumbers[0]);

		for (SIZE_T I=0; I<SequenceNumberCount; ++I)
		{
			FNetPacketNotifyTestUtil::PretendReceiveSeq(PacketNotify, InSequenceNumbers[I]);
		}
	}
	
	// Pretend that we received a packet
	template<typename T>
	static SIZE_T PretendReceivedPacket(FNetPacketNotify& PacketNotify, const FNetPacketNotify::FNotificationHeader Data, T& OutSequenceNumbers)
	{
		SIZE_T NotificationCount = 0;

		auto HandleAck = [&OutSequenceNumbers, &NotificationCount](FNetPacketNotify::SequenceNumberT Seq, bool delivered)
		{
			const SIZE_T MaxSequenceNumberCount = sizeof(OutSequenceNumbers) / sizeof(OutSequenceNumbers[0]);

			if (delivered)
			{
				if (NotificationCount < MaxSequenceNumberCount)
				{
					OutSequenceNumbers[NotificationCount] = Seq;
				}
				++NotificationCount;		
			}
		};
		return PacketNotify.Update(Data, HandleAck);
	}

	// Test to fake sending and receiving an array of sequence numbers and test if we get the expected notifications back
	template<typename T>
	static bool TestNotificationSequence(const T& InSequenceNumbers, FNetPacketNotify::SequenceNumberT FirstSequence = 0)
	{
		T NotifiedSequenceNumbers = { 0 };

		// Sender, which we will also be the receiver of the acks
		FNetPacketNotify Sender;
		Sender.Init(FNetPacketNotify::SequenceNumberT(FirstSequence.Get() - 1), FirstSequence);
	
		// pretend that we have sent the InSequenceNumbers
		SIZE_T Count = sizeof(T) / sizeof(InSequenceNumbers[0]);
		for (SIZE_T It=0; It < Count; ++It)
		{			
			do
			{
				FNetPacketNotifyTestUtil::PretendSendSeq(Sender, 0);
			}
			while (InSequenceNumbers[It] >= Sender.GetOutSeq());
		}

		// Receiver which we fake have received the packets sent from sender
		FNetPacketNotify Receiver;
		Receiver.Init(FNetPacketNotify::SequenceNumberT(FirstSequence.Get() - 1), FirstSequence);
		PretendAckSequenceNumbers(Receiver, InSequenceNumbers);

		// Fake Header with acks sent from receiver back to sender
		FNetPacketNotify::FNotificationHeader Data;
		Data.Seq = Receiver.GetOutSeq();
		Data.AckedSeq = Receiver.GetInAckSeq();
		Data.HistoryWordCount = FNetPacketNotify::SequenceHistoryT::WordCount;
		Data.History = Receiver.GetInSeqHistory();

		// Process the received ack information
		PretendReceivedPacket(Sender, Data, NotifiedSequenceNumbers);

		// Check that it matches the expected result
		return FPlatformMemory::Memcmp(&InSequenceNumbers[0], &NotifiedSequenceNumbers[0], sizeof(InSequenceNumbers)) == 0;
	}
};

bool FNetPacketNotifyTest::RunTest(const FString& Parameters)
{
	FNetPacketNotifyTestUtil Util;
	// Test fill
	{
		FNetPacketNotify::SequenceNumberT ExpectedInSeq(31);
		FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0xffffffffu, 1);

		FNetPacketNotify Acks = Util.DefaultNotify;
				
		for (int32 I=0; I<32; ++I)
		{
			FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, I, true);
		}

		TestEqual(TEXT("Test fill - InSeq"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test fill - History"), Acks.GetInSeqHistory(), ExpectedInSeqHistory);
	}

	// Test drop every other
	{
		FNetPacketNotify::SequenceNumberT ExpectedInSeq(30);
		FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0x55555555u, 1);

		FNetPacketNotify Acks = Util.DefaultNotify;
				
		for (int32 I=0; I<16; ++I)
		{
			FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, I*2);
		}
		
		TestEqual(TEXT("Test drop every other - InSeq"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test drop every other - History"), Acks.GetInSeqHistory(), ExpectedInSeqHistory);
	}

	// Test burst drop
	{
		FNetPacketNotify::SequenceNumberT ExpectedInSeq(128);
		FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory;
		uint32 ExpectedArray[] = {0x1, 0, 0, 0x20000000 };
		FNetPacketNotifyTestUtil::InitHistory(ExpectedInSeqHistory, ExpectedArray );

		FNetPacketNotify Acks = Util.DefaultNotify;

		// Drop early
		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, 3);

		// Large gap until next seq
		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, 128);

		TestEqual(TEXT("Test burst drop - InSeq"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test burst drop - History"), Acks.GetInSeqHistory(), ExpectedInSeqHistory);
	}

	// Test window overflow
	{
		FNetPacketNotify Acks = Util.DefaultNotify;

		const FNetPacketNotify::SequenceNumberT ExpectedInSeq(0);
		const FNetPacketNotify::SequenceNumberT MaxWindowSeq(FNetPacketNotify::SequenceNumberT::SeqNumberHalf);

		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, ExpectedInSeq, true);
		TestEqual(TEXT("Test window overflow - Expect InSeq 0"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test window overflow - Expect InAckSeq 0"), Acks.GetInAckSeq(), ExpectedInSeq);

		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, MaxWindowSeq, true);
		TestEqual(TEXT("Test window overflow - Expect Seq reject"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test window overflow - Expect Seq reject"), Acks.GetInAckSeq(), ExpectedInSeq);

		const FNetPacketNotify::SequenceNumberT NextExpectedInSeq(1);
		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, NextExpectedInSeq, true);
		TestEqual(TEXT("Test window overflow - Expect InSeq 1"), Acks.GetInSeq(), NextExpectedInSeq);
		TestEqual(TEXT("Test window overflow - Expect InAckSeq 1"), Acks.GetInAckSeq(), NextExpectedInSeq);

		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, MaxWindowSeq, true);
		TestEqual(TEXT("Test window overflow - Expect InSeq MaxWindowSeq"), Acks.GetInSeq(), MaxWindowSeq);
		TestEqual(TEXT("Test window overflow - Expect InSeq MaxWindowSeq"), Acks.GetInAckSeq(), MaxWindowSeq);
	}

	// create history
	{
		const FNetPacketNotify::SequenceNumberT ExpectedInSeq(18);
		const FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0x8853u);

		const FNetPacketNotify::SequenceNumberT AckdPacketIds[] = {3, 7, 12, 14, 17, 18};
		const SIZE_T ExpectedCount = sizeof(AckdPacketIds)/sizeof((AckdPacketIds)[0]);		

		FNetPacketNotify Acks = Util.DefaultNotify;
		FNetPacketNotifyTestUtil::PretendAckSequenceNumbers(Acks, AckdPacketIds);
 
		TestEqual(TEXT("Create history - InSeq"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Create history - InSeqHistory"), Acks.GetInSeqHistory(), ExpectedInSeqHistory);
	}
	
	// test notifications
	{
		static const FNetPacketNotify::SequenceNumberT ExpectedAckdPacketIds[] = {3, 7, 12, 14, 17, 18};
		static const SIZE_T ExpectedCount = sizeof(ExpectedAckdPacketIds)/sizeof((ExpectedAckdPacketIds)[0]);		

		FNetPacketNotify::SequenceNumberT RcvdAcks[ExpectedCount] = { 0 };
		SIZE_T RcvdCount = 0;

		// Create src data
		FNetPacketNotify Acks = Util.DefaultNotify;

		// Fill in some data
		FNetPacketNotify::FNotificationHeader Data;
		Data.Seq = FNetPacketNotify::SequenceNumberT(0);
		Data.AckedSeq = FNetPacketNotify::SequenceNumberT(18);
		Data.History = FNetPacketNotify::SequenceHistoryT(0x8853u);
		Data.HistoryWordCount = 1;

		// Need to fake ack record as well.
		for (SIZE_T It=0; It <= 18; ++It)
		{
			FNetPacketNotifyTestUtil::PretendSendSeq(Acks, 0);
		}
	
		SIZE_T DeltaSeq = FNetPacketNotifyTestUtil::PretendReceivedPacket(Acks, Data, RcvdAcks);

		TestEqual(TEXT("Notifications - Create sequence delta"), DeltaSeq, SIZE_T(1));
		TestEqual(TEXT("Notifications - Create sequence"), FPlatformMemory::Memcmp(ExpectedAckdPacketIds, RcvdAcks, sizeof(ExpectedAckdPacketIds)), 0u);
	}

	// test received invalid ack
	{
		static const FNetPacketNotify::SequenceNumberT ExpectedAckdPacketIds[] = {3, 7, 12, 14, 17, 18};
		static const SIZE_T ExpectedCount = sizeof(ExpectedAckdPacketIds)/sizeof((ExpectedAckdPacketIds)[0]);		

		FNetPacketNotify::SequenceNumberT RcvdAcks[ExpectedCount] = { 0 };
		SIZE_T RcvdCount = 0;

		// Create src data
		FNetPacketNotify Acks = Util.DefaultNotify;

		// Fill in some data
		FNetPacketNotify::FNotificationHeader Data;
		Data.Seq = FNetPacketNotify::SequenceNumberT(0);
		Data.AckedSeq = FNetPacketNotify::SequenceNumberT(19);
		Data.History = FNetPacketNotify::SequenceHistoryT(0x8853u);
		Data.HistoryWordCount = 1;

		// Need to fake ack record as well.
		for (SIZE_T It=0; It <= 18; ++It)
		{
			FNetPacketNotifyTestUtil::PretendSendSeq(Acks, 0);
		}
	
		SIZE_T DeltaSeq = FNetPacketNotifyTestUtil::PretendReceivedPacket(Acks, Data, RcvdAcks);

		TestEqual(TEXT("Notifications - Create sequence delta"), DeltaSeq, SIZE_T(0));
	}


	// test various sequence
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {3, 7, 12, 14, 17, 18};	
		TestTrue(TEXT("Test Seq {3, 7, 12, 14, 17, 18}"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};	
		TestTrue(TEXT("Test Seq {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};	
		TestTrue(TEXT("{2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, FNetPacketNotify::MaxSequenceHistoryLength - 1};	
		TestTrue(TEXT("Test Seq {0, FNetPacketNotify::MaxSequenceHistoryLength - 1}"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		// This test will trigger a warning since the sequence delta is larger than the history window so we supress it
		LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, FNetPacketNotify::MaxSequenceHistoryLength};	
		TestFalse(TEXT("Test Seq {0, FNetPacketNotify::MaxSequenceHistoryLength}"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, FNetPacketNotify::SequenceNumberT::SeqNumberHalf };
		TestFalse(TEXT("Test Seq {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0};"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, FNetPacketNotify::SequenceNumberT::SeqNumberHalf - 1};
		TestFalse(TEXT("Test Seq {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0};"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0};
		TestFalse(TEXT("Test Seq {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0} From 0;"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		// This test will trigger a warning since the sequence delta is larger than the history window so we supress it
		LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0};
		TestTrue(TEXT("Test Seq {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0} From SeqNumberHalf + 1;"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs, FNetPacketNotify::SequenceNumberT::SeqNumberHalf + 2));
	}

	check(!HasAnyErrors());

	return true;
}

#endif // #if WITH_DEV_AUTOMATION_TESTS