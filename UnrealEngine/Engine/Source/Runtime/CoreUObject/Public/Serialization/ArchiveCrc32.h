// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/MemoryLayout.h"
#include "Templates/EnableIf.h"
#include "Templates/Models.h"

class FArchive;
class UObject;
struct CStaticStructProvider;

/**
* Calculates a checksum from the input provided to the archive.
*/
class FArchiveCrc32 : public FArchiveUObject
{
public:
	/**
	* Default constructor.
	*/
	COREUOBJECT_API FArchiveCrc32(uint32 InCRC = 0, UObject* InRootObject = nullptr);

	/**
	 * @return The checksum computed so far.
	 */
	uint32 GetCrc() const { return CRC; }

	//~ Begin FArchive Interface
	COREUOBJECT_API virtual void Serialize(void* Data, int64 Num) override;
	COREUOBJECT_API virtual FArchive& operator<<(class FName& Name);
	COREUOBJECT_API virtual FArchive& operator<<(class UObject*& Object);
	virtual FString GetArchiveName() const { return TEXT("FArchiveCrc32"); }
	//~ End FArchive Interface

private:
	uint32 CRC;
	UObject* RootObject;
};

/**
	* Serializes a USTRUCT value from or into an archive.
	*
	* @param Ar The archive to serialize from or to.
	* @param Value The value to serialize.
	*/
template <typename StructType>
FORCEINLINE typename TEnableIf<TModels_V<CStaticStructProvider, StructType>, FArchive&>::Type operator <<(FArchive& Ar, const StructType& Value)
{
	StructType* MutableValue = const_cast<StructType*>(&Value);
	StructType::StaticStruct()->SerializeItem(Ar, MutableValue, nullptr);
	return Ar;
}
