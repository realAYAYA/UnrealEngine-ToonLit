// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Net/NetPacketNotify.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Net/Util/SequenceHistory.h"
#include "Net/Util/SequenceNumber.h"

namespace UE::Net
{

FTestMessage& operator<<(FTestMessage& Message, const FNetPacketNotify::SequenceNumberT& SequenceNumber)
{
	return Message << SequenceNumber.Get();
}

FTestMessage& operator<<(FTestMessage& Message, const FNetPacketNotify::SequenceHistoryT& History)
{
	for (uint32 WordIt=0U; WordIt<FNetPacketNotify::SequenceHistoryT::WordCount; ++WordIt)
	{
		Message << History.Data()[WordIt];
	}
	return Message;
}

namespace Private
{

struct FNetPacketNotifyTestUtil : public FNetworkAutomationTestSuiteFixture
{
	enum
	{
		LastValidSequenceHistoryIndex = FNetPacketNotify::MaxSequenceHistoryLength - 1
	};

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
		if (SeqDelta > 0 && !PacketNotify.IsWaitingForSequenceHistoryFlush())
		{
			if (Ack)
			{
				PacketNotify.AckSeq(Seq);
			}
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

	struct FTestNode
	{
		FNetPacketNotify PacketNotify;
		TArray<FNetPacketNotify::SequenceNumberT> AcceptedInPackets;
		TArray<FNetPacketNotify::SequenceNumberT> AcknowledgedPackets;

		FTestNode(FNetPacketNotify::SequenceNumberT FirstSequence = 0)
		{
			PacketNotify.Init(FNetPacketNotify::SequenceNumberT(FirstSequence.Get() - 1), FirstSequence);
		}

		FTestNode(FNetPacketNotify::SequenceNumberT InSequnce, FNetPacketNotify::SequenceNumberT OutSequnce)
		{
			PacketNotify.Init(InSequnce, OutSequnce);
		}

		FNetPacketNotify::FNotificationHeader Send()
		{
			const SIZE_T CurrentHistoryWordCount = FMath::Clamp<SIZE_T>((PacketNotify.GetCurrentSequenceHistoryLength() + FNetPacketNotify::SequenceHistoryT::BitsPerWord - 1u) / FNetPacketNotify::SequenceHistoryT::BitsPerWord, 1u, FNetPacketNotify::SequenceHistoryT::WordCount);
					
			FNetPacketNotify::FNotificationHeader Data;
			Data.Seq = PacketNotify.OutSeq;
			Data.AckedSeq = PacketNotify.InAckSeq;
			Data.HistoryWordCount = CurrentHistoryWordCount;
			Data.History = PacketNotify.InSeqHistory;

			PacketNotify.WrittenHistoryWordCount = CurrentHistoryWordCount;
			PacketNotify.WrittenInAckSeq = Data.AckedSeq;
			PacketNotify.CommitAndIncrementOutSeq();

			return Data;
		}

		void Receive(const FNetPacketNotify::FNotificationHeader Data)
		{
			SIZE_T NotificationCount = 0;

			auto HandleAck = [&](FNetPacketNotify::SequenceNumberT Seq, bool bDelivered)
			{
				if (bDelivered)
				{
					AcknowledgedPackets.Add(Seq);
				}
			};

			const uint32 SequenceDelta = PacketNotify.Update(Data, HandleAck);
			if (SequenceDelta > 0U && !PacketNotify.IsWaitingForSequenceHistoryFlush())
			{
				PacketNotify.AckSeq(PacketNotify.GetInSeq());
				AcceptedInPackets.Add(PacketNotify.GetInSeq());
			}
		}

		void SendAndDeliverTo(FTestNode& Target)
		{
			Target.Receive(Send());
		}

		void SendAndDeliverTo(FTestNode& Target, TArrayView<const FNetPacketNotify::SequenceNumberT> SequenceNumbers)
		{
			for (FNetPacketNotify::SequenceNumberT Seq : SequenceNumbers)
			{
				// Skip missing seqs
				while (Seq != PacketNotify.GetOutSeq())
				{
					Send();
				}
				// Send and receive seq
				Target.Receive(Send());
			}
		}

		bool ValidateAcknowledgedPackets(TArrayView<const FNetPacketNotify::SequenceNumberT> ExpectedSequenceNumbers) const
		{
			return AcknowledgedPackets == ExpectedSequenceNumbers;
		}

		bool ValidateAcceptedPackets(TArrayView<const FNetPacketNotify::SequenceNumberT> ExpectedSequenceNumbers) const
		{
			return AcceptedInPackets == ExpectedSequenceNumbers;
		}
	};
};

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, Fill)
{
	FNetPacketNotify Acks = DefaultNotify;

	FNetPacketNotify::SequenceNumberT ExpectedInSeq(30);
	FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0x55555555u, 1);
				
