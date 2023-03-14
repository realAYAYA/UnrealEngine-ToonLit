// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpSerializeMessageTask.h"

#include "HAL/Event.h"
#include "IMessageContext.h"
#include "Backends/JsonStructSerializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "StructSerializer.h"

#include "UdpMessagingPrivate.h"
#include "Transport/UdpSerializedMessage.h"
#include "UdpMessagingSettings.h"
#include "UdpMessagingTracing.h"

namespace UdpSerializeMessageTaskDetails
{

/** Serialization Routine for message using Protocol version 10 */
void SerializeMessageV10(FArchive& Archive, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext)
{
	const FName& MessageType = MessageContext->GetMessageTypePathName().GetAssetName();
	Archive << const_cast<FName&>(MessageType);

	const FMessageAddress& Sender = MessageContext->GetSender();
	Archive << const_cast<FMessageAddress&>(Sender);

	const TArray<FMessageAddress>& Recipients = MessageContext->GetRecipients();
	Archive << const_cast<TArray<FMessageAddress>&>(Recipients);

	EMessageScope Scope = MessageContext->GetScope();
	Archive << Scope;

	const FDateTime& TimeSent = MessageContext->GetTimeSent();
	Archive << const_cast<FDateTime&>(TimeSent);

	const FDateTime& Expiration = MessageContext->GetExpiration();
	Archive << const_cast<FDateTime&>(Expiration);

	int32 NumAnnotations = MessageContext->GetAnnotations().Num();
	Archive << NumAnnotations;

	for (const auto& AnnotationPair : MessageContext->GetAnnotations())
	{
		Archive << const_cast<FName&>(AnnotationPair.Key);
		Archive << const_cast<FString&>(AnnotationPair.Value);
	}

	// serialize message body
	FJsonStructSerializerBackend Backend(Archive, EStructSerializerBackendFlags::Legacy);
	FStructSerializer::Serialize(MessageContext->GetMessage(), *MessageContext->GetMessageTypeInfo(), Backend);
}

/** Serialization Routine for message using Protocol version 11, 12, 13, 14 or 15. */
void SerializeMessageV11_15(FArchive& Archive, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext, EUdpMessageFormat MessageFormat, const EStructSerializerBackendFlags StructSerializerBackendFlags)
{
	const FName& MessageType = MessageContext->GetMessageTypePathName().GetAssetName();
	Archive << const_cast<FName&>(MessageType);

	const FMessageAddress& Sender = MessageContext->GetSender();
	Archive << const_cast<FMessageAddress&>(Sender);

	const TArray<FMessageAddress>& Recipients = MessageContext->GetRecipients();
	Archive << const_cast<TArray<FMessageAddress>&>(Recipients);

	EMessageScope Scope = MessageContext->GetScope();
	Archive << Scope;

	EMessageFlags Flags = MessageContext->GetFlags();
	Archive << Flags;

	const FDateTime& TimeSent = MessageContext->GetTimeSent();
	Archive << const_cast<FDateTime&>(TimeSent);

	const FDateTime& Expiration = MessageContext->GetExpiration();
	Archive << const_cast<FDateTime&>(Expiration);

	int32 NumAnnotations = MessageContext->GetAnnotations().Num();
	Archive << NumAnnotations;

	for (const auto& AnnotationPair : MessageContext->GetAnnotations())
	{
		Archive << const_cast<FName&>(AnnotationPair.Key);
		Archive << const_cast<FString&>(AnnotationPair.Value);
	}

	// Message Wire Format Id
	check(MessageFormat == EUdpMessageFormat::CborPlatformEndianness || MessageFormat == EUdpMessageFormat::CborStandardEndianness); // Versions 11 to 15 only supports CBOR.
	uint8 Format = (uint8)(MessageFormat);
	Archive << Format;
	
	// serialize message body with cbor
	FCborStructSerializerBackend Backend(Archive, StructSerializerBackendFlags | (MessageFormat == EUdpMessageFormat::CborStandardEndianness ? EStructSerializerBackendFlags::WriteCborStandardEndianness : EStructSerializerBackendFlags::None));
	FStructSerializer::Serialize(MessageContext->GetMessage(), *MessageContext->GetMessageTypeInfo(), Backend);
}

/** Serialization Routine for message using Protocol version 16. */
void SerializeMessageV16(FArchive& Archive, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext, EUdpMessageFormat MessageFormat, const EStructSerializerBackendFlags StructSerializerBackendFlags)
{
	const FTopLevelAssetPath& MessageType = MessageContext->GetMessageTypePathName();
	Archive << const_cast<FTopLevelAssetPath&>(MessageType);

	const FMessageAddress& Sender = MessageContext->GetSender();
	Archive << const_cast<FMessageAddress&>(Sender);

	const TArray<FMessageAddress>& Recipients = MessageContext->GetRecipients();
	Archive << const_cast<TArray<FMessageAddress>&>(Recipients);

	EMessageScope Scope = MessageContext->GetScope();
	Archive << Scope;

	EMessageFlags Flags = MessageContext->GetFlags();
	Archive << Flags;

	const FDateTime& TimeSent = MessageContext->GetTimeSent();
	Archive << const_cast<FDateTime&>(TimeSent);

	const FDateTime& Expiration = MessageContext->GetExpiration();
	Archive << const_cast<FDateTime&>(Expiration);

	int32 NumAnnotations = MessageContext->GetAnnotations().Num();
	Archive << NumAnnotations;

	for (const auto& AnnotationPair : MessageContext->GetAnnotations())
	{
		Archive << const_cast<FName&>(AnnotationPair.Key);
		Archive << const_cast<FString&>(AnnotationPair.Value);
	}

	// Message Wire Format Id
	check(MessageFormat == EUdpMessageFormat::CborPlatformEndianness || MessageFormat == EUdpMessageFormat::CborStandardEndianness); // Versions 11 to 15 only supports CBOR.
	uint8 Format = (uint8)(MessageFormat);
	Archive << Format;

	// serialize message body with cbor
	FCborStructSerializerBackend Backend(Archive, StructSerializerBackendFlags | (MessageFormat == EUdpMessageFormat::CborStandardEndianness ? EStructSerializerBackendFlags::WriteCborStandardEndianness : EStructSerializerBackendFlags::None));
	FStructSerializer::Serialize(MessageContext->GetMessage(), *MessageContext->GetMessageTypeInfo(), Backend);
}

} // namespace UdpSerializeMessageTaskDetails


