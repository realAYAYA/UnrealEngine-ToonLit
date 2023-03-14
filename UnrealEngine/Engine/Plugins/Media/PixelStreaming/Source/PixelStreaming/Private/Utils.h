// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCIncludes.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"

namespace UE::PixelStreaming
{
	template <typename T>
	inline void CommandLineParseValue(const TCHAR* Match, TAutoConsoleVariable<T>& CVar)
	{
		T Value;
		if (FParse::Value(FCommandLine::Get(), Match, Value))
			CVar->Set(Value, ECVF_SetByCommandline);
	};

	inline void CommandLineParseValue(const TCHAR* Match, TAutoConsoleVariable<FString>& CVar, bool bStopOnSeparator = false)
	{
		FString Value;
		if (FParse::Value(FCommandLine::Get(), Match, Value, bStopOnSeparator))
			CVar->Set(*Value, ECVF_SetByCommandline);
	};

	inline void CommandLineParseOption(const TCHAR* Match, TAutoConsoleVariable<bool>& CVar)
	{
		FString ValueMatch(Match);
		ValueMatch.Append(TEXT("="));
		FString Value;
		if (FParse::Value(FCommandLine::Get(), *ValueMatch, Value))
		{
			if (Value.Equals(FString(TEXT("true")), ESearchCase::IgnoreCase))
			{
				CVar->Set(true, ECVF_SetByCommandline);
			}
			else if (Value.Equals(FString(TEXT("false")), ESearchCase::IgnoreCase))
			{
				CVar->Set(false, ECVF_SetByCommandline);
			}
		}
		else if (FParse::Param(FCommandLine::Get(), Match))
		{
			CVar->Set(true, ECVF_SetByCommandline);
		}
	}

	template <typename T>
	void DoOnGameThread(T&& Func)
	{
		if (IsInGameThread())
		{
			Func();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [Func]() { Func(); });
		}
	}

	template <typename T>
	void DoOnGameThreadAndWait(uint32 Timeout, T&& Func)
	{
		if (IsInGameThread())
		{
			Func();
		}
		else
		{
			FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool();
			AsyncTask(ENamedThreads::GameThread, [Func, TaskEvent]() {
				Func();
				TaskEvent->Trigger();
			});
			TaskEvent->Wait(Timeout);
			FPlatformProcess::ReturnSynchEventToPool(TaskEvent);
		}
	}
#if WEBRTC_VERSION == 84
	inline webrtc::SdpVideoFormat CreateH264Format(webrtc::H264::Profile profile, webrtc::H264::Level level)
	{
		const absl::optional<std::string> ProfileString =
			webrtc::H264::ProfileLevelIdToString(webrtc::H264::ProfileLevelId(profile, level));
		check(ProfileString);
		return webrtc::SdpVideoFormat(
			cricket::kH264CodecName,
			{ { cricket::kH264FmtpProfileLevelId, *ProfileString },
				{ cricket::kH264FmtpLevelAsymmetryAllowed, "1" },
				{ cricket::kH264FmtpPacketizationMode, "1" } });

	}
#elif WEBRTC_VERSION == 96
	inline webrtc::SdpVideoFormat CreateH264Format(webrtc::H264Profile profile, webrtc::H264Level level)
	{
		const absl::optional<std::string> ProfileString =
			webrtc::H264ProfileLevelIdToString(webrtc::H264ProfileLevelId(profile, level));
		check(ProfileString);
		return webrtc::SdpVideoFormat(
			cricket::kH264CodecName,
			{ { cricket::kH264FmtpProfileLevelId, *ProfileString },
				{ cricket::kH264FmtpLevelAsymmetryAllowed, "1" },
				{ cricket::kH264FmtpPacketizationMode, "1" } });

	}
#endif
	inline void MemCpyStride(void* Dest, const void* Src, size_t DestStride, size_t SrcStride, size_t Height)
	{
		char* DestPtr = static_cast<char*>(Dest);
		const char* SrcPtr = static_cast<const char*>(Src);
		size_t Row = Height;
		while (Row--)
		{
			FMemory::Memcpy(DestPtr + DestStride * Row, SrcPtr + SrcStride * Row, DestStride);
		}
	}

	inline size_t SerializeToBuffer(rtc::CopyOnWriteBuffer& Buffer, size_t Pos, const void* Data, size_t DataSize)
	{
#if WEBRTC_VERSION == 84
		FMemory::Memcpy(&Buffer[Pos], Data, DataSize);
#elif WEBRTC_VERSION == 96
		FMemory::Memcpy(Buffer.MutableData() + Pos, Data, DataSize);
#endif
		return Pos + DataSize;
	}

	inline void ExtendJsonWithField(const FString& Descriptor, FString FieldName, FString StringValue, FString& NewDescriptor, bool& Success)
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

		if (!Descriptor.IsEmpty())
		{
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Descriptor);
			if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
			{
				Success = false;
				return;
			}
		}

		TSharedRef<FJsonValueString> JsonValueObject = MakeShareable(new FJsonValueString(StringValue));
		JsonObject->SetField(FieldName, JsonValueObject);

		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&NewDescriptor);
		Success = FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter);
	}

	inline void ExtractJsonFromDescriptor(FString Descriptor, FString FieldName, FString& StringValue, bool& Success)
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Descriptor);
		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			const TSharedPtr<FJsonObject>* JsonObjectPtr = &JsonObject;
			Success = (*JsonObjectPtr)->TryGetStringField(FieldName, StringValue);
		}
		else
		{
			Success = false;
		}
	}
} // namespace UE::PixelStreaming
