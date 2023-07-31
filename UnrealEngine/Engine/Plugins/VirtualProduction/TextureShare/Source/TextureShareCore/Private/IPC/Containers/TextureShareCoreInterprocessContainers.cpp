// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPC/Containers/TextureShareCoreInterprocessContainers.h"

#include "Misc/SecureHash.h"
#include "Windows/MinimalWindowsApi.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareCoreInterprocessContainersHelper
{
	static FTextureShareCoreSMD5Hash CreateEmptySMD5Hash()
	{
		FTextureShareCoreSMD5Hash Result;
		Result.Empty();

		return Result;
	}
};
using namespace TextureShareCoreInterprocessContainersHelper;

//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareCoreSMD5Hash FTextureShareCoreSMD5Hash::EmptyValue = CreateEmptySMD5Hash();

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreSMD5Hash
//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreSMD5Hash::Initialize(const FString& InText)
{
	Empty();

	// Create MD5 Hash
	const uint8* BlobInput = (unsigned char*)TCHAR_TO_ANSI(*InText);
	uint64 BlobInputLen = FCString::Strlen(*InText);

	FMD5 Md5Gen;
	Md5Gen.Update(BlobInput, BlobInputLen);
	Md5Gen.Final(MD5Digest);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreSMD5HashList
//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreSMD5HashList::Initialize(const TArraySerializable<FString>& InStringList)
{
	Empty();

	int32 Index = 0;
	for (const FString& StringIt : InStringList)
	{
		if (Index < MaxStringsCnt)
		{
			List[Index].Initialize(StringIt);
		}
		else
		{
			break;
		}

		Index++;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreStringHash
//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreStringHash::Initialize(const FString& InText)
{
	Empty();

	Hash.Initialize(InText);

	// Copy string
	const wchar_t* WCharValue = FTCHARToWChar(*InText).Get();
	if (WCharValue && WCharValue[0])
	{
		for (int32 CharIt = 0; CharIt < (MaxStringLength-1); CharIt++)
		{
			String[CharIt] = WCharValue[CharIt];

			// EndOfString
			if (WCharValue[CharIt] == 0)
			{
				break;
			}
		}
	}
}