/* FUdpSerializeMessageTask interface
 *****************************************************************************/

void FUdpSerializeMessageTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	SCOPED_MESSAGING_TRACE(FUdpSerializeMessageTask_DoTask);
	if (MessageContext->IsValid())
	{
		// Note that some complex values are serialized manually here, so that we can ensure
		// a consistent wire format, if their implementations change. This allows us to sanity
		// check the values during deserialization. @see FUdpDeserializeMessage::Deserialize()

		// serialize context depending on supported protocol version
		int64 ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)UINT16_MAX;
		FArchive& Archive = SerializedMessage.Get();
		bool Serialized = true;
		switch (SerializedMessage->GetProtocolVersion())
		{
			case 10:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)UINT16_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV10(Archive, MessageContext);
				break;

			case 11:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)UINT16_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV11_15(Archive, MessageContext, EUdpMessageFormat::CborPlatformEndianness, EStructSerializerBackendFlags::Legacy);
				break;

			case 12:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)INT32_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV11_15(Archive, MessageContext, EUdpMessageFormat::CborPlatformEndianness, EStructSerializerBackendFlags::WriteTextAsComplexString);
				break;

			case 13:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)INT32_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV11_15(Archive, MessageContext, EUdpMessageFormat::CborPlatformEndianness, EStructSerializerBackendFlags::LegacyUE4);
				break;

			case 14:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)INT32_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV11_15(Archive, MessageContext, SerializedMessage->GetFormat(), EStructSerializerBackendFlags::LegacyUE4);
				break;

			case 15:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)INT32_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV11_15(Archive, MessageContext, SerializedMessage->GetFormat(), EStructSerializerBackendFlags::Default);
				break;

			case 16:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)INT32_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV16(Archive, MessageContext, SerializedMessage->GetFormat(), EStructSerializerBackendFlags::Default);
				break;

			default:
				// Unsupported protocol version
				Serialized = false;
				break;
		}

		// if the message wasn't serialized, flag it invalid
		if (!Serialized)
		{
			UE_LOG(LogUdpMessaging, Error, TEXT("Unsupported Protocol Version message tasked for serialization, discarding..."));
			SerializedMessage->UpdateState(EUdpSerializedMessageState::Invalid);
		}
		// Once serialized if the size of the message is bigger than the maximum allow mark it as invalid and log an error
		else if (SerializedMessage->TotalSize() > ProtocolMaxSegmentSize)
		{
			UE_LOG(LogUdpMessaging, Error, TEXT("Serialized Message total size '%i' is over the allowed maximum '%i', discarding..."), SerializedMessage->TotalSize(), ProtocolMaxSegmentSize);
			SerializedMessage->UpdateState(EUdpSerializedMessageState::Invalid);
		}
		else
		{
			SerializedMessage->UpdateState(EUdpSerializedMessageState::Complete);
		}
	}
	else
	{
		SerializedMessage->UpdateState(EUdpSerializedMessageState::Invalid);
	}

	UE_LOG(LogUdpMessaging, Verbose, TEXT("Serialized %s from %s to %d bytes"), *MessageContext->GetMessageTypePathName().ToString(), *MessageContext->GetSender().ToString(), SerializedMessage->TotalSize());

	// signal task completion
	TSharedPtr<FEvent, ESPMode::ThreadSafe> CompletionEvent = CompletionEventPtr.Pin();

	if (CompletionEvent.IsValid())
	{
		CompletionEvent->Trigger();
	}
}


ENamedThreads::Type FUdpSerializeMessageTask::GetDesiredThread()
{
	return ENamedThreads::AnyThread;
}


TStatId FUdpSerializeMessageTask::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FUdpSerializeMessageTask, STATGROUP_TaskGraphTasks);
}


ESubsequentsMode::Type FUdpSerializeMessageTask::GetSubsequentsMode() 
{ 
	return ESubsequentsMode::FireAndForget; 
}
