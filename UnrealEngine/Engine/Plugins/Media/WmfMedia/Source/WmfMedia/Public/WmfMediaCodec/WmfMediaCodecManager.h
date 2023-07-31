// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Templates/UniquePtr.h"

#include "WmfMediaCommon.h"

#include "IWmfMediaCodec.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

class WMFMEDIA_API WmfMediaCodecManager : public IWmfMediaCodec
{
public:
	WmfMediaCodecManager() = default;
	WmfMediaCodecManager(const WmfMediaCodecManager&) = delete;
	WmfMediaCodecManager(WmfMediaCodecManager&&) = delete;
	WmfMediaCodecManager& operator=(const WmfMediaCodecManager&) = delete;

	virtual bool IsCodecSupported(const GUID& InMajorType, const GUID& InSubType) const override;
	virtual bool SetVideoFormat(const GUID& InSubType, GUID& OutVideoFormat) const override;
	virtual bool SetupDecoder(
		const GUID& InMajorType,
		const GUID& InSubType,
		TComPtr<IMFTopologyNode>& InDecoderNode,
		TComPtr<IMFTransform>& InTransform) override;

	void AddCodec(TUniquePtr<IWmfMediaCodec> InCodec);

private:
	TArray<TUniquePtr<IWmfMediaCodec>> RegisteredCodec;
};

#else

class WmfMediaCodecManager
{

};

#endif // WMFMEDIA_SUPPORTED_PLATFORM
