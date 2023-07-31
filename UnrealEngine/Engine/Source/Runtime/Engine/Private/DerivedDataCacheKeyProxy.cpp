// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCacheKeyProxy.h"

#if WITH_EDITORONLY_DATA

#include "DerivedDataCacheKey.h"

namespace UE::DerivedData
{

static_assert(sizeof(FCacheKeyProxy) == sizeof(FCacheKeyProxy));
static_assert(alignof(FCacheKeyProxy) == alignof(FCacheKeyProxy));

FCacheKeyProxy::FCacheKeyProxy(const FCacheKey& InKey)
{
	new(AsCacheKey()) FCacheKey(InKey);
}

FCacheKeyProxy::~FCacheKeyProxy()
{
	AsCacheKey()->~FCacheKey();
}

} // UE::DerivedData

#endif // WITH_EDITORONLY_DATA