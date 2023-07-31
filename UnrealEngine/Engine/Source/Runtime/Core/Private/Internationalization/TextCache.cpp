// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextCache.h"

#include "Containers/UnrealString.h"
#include "HAL/LowLevelMemTracker.h"
#include "Internationalization/TextKey.h"
#include "Misc/CString.h"
#include "Misc/LazySingleton.h"
#include "Misc/ScopeLock.h"

FTextCache& FTextCache::Get()
{
	return TLazySingleton<FTextCache>::Get();
}

void FTextCache::TearDown()
{
	return TLazySingleton<FTextCache>::TearDown();
}

FText FTextCache::FindOrCache(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey)
{
	LLM_SCOPE(ELLMTag::Localization);
	const FTextId TextId(InNamespace, InKey);

	// First try and find a cached instance
	{
		FScopeLock Lock(&CachedTextCS);
	
		const FText* FoundText = CachedText.Find(TextId);
		if (FoundText)
		{
			const FString* FoundTextLiteral = FTextInspector::GetSourceString(*FoundText);
			if (FoundTextLiteral && FCString::Strcmp(**FoundTextLiteral, InTextLiteral) == 0)
			{
				return *FoundText;
			}
		}
	}

	// Not currently cached, make a new instance...
	FText NewText = FText(InTextLiteral, TextId.GetNamespace(), TextId.GetKey(), ETextFlag::Immutable);

	// ... and add it to the cache
	{
		FScopeLock Lock(&CachedTextCS);

		CachedText.Emplace(TextId, NewText);
	}

	return NewText;
}