// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "JsonObjectWrapper.generated.h"

class FArchive;
class FJsonObject;
class FOutputDevice;

/** UStruct that holds a JsonObject, can be used by structs passed to JsonObjectConverter to pass through JsonObjects directly */
USTRUCT(BlueprintType, meta = (DisplayName = "JsonObject"))
struct JSONUTILITIES_API FJsonObjectWrapper
{
	GENERATED_USTRUCT_BODY()
	
public:
	FJsonObjectWrapper();
	
	UPROPERTY(EditAnywhere, Category = "JSON")
	FString JsonString;

	TSharedPtr<FJsonObject> JsonObject;

	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);
	bool ExportTextItem(FString& ValueStr, FJsonObjectWrapper const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	void PostSerialize(const FArchive& Ar);

	explicit operator bool() const;

	bool JsonObjectToString(FString& Str) const;
	bool JsonObjectFromString(const FString& Str);
};

template<>
struct TStructOpsTypeTraits<FJsonObjectWrapper> : public TStructOpsTypeTraitsBase2<FJsonObjectWrapper>
{
	enum
	{
		WithImportTextItem = true,
		WithExportTextItem = true,
		WithPostSerialize = true,
	};
};
UCLASS()
class UJsonUtilitiesDummyObject : public UObject
{
	GENERATED_BODY()
};
