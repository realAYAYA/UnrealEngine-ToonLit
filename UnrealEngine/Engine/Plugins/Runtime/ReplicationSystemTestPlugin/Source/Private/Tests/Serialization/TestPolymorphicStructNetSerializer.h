// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/Serialization/PolymorphicNetSerializer.h"
#include "UObject/ObjectMacros.h"
#include "TestPolymorphicStructNetSerializer.generated.h"

/**
* Polymorphic setup used in gamecode which we need to support
*/
USTRUCT()
struct FExamplePolymorphicArrayItem
{
	GENERATED_USTRUCT_BODY()

	virtual ~FExamplePolymorphicArrayItem() { }

	/** Returns the serialization data, must always be overridden */
	virtual UScriptStruct* GetScriptStruct() const
	{
		return FExamplePolymorphicArrayItem::StaticStruct();
	}
};

USTRUCT()
struct FExamplePolymorphicArrayStruct
{
	GENERATED_BODY()

	FExamplePolymorphicArrayStruct() { }

	FExamplePolymorphicArrayStruct(FExamplePolymorphicArrayItem* DataPtr)
	{
		Data.Add(TSharedPtr<FExamplePolymorphicArrayItem>(DataPtr));
	}

	FExamplePolymorphicArrayStruct(FExamplePolymorphicArrayStruct&& Other) : Data(MoveTemp(Other.Data))	{ }
	FExamplePolymorphicArrayStruct(const FExamplePolymorphicArrayStruct& Other) : Data(Other.Data) { }

	FExamplePolymorphicArrayStruct& operator=(FExamplePolymorphicArrayStruct&& Other) { Data = MoveTemp(Other.Data); return *this; }
	FExamplePolymorphicArrayStruct& operator=(const FExamplePolymorphicArrayStruct& Other) { Data = Other.Data; return *this; }

private:
	/** Raw storage of target data, do not modify this directly */
	TArray<TSharedPtr<FExamplePolymorphicArrayItem>, TInlineAllocator<1> >	Data;

public:
	/** Resets handle to have no targets */
	void Clear()
	{
		Data.Reset();
	}

	/** Returns number of target data, not number of actors/targets as target data may contain multiple actors */
	int32 Num() const
	{
		return Data.Num();
	}

	/** Returns true if there are any valid targets */
	bool IsValid(int32 Index) const
	{
		return (Index < Data.Num() && Data[Index].IsValid());
	}

	/** Returns data at index, or nullptr if invalid */
	const FExamplePolymorphicArrayItem* Get(int32 Index) const
	{
		return IsValid(Index) ? Data[Index].Get() : nullptr;
	}

	/** Returns data at index, or nullptr if invalid */
	FExamplePolymorphicArrayItem* Get(int32 Index)
	{
		return IsValid(Index) ? Data[Index].Get() : nullptr;
	}

	/** Adds a new target data to handle, it must have been created with new */
	void Add(FExamplePolymorphicArrayItem* DataPtr)
	{
		Data.Add(TSharedPtr<FExamplePolymorphicArrayItem>(DataPtr));
	}

	/** Does a shallow copy of target data from one handle to another */
	void Append(const FExamplePolymorphicArrayStruct& OtherHandle)
	{
		Data.Append(OtherHandle.Data);
	}

	/** Serialize for networking, handles polymorphism */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) { return true; };

	/** Comparison operator */
	bool operator==(const FExamplePolymorphicArrayStruct& Other) const
	{
		// Both invalid structs or both valid and Pointer compare (???) // deep comparison equality
		// From the original code this is odd, It should really compare the data instead as if we clone 
		if (Data.Num() != Other.Data.Num())
		{
			return false;
		}
		for (int32 i = 0; i < Data.Num(); ++i)
		{
			if (Data[i].IsValid() != Other.Data[i].IsValid())
			{
				return false;
			}
			if (Data[i].Get() != Other.Data[i].Get())
			{
				return false;
			}
		}
		return true;
	}

	/** Comparison operator */
	bool operator!=(const FExamplePolymorphicArrayStruct& Other) const
	{
		return !(FExamplePolymorphicArrayStruct::operator==(Other));
	}

	/** For PolymorphicStructArrayNetSerializer */
	static TArrayView<TSharedPtr<FExamplePolymorphicArrayItem>> GetArray(FExamplePolymorphicArrayStruct& ArrayContainer)
	{
		return MakeArrayView(ArrayContainer.Data);
	}

	static void SetArrayNum(FExamplePolymorphicArrayStruct& ArrayContainer, SIZE_T Num)
	{
		ArrayContainer.Data.SetNum(static_cast<SSIZE_T>(Num));
	}
};

template<>
struct TStructOpsTypeTraits<FExamplePolymorphicArrayStruct> : public TStructOpsTypeTraitsBase2<FExamplePolymorphicArrayStruct>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FExamplePolymorphicArrayItem> Data is copied around
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

// Some test structs
USTRUCT()
struct FExamplePolymorphicStructA : public FExamplePolymorphicArrayItem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 SomeInt;

	/** Returns the serialization data, must always be overridden */
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FExamplePolymorphicStructA::StaticStruct();
	}
};

USTRUCT()
struct FExamplePolymorphicStructB : public FExamplePolymorphicArrayItem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 SomeFloat;

	/** Returns the serialization data, must always be overridden */
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FExamplePolymorphicStructB::StaticStruct();
	}
};

USTRUCT()
struct FExamplePolymorphicStructC : public FExamplePolymorphicArrayItem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	bool SomeBool;

	UPROPERTY()
	TObjectPtr<UObject> SomeObjectRef = nullptr;

	/** Returns the serialization data, must always be overridden */
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FExamplePolymorphicStructC::StaticStruct();
	}
};

USTRUCT()
struct FExamplePolymorphicStructD : public FExamplePolymorphicArrayItem
{
	GENERATED_USTRUCT_BODY()

	uint32 SomeValue;

	/** Returns the serialization data, must always be overridden */
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FExamplePolymorphicStructD::StaticStruct();
	}
};

USTRUCT()
struct FExamplePolymorphicStructDNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestPolymorphicArrayStructNetSerializerConfig : public FPolymorphicArrayStructNetSerializerConfig
{
	GENERATED_USTRUCT_BODY()
};


namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER(FExamplePolymorphicStructDNetSerializer, REPLICATIONSYSTEMTESTPLUGIN_API);
UE_NET_DECLARE_SERIALIZER(FTestPolymorphicArrayStructNetSerializer, REPLICATIONSYSTEMTESTPLUGIN_API);

}
