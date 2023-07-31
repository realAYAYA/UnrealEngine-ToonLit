// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>



namespace Electra
{

	namespace ISO639
	{
		const TCHAR* Get639_1(const FString& InFrom639_1_2_3);
		FString MapTo639_1(const FString& InFrom639_1_2_3);

		FString RFC5646To639_1(const FString& InFromRFC5646);
	}

}