	for (FNetPacketNotify::SequenceNumberT::SequenceT I = 0U; I < 16U; ++I)
	{
		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, I * 2U);
	}
		
	// Verify InSeq
	UE_NET_ASSERT_EQ(Acks.GetInSeq(), ExpectedInSeq);

	// Verify History
	UE_NET_ASSERT_EQ(Acks.GetInSeqHistory(), ExpectedInSeqHistory);
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, DropEveryOther)
{
	FNetPacketNotify Acks = DefaultNotify;

	FNetPacketNotify::SequenceNumberT ExpectedInSeq(31U);
	FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0xffffffffu, 1U);
				
	for (FNetPacketNotify::SequenceNumberT::SequenceT I=0U; I<32U; ++I)
	{
		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, I, true);
	}

	// Verify InSeq
	UE_NET_ASSERT_EQ(Acks.GetInSeq(), ExpectedInSeq);

	// Verify History
	UE_NET_ASSERT_EQ(Acks.GetInSeqHistory(), ExpectedInSeqHistory);
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, SequenceNumberOverflow)
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

	FNetPacketNotify Acks = DefaultNotify;

	const FNetPacketNotify::SequenceNumberT MaxWindowSeq(FNetPacketNotify::SequenceNumberT::SeqNumberHalf);
	const FNetPacketNotify::SequenceNumberT ExpectedInSeq(0);

	// Verify default
	UE_NET_ASSERT_NE(Acks.GetInSeq(), ExpectedInSeq);

	// Pretend receive first seq
	FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, ExpectedInSeq, true);

	// Expects that we have accepted and acked the this sequence
	UE_NET_ASSERT_EQ(Acks.GetInSeq(), ExpectedInSeq);
	UE_NET_ASSERT_EQ(Acks.GetInAckSeq(), ExpectedInSeq);

	// Verify that we did not accept invalid sequence
	FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, MaxWindowSeq, true);
	UE_NET_ASSERT_EQ(Acks.GetInSeq(), ExpectedInSeq);
	UE_NET_ASSERT_EQ(Acks.GetInAckSeq(), ExpectedInSeq);

	// Verify that we can receive next valid sequence
	const FNetPacketNotify::SequenceNumberT NextExpectedInSeq(1);
	FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, NextExpectedInSeq, true);
	UE_NET_ASSERT_EQ(Acks.GetInSeq(), NextExpectedInSeq);
	UE_NET_ASSERT_EQ(Acks.GetInAckSeq(), NextExpectedInSeq);

	// Verify that we accepted a new sequence even if it is out and bounds and will trigger a SequencyHistoryFlush
	FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, MaxWindowSeq, true);
	UE_NET_ASSERT_GT(Acks.GetInSeq(), NextExpectedInSeq);
	UE_NET_ASSERT_GT(Acks.GetInAckSeq(), NextExpectedInSeq);
	UE_NET_ASSERT_TRUE(Acks.IsWaitingForSequenceHistoryFlush());
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, BurstDrop)
{
	FNetPacketNotify Acks = DefaultNotify;

	FNetPacketNotify::SequenceNumberT ExpectedInSeq(128);
	FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory;
	uint32 ExpectedArray[] = {0x1, 0, 0, 0x20000000 };
	FNetPacketNotifyTestUtil::InitHistory(ExpectedInSeqHistory, ExpectedArray );

	// Drop early
	FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, 3);

	// Large gap until next seq
	FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, 128);

	UE_NET_ASSERT_EQ(Acks.GetInSeq(), ExpectedInSeq);
	UE_NET_ASSERT_EQ(Acks.GetInSeqHistory(), ExpectedInSeqHistory);
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, CreateHistory)
{
	FNetPacketNotify Acks = DefaultNotify;

	const FNetPacketNotify::SequenceNumberT ExpectedInSeq(18);
	const FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0x8853u);

	const FNetPacketNotify::SequenceNumberT AckdPacketIds[] = {3, 7, 12, 14, 17, 18};
	const SIZE_T ExpectedCount = sizeof(AckdPacketIds)/sizeof((AckdPacketIds)[0]);		

	FNetPacketNotifyTestUtil::PretendAckSequenceNumbers(Acks, AckdPacketIds);
 
	UE_NET_ASSERT_EQ(Acks.GetInSeq(), ExpectedInSeq);
	UE_NET_ASSERT_EQ(Acks.GetInSeqHistory(), ExpectedInSeqHistory);
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, Notifications)
{
	FNetPacketNotify Acks = DefaultNotify;

	static const FNetPacketNotify::SequenceNumberT ExpectedAckdPacketIds[] = {3, 7, 12, 14, 17, 18};
	static const SIZE_T ExpectedCount = sizeof(ExpectedAckdPacketIds)/sizeof((ExpectedAckdPacketIds)[0]);		

	FNetPacketNotify::SequenceNumberT RcvdAcks[ExpectedCount] = { 0 };
	SIZE_T RcvdCount = 0;

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

	UE_NET_ASSERT_EQ(DeltaSeq, SIZE_T(1));
	UE_NET_ASSERT_EQ(FPlatformMemory::Memcmp(ExpectedAckdPacketIds, RcvdAcks, sizeof(ExpectedAckdPacketIds)), 0);
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, ReceiveInvalidAck)
{
	FNetPacketNotify Acks = DefaultNotify;

	static const FNetPacketNotify::SequenceNumberT ExpectedAckdPacketIds[] = {3, 7, 12, 14, 17, 18};
	static const SIZE_T ExpectedCount = sizeof(ExpectedAckdPacketIds)/sizeof((ExpectedAckdPacketIds)[0]);		

	FNetPacketNotify::SequenceNumberT RcvdAcks[ExpectedCount] = { 0 };
	SIZE_T RcvdCount = 0;

	// Fill in some data
	FNetPacketNotify::FNotificationHeader Data;
	Data.Seq = FNetPacketNotify::SequenceNumberT(0);
	Data.AckedSeq = FNetPacketNotify::SequenceNumberT(19);
	Data.History = FNetPacketNotify::SequenceHistoryT(0x8853u);
	Data.HistoryWordCount = 1;

	// Need to fake ack record as well.
	for (SIZE_T It=0; It <= 18; ++It)
	{
		FNetPacketNotifyTestUtil::PretendSendSeq(Acks, FNetPacketNotify::SequenceNumberT(0U));
	}
	
	SIZE_T DeltaSeq = FNetPacketNotifyTestUtil::PretendReceivedPacket(Acks, Data, RcvdAcks);

	UE_NET_ASSERT_EQ(DeltaSeq, SIZE_T(0));
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, FillSeqWindowWitNoMargin)
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

	// Test without safety margin
	for (FNetPacketNotify::SequenceNumberT SeqOffset : {FNetPacketNotify::SequenceNumberT(0), FNetPacketNotify::SequenceNumberT(3)})
	{
		FNetPacketNotify PacketNotify = DefaultNotify;
		for (FNetPacketNotify::SequenceNumberT It = 0; It < FNetPacketNotify::MaxSequenceHistoryLength - 1; ++It)
		{
			PretendSendSeq(PacketNotify, SeqOffset);
		}
		UE_NET_ASSERT_FALSE_MSG(PacketNotify.IsSequenceWindowFull(), "Test SeqWindowFull {0, MaxSequenceHistoryLength - 2}");

		// Fill sequence window
		FNetPacketNotifyTestUtil::PretendSendSeq(PacketNotify, SeqOffset);
		UE_NET_ASSERT_TRUE_MSG(PacketNotify.IsSequenceWindowFull(), "Test SeqWindowFull {0, MaxSequenceHistoryLength - 1}");

		// Fake all acked. We expect acks on all sequence numbers.
		{
			FNetPacketNotify::FNotificationHeader NotificationData;
			NotificationData.Seq = 0;
			NotificationData.AckedSeq = PacketNotify.GetOutSeq() - 1;
			NotificationData.History = FNetPacketNotify::SequenceHistoryT(0xFFFFFFFFU, FNetPacketNotify::SequenceHistoryT::WordCount);
			NotificationData.HistoryWordCount = FNetPacketNotify::SequenceHistoryT::WordCount;

			PacketNotify.Update(NotificationData, [this](FNetPacketNotify::SequenceNumberT, bool bWasDelivered)
				{
					UE_NET_ASSERT_TRUE_MSG(bWasDelivered, "Test SeqWindowFull all delivered");
				});
		}
	}
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, FillSeqWindowWitSafetyMargin)
{
	// Test with safety margin
	constexpr unsigned SafetyMargin = 3;
	FNetPacketNotify PacketNotify = DefaultNotify;
	for (SIZE_T It = 0; It < FNetPacketNotify::MaxSequenceHistoryLength - 1 - SafetyMargin; ++It)
	{
		FNetPacketNotifyTestUtil::PretendSendSeq(PacketNotify, 0);
	}
	UE_NET_ASSERT_FALSE_MSG(PacketNotify.IsSequenceWindowFull(SafetyMargin), "Test SeqWindowFull Margin 3 {0, MaxSequenceHistoryLength - 2}");

	FNetPacketNotifyTestUtil::PretendSendSeq(PacketNotify, 0);
	UE_NET_ASSERT_TRUE_MSG(PacketNotify.IsSequenceWindowFull(SafetyMargin), "Test SeqWindowFull Margin 3 {0, MaxSequenceHistoryLength - 1}");
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestSendAndReceive)
{
	FTestNode Src;
	FTestNode Dst;

	// Pretend to send and accept packet in both directions
	Src.SendAndDeliverTo(Dst);
	Dst.SendAndDeliverTo(Src);

	// Validate expected state
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({0}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({0}));
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestIgnoresOutOfBound)
{
	// Initialize with an offset that wraps around the sequence space
	FTestNode Src(FNetPacketNotify::SequenceNumberT::SeqNumberHalf);
	FTestNode Dst;

	// Pretend to send and accept packet in both directions
	Src.SendAndDeliverTo(Dst);
	Dst.SendAndDeliverTo(Src);

	// Validate expected state
	UE_NET_ASSERT_EQ(Dst.AcceptedInPackets.Num(), 0);
	UE_NET_ASSERT_EQ(Src.AcknowledgedPackets.Num(), 0);	
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestOutOfBoundDoesNotTriggerFlush)
{
	FTestNode Src;
	FTestNode Dst;

	// Pretend to send and accept packet in both directions
	Src.SendAndDeliverTo(Dst , {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0U});
	Dst.SendAndDeliverTo(Src);

	// Validate expected state
	UE_NET_ASSERT_FALSE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({0}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({0}));
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestBurstReceiveValidRangeWrapAround)
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

	// Initialize with an offset that wraps around the sequence space
	FTestNode Src(FNetPacketNotify::SequenceNumberT::SeqNumberHalf + 2);
	FTestNode Dst(FNetPacketNotify::SequenceNumberT::SeqNumberHalf + 2);

	// Pretend to send and accept packet in both directions
	Src.SendAndDeliverTo(Dst, {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0U});
	Dst.SendAndDeliverTo(Src);

	// Validate expected state
	UE_NET_ASSERT_FALSE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0U}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0U}));
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestAcceptsHugeDeltaWithNoPreviousAcks)
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

	// Initialize with an offset that is at the end of the sequence space
	const FNetPacketNotify::SequenceNumberT FirstOutSeq(FNetPacketNotify::SequenceNumberT::SeqNumberHalf - 2);

	FTestNode Src(-1, FirstOutSeq);
	FTestNode Dst;

	// Pretend to send and accept packet in both directions
	Src.SendAndDeliverTo(Dst);
	Dst.SendAndDeliverTo(Src);

	// Validate expected state
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({FirstOutSeq}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({FirstOutSeq}));
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestBurstReceiveValidRange)
{
	FTestNode Src;
	FTestNode Dst;

	// Send a range burst of packets
	Src.SendAndDeliverTo(Dst, {0U, LastValidSequenceHistoryIndex});

	// Respond
	Dst.SendAndDeliverTo(Src);

	// Validate expected state
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({0U, LastValidSequenceHistoryIndex}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({0U, LastValidSequenceHistoryIndex}));	
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestBurstReceiveTriggersWaitForSequenceWindowFlush)
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

	FTestNode Src;
	FTestNode Dst;

	// Send a range burst of packets
	Src.SendAndDeliverTo(Dst, {0U, FNetPacketNotify::MaxSequenceHistoryLength});

	// Respond
	Dst.SendAndDeliverTo(Src);

	// Verify that we triggered overshoot
	UE_NET_ASSERT_TRUE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());

	// Validate expected state
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({0U}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({0U}));	
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestReceiveDoesNotTriggersWaitForSequenceWindowFlushIfNoAcks)
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

	FTestNode Src;
	FTestNode Dst;

	// Send a specific sequence that would overflow window if we had already acked data
	// we have no previous acks we can accept the sequence and allow the other end to inject any missing nacks
	Src.SendAndDeliverTo(Dst, {FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2});

	// Respond
	Dst.SendAndDeliverTo(Src);

	// Verify that we triggered overshoot
	UE_NET_ASSERT_FALSE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());

	// Validate expected state
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2}));
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestCanRecoverFromsWaitForSequenceWindowFlush)
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

	FTestNode Src;
	FTestNode Dst;

	// Send a range burst of packets
	Src.SendAndDeliverTo(Dst, {0U, FNetPacketNotify::MaxSequenceHistoryLength});

	// Respond
	Dst.SendAndDeliverTo(Src);

	// Verify that we triggered overshoot
	UE_NET_ASSERT_TRUE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());

	// Validate expected state
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({0U}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({0U}));

	// Send a range burst of packets
	Src.SendAndDeliverTo(Dst, {FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2});

	// Verify that we recovered from overshoot
	UE_NET_ASSERT_FALSE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());

	// Respond
	Dst.SendAndDeliverTo(Src);

	// Validate expected state
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({0U, FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2}));
	UE_NET_ASSERT_TRUE(Dst.ValidateAcceptedPackets({0U, FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2}));
}

