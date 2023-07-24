// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Async.h"
#include "Async/Future.h"
#include "Concepts/EqualityComparable.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FJsonValue;
class FName;
class FText;

namespace UE
{
	class EDITORCONFIG_API FJsonPath
	{
	public:
		struct FPart
		{
			FString Name;
			int32 Index = INDEX_NONE;
		};

		FJsonPath();
		FJsonPath(const TCHAR* Path);
		FJsonPath(FStringView Path);
		FJsonPath(const FJsonPath& Other);
		FJsonPath(FJsonPath&& Other);

		bool IsValid() const { return Length() > 0; }
		const int32 Length() const { return PathParts.Num(); }

		void Append(FStringView Name);
		void SetArrayIndex(int32 Index);

		FJsonPath GetSubPath(int32 NumParts) const;

		FString ToString() const;

		const FPart& operator[](int32 Idx) const { return PathParts[Idx]; }
		const TArray<FPart>& GetAll() const { return PathParts; }

	private:
		void ParsePath(const FString& InPath);

	private:
		TArray<FPart> PathParts;
	};

	using FJsonValuePair = TPair<TSharedPtr<FJsonValue>, TSharedPtr<FJsonValue>>;

	class EDITORCONFIG_API FJsonConfig
	{
	public:
		FJsonConfig();

		void SetParent(const TSharedPtr<FJsonConfig>& Parent);

		bool LoadFromFile(FStringView FilePath);
		bool LoadFromString(FStringView Content);
	
		bool SaveToFile(FStringView FilePath) const;
		bool SaveToString(FString& OutResult) const;

		bool IsValid() const { return MergedObject.IsValid(); }

		const FJsonConfig* GetParentConfig() const;

		template <typename T>
		bool TryGetNumber(const FJsonPath& Path, T& OutValue) const;
		bool TryGetBool(const FJsonPath& Path, bool& OutValue) const;
		bool TryGetString(const FJsonPath& Path, FString& OutValue) const;
		bool TryGetString(const FJsonPath& Path, FName& OutValue) const;
		bool TryGetString(const FJsonPath& Path, FText& OutValue) const;
		bool TryGetJsonValue(const FJsonPath& Path, TSharedPtr<FJsonValue>& OutValue) const;
		bool TryGetJsonObject(const FJsonPath& Path, TSharedPtr<FJsonObject>& OutValue) const;
		bool TryGetArray(const FJsonPath& Path, TArray<TSharedPtr<FJsonValue>>& OutArray) const;

		TSharedPtr<FJsonObject> GetRootObject() const;

		// these are specializations for arithmetic and string arrays
		// these could be templated with enable-ifs, 
		// but it ended up being more lines of incomprehensible template SFINAE than this clear list of types is
		bool TryGetArray(const FJsonPath& Path, TArray<bool>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<int8>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<int16>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<int32>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<int64>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<uint8>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<uint16>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<uint32>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<uint64>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<float>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<double>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<FString>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<FText>& OutArray) const;
		bool TryGetArray(const FJsonPath& Path, TArray<FName>& OutArray) const;

		bool TryGetMap(const FJsonPath& Path, TArray<FJsonValuePair>& OutMap) const;

		// try to set a number - returns false if:
		// - the path references an object that doesn't exist
		// - the path references an index in an array that doesn't exist
		template <typename T>
		bool SetNumber(const FJsonPath& Path, T Value);
		bool SetBool(const FJsonPath& Path, bool Value);
		bool SetString(const FJsonPath& Path, FStringView Value);
		bool SetString(const FJsonPath& Path, const FText& Value);
		bool SetJsonValue(const FJsonPath& Path, const TSharedPtr<FJsonValue>& Value);
		bool SetJsonObject(const FJsonPath& Path, const TSharedPtr<FJsonObject>& Object);
		bool SetJsonArray(const FJsonPath& Path, const TArray<TSharedPtr<FJsonValue>>& Array);

		bool SetRootObject(const TSharedPtr<FJsonObject>& Object);

		bool HasOverride(const FJsonPath& Path) const;

	private:

		template <typename T, typename TGetter>
		bool TryGetArrayHelper(const FJsonPath& Path, TArray<T>& OutArray, TGetter Getter) const;

		template <typename T>
		bool TryGetNumericArrayHelper(const FJsonPath& Path, TArray<T>& OutArray) const;

		bool SetJsonValueInMerged(const FJsonPath& Path, const TSharedPtr<FJsonValue>& Value);
		bool SetJsonValueInOverride(const FJsonPath& Path, const TSharedPtr<FJsonValue>& NewValue, const TSharedPtr<FJsonValue>& PreviousValue, const TSharedPtr<FJsonValue>& ParentValue);
		bool SetArrayValueInOverride(const TSharedPtr<FJsonValue>& CurrentValue, const TArray<TSharedPtr<FJsonValue>>& NewArray, const TSharedPtr<FJsonValue>& ParentValue);
		bool SetObjectValueInOverride(const TSharedPtr<FJsonObject>& CurrentObject, const TSharedPtr<FJsonObject>& NewObject, const TSharedPtr<FJsonValue>& ParentValue);

		bool RemoveJsonValueFromOverride(const FJsonPath& Path, const TSharedPtr<FJsonValue>& PreviousValue);

		bool MergeThisWithParent();
		void OnParentConfigChanged();

	private:

		TFuture<void> SaveFuture;

		FSimpleDelegate OnConfigChanged;

		TSharedPtr<FJsonConfig> ParentConfig;

		TSharedPtr<FJsonObject> OverrideObject;
		TSharedPtr<FJsonObject> MergedObject;
	};
}

#include "JsonConfig.inl" // IWYU pragma: export
