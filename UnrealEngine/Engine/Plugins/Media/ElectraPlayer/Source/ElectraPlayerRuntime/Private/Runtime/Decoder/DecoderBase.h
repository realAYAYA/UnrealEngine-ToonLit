// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Player/PlayerSessionServices.h"
#include "Renderer/RendererBase.h"


namespace Electra
{
	class IAccessUnitBufferListener;
	class IDecoderOutputBufferListener;


	/**
	 *
	**/
	class IDecoderBase
	{
	public:
		virtual ~IDecoderBase() = default;
		virtual void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) = 0;
	};


	/**
	 *
	**/
	class IDecoderAUBufferDiags
	{
	public:
		virtual ~IDecoderAUBufferDiags() = default;
		virtual void SetAUInputBufferListener(IAccessUnitBufferListener* Listener) = 0;
	};


	/**
	 *
	**/
	class IDecoderReadyBufferDiags
	{
	public:
		virtual ~IDecoderReadyBufferDiags() = default;
		virtual void SetReadyBufferListener(IDecoderOutputBufferListener* Listener) = 0;
	};




	class FDecoderMessage : public IPlayerMessage
	{
	public:
		enum class EReason
		{
			DrainingFinished
		};

		static TSharedPtrTS<IPlayerMessage> Create(EReason InReason, IDecoderBase* InDecoderInstance, EStreamType InStreamType, FStreamCodecInformation::ECodec InCodec)
		{
			TSharedPtrTS<FDecoderMessage> p(new FDecoderMessage(InReason, InDecoderInstance, InStreamType, InCodec));
			return p;
		}

		static const FString& Type()
		{
			static FString TypeName("Decoder");
			return TypeName;
		}

		virtual const FString& GetType() const
		{
			return Type();
		}

		EReason GetReason() const
		{
			return Reason;
		}

		IDecoderBase* GetDecoderInstance() const
		{
			return DecoderInstance;
		}
		
		EStreamType GetStreamType() const
		{
			return StreamType;
		}
		
		FStreamCodecInformation::ECodec GetCodec() const
		{
			return Codec;
		}

	private:
		FDecoderMessage(EReason InReason, IDecoderBase* InDecoderInstance, EStreamType InStreamType, FStreamCodecInformation::ECodec InCodec)
			: Reason(InReason)
			, DecoderInstance(InDecoderInstance)
			, StreamType(InStreamType)
			, Codec(InCodec)
		{
		}
		EReason							Reason;
		IDecoderBase*					DecoderInstance;
		EStreamType						StreamType;
		FStreamCodecInformation::ECodec	Codec;
	};


} // namespace Electra

