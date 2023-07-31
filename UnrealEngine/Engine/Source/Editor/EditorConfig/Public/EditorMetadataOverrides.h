// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "EditorConfig.h"
#include "EditorSubsystem.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "EditorMetadataOverrides.generated.h"

class FEditorConfig;
class FField;
class FSubsystemCollectionBase;
class UClass;
class UObject;
class UStruct;

USTRUCT()
struct FMetadataSet
{
	GENERATED_BODY()

	// map of metadata key to metadata value
	UPROPERTY()
	TMap<FName, FString> Strings;
		
	UPROPERTY()
	TMap<FName, bool> Bools;

	UPROPERTY()
	TMap<FName, int32> Ints;
		
	UPROPERTY()
	TMap<FName, float> Floats;
};

USTRUCT()
struct FStructMetadata
{
	GENERATED_BODY()

	// map of field name to field metadata
	UPROPERTY()
	TMap<FName, FMetadataSet> Fields;

	UPROPERTY()
	FMetadataSet StructMetadata;
};

USTRUCT()
struct FMetadataConfig
{
	GENERATED_BODY()
		
	// map of class name to class metadata
	UPROPERTY()
	TMap<FName, FStructMetadata> Classes;
};

UENUM()
enum class EMetadataType
{
	None,
	Bool,
	Int,
	Float,
	String
};

UCLASS()
class EDITORCONFIG_API UEditorMetadataOverrides : 
	public UEditorSubsystem
{ 
	GENERATED_BODY()

public:
	UEditorMetadataOverrides();
	virtual ~UEditorMetadataOverrides() {}

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool LoadFromConfig(TSharedPtr<FEditorConfig> Config);
	void Save();

	EMetadataType GetMetadataType(const FField* Field, FName Key) const;
	EMetadataType GetMetadataType(const UStruct* Struct, FName Key) const;

	bool GetStringMetadata(const FField* Field, FName Key, FString& OutValue) const;
	void SetStringMetadata(const FField* Field, FName Key, FStringView Value);

	bool GetFloatMetadata(const FField* Field, FName Key, float& OutValue) const;
	void SetFloatMetadata(const FField* Field, FName Key, float Value);

	bool GetIntMetadata(const FField* Field, FName Key, int32& OutValue) const;
	void SetIntMetadata(const FField* Field, FName Key, int32 Value);

	bool GetBoolMetadata(const FField* Field, FName Key, bool& OutValue) const;
	void SetBoolMetadata(const FField* Field, FName Key, bool Value);

	bool GetClassMetadata(const FField* Field, FName Key, UClass*& OutValue) const;
	void SetClassMetadata(const FField* Field, FName Key, UClass* Value);

	bool GetArrayMetadata(const FField* Field, FName Key, TArray<FString>& OutValue) const;
	void SetArrayMetadata(const FField* Field, FName Key, const TArray<FString>& Value);

	void AddToArrayMetadata(const FField* Field, FName Key, const FString& Value);
	void RemoveFromArrayMetadata(const FField* Field, FName Key, const FString& Value);

	void RemoveMetadata(const FField* Field, FName Key);

	bool GetStringMetadata(const UStruct* Struct, FName Key, FString& OutValue) const;
	void SetStringMetadata(const UStruct* Struct, FName Key, FStringView Value);

	bool GetFloatMetadata(const UStruct* Struct, FName Key, float& OutValue) const;
	void SetFloatMetadata(const UStruct* Struct, FName Key, float Value);

	bool GetIntMetadata(const UStruct* Struct, FName Key, int32& OutValue) const;
	void SetIntMetadata(const UStruct* Struct, FName Key, int32 Value);

	bool GetBoolMetadata(const UStruct* Struct, FName Key, bool& OutValue) const;
	void SetBoolMetadata(const UStruct* Struct, FName Key, bool Value);

	bool GetClassMetadata(const UStruct* Struct, FName Key, UClass*& OutValue) const;
	void SetClassMetadata(const UStruct* Struct, FName Key, UClass* Value);

	bool GetArrayMetadata(const UStruct* Struct, FName Key, TArray<FString>& OutValue) const;
	void SetArrayMetadata(const UStruct* Struct, FName Key, const TArray<FString>& Value);

	void AddToArrayMetadata(const UStruct* Struct, FName Key, const FString& Value);
	void RemoveFromArrayMetadata(const UStruct* Struct, FName Key, const FString& Value);

	void RemoveMetadata(const UStruct* Struct, FName Key);

private:
	const FMetadataSet* FindFieldMetadata(const FField* Field) const;
	FMetadataSet* FindFieldMetadata(const FField* Field);
	FMetadataSet* FindOrAddFieldMetadata(const FField* Field);

	const FMetadataSet* FindStructMetadata(const UStruct* Struct) const;
	FMetadataSet* FindStructMetadata(const UStruct* Struct);
	FMetadataSet* FindOrAddStructMetadata(const UStruct* Struct);

	void OnCompleted(bool bSuccess);

private:
	TSharedPtr<FEditorConfig> SourceConfig;
	FMetadataConfig LoadedMetadata;
};