UE_NET_TEST_FIXTURE(FNetPacketNotifyTestUtil, TestGetBothEndsWaitForSequenceWindowFlush)
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::Error);

	FTestNode Src;
	FTestNode Dst;

	// Send a range burst of packets
	Src.SendAndDeliverTo(Dst, {0U, FNetPacketNotify::MaxSequenceHistoryLength});
	Dst.SendAndDeliverTo(Src, {0U, FNetPacketNotify::MaxSequenceHistoryLength});

	// Verify that we triggered overshoot on both ends
	UE_NET_ASSERT_TRUE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());
	UE_NET_ASSERT_TRUE(Src.PacketNotify.IsWaitingForSequenceHistoryFlush());

	// Send a range burst of from source to src, the packets should be accepted as we have cleared the sequence history
	Src.SendAndDeliverTo(Dst, {FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2});

	// Verify that we recovered from overshoot
	UE_NET_ASSERT_FALSE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());

	// Src should still be waiting on flush
	UE_NET_ASSERT_TRUE(Src.PacketNotify.IsWaitingForSequenceHistoryFlush());

	// Send a range burst of from dst to src, the packets should be accepted as we have cleared the sequence history
	Dst.SendAndDeliverTo(Src, {FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2});

	// Verify that both recovered from overshoot
	UE_NET_ASSERT_FALSE(Src.PacketNotify.IsWaitingForSequenceHistoryFlush());
	UE_NET_ASSERT_FALSE(Dst.PacketNotify.IsWaitingForSequenceHistoryFlush());

	// Respond
	Src.SendAndDeliverTo(Src);
	Dst.SendAndDeliverTo(Dst);

	// Validate expected state
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({0, FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2}));
	UE_NET_ASSERT_TRUE(Src.ValidateAcceptedPackets({0, FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2, FNetPacketNotify::MaxSequenceHistoryLength + 3}));
	UE_NET_ASSERT_TRUE(Src.ValidateAcknowledgedPackets({0, FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2}));
	UE_NET_ASSERT_TRUE(Src.ValidateAcceptedPackets({0, FNetPacketNotify::MaxSequenceHistoryLength + 1, FNetPacketNotify::MaxSequenceHistoryLength + 2, FNetPacketNotify::MaxSequenceHistoryLength + 3}));
}

}
}

