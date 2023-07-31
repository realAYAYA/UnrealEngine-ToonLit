// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

namespace ElectraCDM
{


/**
 *
 */
class ELECTRACDM_API IStreamDecrypterAES128
{
public:
	static TSharedPtr<IStreamDecrypterAES128, ESPMode::ThreadSafe> Create();
	virtual ~IStreamDecrypterAES128() = default;

	enum class EResult
	{
		Ok,
		NotInitialized,
		BadKeyLength,
		BadIVLength,
		BadDataLength,
		InvalidArg,
		BadHexChar
	};
	static const TCHAR* GetResultText(EResult ResultCode);

	static EResult ConvHexStringToBin(TArray<uint8>& OutBinData, const char* InHexString);
	static void MakePaddedIVFromUInt64(TArray<uint8>& OutBinData, uint64 lower64Bits);

	virtual EResult CBCInit(const TArray<uint8>& Key, const TArray<uint8>* OptionalIV=nullptr) = 0;
	virtual EResult CBCDecryptInPlace(int32& OutNumBytes, uint8* InOutData, int32 NumBytes16, bool bIsFinalBlock) = 0;
	virtual int32 CBCGetEncryptionDataSize(int32 PlaintextSize) = 0;
	virtual EResult CBCEncryptInPlace(int32& OutNumBytes, uint8* InOutData, int32 NumBytes, bool bIsFinalData) = 0;

	virtual EResult CTRInit(const TArray<uint8>& Key) = 0;
	virtual EResult CTRSetKey(const TArray<uint8>& Key) = 0;
	virtual EResult CTRSetIV(const TArray<uint8>& IV) = 0;
	virtual EResult CTRDecryptInPlace(uint8* InOutData, int32 InNumBytes) = 0;
};


} // namespace ElectraCDM


