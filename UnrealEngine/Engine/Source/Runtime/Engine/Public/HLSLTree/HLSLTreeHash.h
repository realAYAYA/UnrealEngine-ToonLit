// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Containers/BitArray.h"
#include "Misc/StringBuilder.h"
#include "Hash/xxhash.h"
#include "HLSLTree/HLSLTreeTypes.h"

namespace UE::HLSLTree
{

class FHasher
{
public:
	FXxHash64 Finalize() { return Builder.Finalize(); }
	void AppendData(const void* Data, uint64 Size) { Builder.Update(Data, Size); }

private:
	FXxHash64Builder Builder;
};

template<typename T>
inline void AppendHash(FHasher& Hasher, const T& Value)
{
	Hasher.AppendData(&Value, sizeof(Value));
}

template<typename T>
inline void AppendHash(FHasher& Hasher, TArrayView<T> Value)
{
	for (const T& Element : Value)
	{
		AppendHash(Hasher, Element);
	}
}

template<typename Allocator>
inline void AppendHash(FHasher& Hasher, const TBitArray<Allocator>& Value)
{
	const uint32 NumWords = FBitSet::CalculateNumWords(Value.Num());
	const uint32* Data = Value.GetData();
	Hasher.AppendData(Data, NumWords * sizeof(uint32));
}

inline void AppendHash(FHasher& Hasher, const FName& Value)
{
	AppendHash(Hasher, Value.GetComparisonIndex());
	AppendHash(Hasher, Value.GetNumber());
}

inline void AppendHash(FHasher& Hasher, FStringView Value)
{
	Hasher.AppendData(Value.GetData(), Value.Len() * sizeof(TCHAR));
}

inline void AppendHash(FHasher& Hasher, const FString& Value)
{
	Hasher.AppendData(&Value[0], Value.Len() * sizeof(TCHAR));
}

inline void AppendHash(FHasher& Hasher, const TCHAR* Value)
{
	Hasher.AppendData(Value, FCString::Strlen(Value) * sizeof(TCHAR));
}

inline void AppendHash(FHasher& Hasher, const FCustomHLSLInput& Value)
{
	AppendHash(Hasher, Value.Name);
	AppendHash(Hasher, Value.Expression);
}

inline void AppendHash(FHasher& Hasher, const Shader::FType& Type)
{
	if (Type.IsStruct()) AppendHash(Hasher, Type.StructType);
	else if (Type.IsObject()) AppendHash(Hasher, Type.ObjectType);
	else AppendHash(Hasher, Type.ValueType);
}

inline void AppendHash(FHasher& Hasher, const Shader::FValue& Value)
{
	AppendHash(Hasher, Value.Type);
	for (int32 i = 0; i < Value.GetNumComponents(); ++i)
	{
		AppendHash(Hasher, Value.GetComponent(i));
	}
}

inline void AppendHashes(FHasher& Hasher) {}

template<typename T, typename... ArgTypes>
inline void AppendHashes(FHasher& Hasher, const T& Value, ArgTypes&&... Args)
{
	AppendHash(Hasher, Value);
	AppendHashes(Hasher, Forward<ArgTypes>(Args)...);
}

template<typename T>
inline FXxHash64 HashValue(const T& Value)
{
	FHasher Hasher;
	AppendHash(Hasher, Value);
	return Hasher.Finalize();
}

} // namespace UE::HLSLTree

#endif // WITH_EDITOR
