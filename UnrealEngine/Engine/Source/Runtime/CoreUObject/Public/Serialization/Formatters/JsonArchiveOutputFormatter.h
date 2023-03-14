// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Serialization/StructuredArchiveFormatter.h"
#include "Serialization/StructuredArchiveNameHelpers.h"
#include "UObject/ObjectResource.h"

class FArchive;
class FName;
class FPackageIndex;
class FText;
class UObject;
struct FLazyObjectPtr;
struct FObjectPtr;
struct FSoftObjectPath;
struct FSoftObjectPtr;
struct FWeakObjectPtr;

#if WITH_TEXT_ARCHIVE_SUPPORT

class COREUOBJECT_API FJsonArchiveOutputFormatter final : public FStructuredArchiveFormatter
{
public:
	FJsonArchiveOutputFormatter(FArchive& InInner);
	virtual ~FJsonArchiveOutputFormatter();

	virtual FArchive& GetUnderlyingArchive() override;

	virtual bool HasDocumentTree() const override;

	virtual void EnterRecord() override;
	virtual void LeaveRecord() override;
	virtual void EnterField(FArchiveFieldName Name) override;
	virtual void LeaveField() override;
	virtual bool TryEnterField(FArchiveFieldName Name, bool bEnterWhenSaving) override;

	virtual void EnterArray(int32& NumElements) override;
	virtual void LeaveArray() override;
	virtual void EnterArrayElement() override;
	virtual void LeaveArrayElement() override;

	virtual void EnterStream() override;
	virtual void LeaveStream() override;
	virtual void EnterStreamElement() override;
	virtual void LeaveStreamElement() override;

	virtual void EnterMap(int32& NumElements) override;
	virtual void LeaveMap() override;
	virtual void EnterMapElement(FString& Name) override;
	virtual void LeaveMapElement() override;

	virtual void EnterAttributedValue() override;
	virtual void EnterAttribute(FArchiveFieldName AttributeName) override;
	virtual void EnterAttributedValueValue() override;
	virtual void LeaveAttribute() override;
	virtual void LeaveAttributedValue() override;
	virtual bool TryEnterAttribute(FArchiveFieldName AttributeName, bool bEnterWhenSaving) override;

	virtual bool TryEnterAttributedValueValue() override;

	virtual void Serialize(uint8& Value) override;
	virtual void Serialize(uint16& Value) override;
	virtual void Serialize(uint32& Value) override;
	virtual void Serialize(uint64& Value) override;
	virtual void Serialize(int8& Value) override;
	virtual void Serialize(int16& Value) override;
	virtual void Serialize(int32& Value) override;
	virtual void Serialize(int64& Value) override;
	virtual void Serialize(float& Value) override;
	virtual void Serialize(double& Value) override;
	virtual void Serialize(bool& Value) override;
	virtual void Serialize(FString& Value) override;
	virtual void Serialize(FName& Value) override;
	virtual void Serialize(UObject*& Value) override;
	virtual void Serialize(FText& Value) override;
	virtual void Serialize(FWeakObjectPtr& Value) override;
	virtual void Serialize(FSoftObjectPtr& Value) override;
	virtual void Serialize(FSoftObjectPath& Value) override;
	virtual void Serialize(FLazyObjectPtr& Value) override;
	virtual void Serialize(FObjectPtr& Value) override;
	virtual void Serialize(TArray<uint8>& Value) override;
	virtual void Serialize(void* Data, uint64 DataSize) override;

	void SetObjectIndicesMap(const TMap<UObject*, FPackageIndex>* InObjectIndicesMap)
	{
		ObjectIndicesMap = InObjectIndicesMap;
	}

private:
	FArchive& Inner;

	const TMap<UObject*, FPackageIndex>* ObjectIndicesMap = nullptr;

	TArray<ANSICHAR> Newline;
	bool bNeedsComma   = false;
	bool bNeedsNewline = false;

	TArray<int32> NumAttributesStack;
	TArray<int64> TextStartPosStack;

	void Write(ANSICHAR Character);

	void Write(const ANSICHAR* Text);
	void Write(const FString& Text);

	void WriteFieldName(const TCHAR* Name);
	void WriteValue(const FString& Value);

	void WriteOptionalComma();
	void WriteOptionalNewline();
	void WriteOptionalAttributedBlockOpening();
	void WriteOptionalAttributedBlockValue();
	void WriteOptionalAttributedBlockClosing();

	void SerializeStringInternal(const FString& String);

	bool IsObjectAllowed(UObject* InObject) const;
};

#endif
