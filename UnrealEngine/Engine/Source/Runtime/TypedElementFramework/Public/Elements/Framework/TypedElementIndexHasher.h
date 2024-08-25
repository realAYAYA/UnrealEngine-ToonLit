// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace TypedElementDataStorage
{
	template<typename T>
	IndexHash GenerateIndexHash(const T* Object);
	
	template<typename T>
	IndexHash GenerateIndexHash(const TWeakObjectPtr<T>& Object);
	template<typename T>
	IndexHash GenerateIndexHash(const TObjectPtr<T>& Object);
	template<typename T>
	IndexHash GenerateIndexHash(const TStrongObjectPtr<T>& Object);

	inline IndexHash GenerateIndexHash(const FString& Object);
	inline IndexHash GenerateIndexHash(FName Object);
} // namespace TypedElementDataStorage

#include "Elements/Framework/TypedElementIndexHasher.inl"