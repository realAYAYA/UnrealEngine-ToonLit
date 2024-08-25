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
	return FindOrCache(InTextLiteral, FTextId(InNamespace, InKey));
}

FText FTextCache::FindOrCache(const TCHAR* InTextLiteral, const FTextId& InTextId)
{
	LLM_SCOPE(ELLMTag::Localization);

	// First try and find a cached instance
	{
		FScopeLock Lock(&CachedTextCS);
	
		const FText* FoundText = CachedText.Find(InTextId);
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
	FText NewText = FText(InTextLiteral, InTextId.GetNamespace(), InTextId.GetKey(), ETextFlag::Immutable);

	// ... and add it to the cache
	{
		FScopeLock Lock(&CachedTextCS);

		CachedText.Emplace(InTextId, NewText);
	}

	return NewText;
}

void FTextCache::RemoveCache(const FTextId& InTextId)
{
	return RemoveCache(MakeArrayView(&InTextId, 1));
}

void FTextCache::RemoveCache(TArrayView<const FTextId> InTextIds)
{
	FScopeLock Lock(&CachedTextCS);
	for (const FTextId& TextId : InTextIds)
	{
		CachedText.Remove(TextId);
	}
}

void FTextCache::RemoveCache(const TSet<FTextId>& InTextIds)
{
	FScopeLock Lock(&CachedTextCS);
	for (const FTextId& TextId : InTextIds)
	{
		CachedText.Remove(TextId);
	}
}
