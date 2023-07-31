// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

template <typename ObjectType = UObject, typename = typename TEnableIf<TIsDerivedFrom<ObjectType, UObject>::Value>::Type>
class FGLTFObjectArrayScopeGuard : public FGCObject, public TArray<ObjectType*>
{
public:

	FGLTFObjectArrayScopeGuard()
	{
	}

	using TArray<ObjectType*>::TArray;

	/** Non-copyable */
	FGLTFObjectArrayScopeGuard(const FGLTFObjectArrayScopeGuard&) = delete;
	FGLTFObjectArrayScopeGuard& operator=(const FGLTFObjectArrayScopeGuard&) = delete;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObjects<ObjectType>(*this);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("FGLTFObjectArrayScopeGuard");
	}
};
