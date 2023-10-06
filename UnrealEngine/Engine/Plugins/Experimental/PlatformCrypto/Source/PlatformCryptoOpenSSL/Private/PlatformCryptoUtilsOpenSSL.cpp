// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlatformCryptoUtilsOpenSSL.h"


void FPlatformCryptoUtilsOpenSSL::BigNumToArray(
	const BIGNUM* InNum, TArray<uint8>& OutBytes)
{
	int32 NumBytes = BN_num_bytes(InNum);
	OutBytes.SetNumZeroed(NumBytes);

	BN_bn2bin(InNum, OutBytes.GetData());
	Algo::Reverse(OutBytes);
}


void FPlatformCryptoUtilsOpenSSL::LoadBinaryIntoBigNum(
	const uint8* InData, int32 InDataSize, BIGNUM* InBigNum)
{
#if USE_LEGACY_OPENSSL
	TArray<uint8> Bytes(InData, InDataSize);
	Algo::Reverse(Bytes);
	BN_bin2bn(Bytes.GetData(), Bytes.Num(), InBigNum);
#else
	BN_lebin2bn(InData, InDataSize, InBigNum);
#endif
}
