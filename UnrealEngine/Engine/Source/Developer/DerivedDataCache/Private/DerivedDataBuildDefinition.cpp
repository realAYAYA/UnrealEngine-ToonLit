// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildDefinition.h"

#include "Algo/AllOf.h"
#include "Containers/Map.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "DerivedDataBuildKey.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataSharedString.h"
#include "Misc/Guid.h"
#include "Misc/StringBuilder.h"
#include "Misc/TVariant.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include <atomic>

namespace UE::DerivedData::Private
{

class FBuildDefinitionBuilderInternal final : public IBuildDefinitionBuilderInternal
{
public:
	inline FBuildDefinitionBuilderInternal(const FSharedString& InName, const FUtf8SharedString& InFunction)
		: Name(InName)
		, Function(InFunction)
	{
		checkf(!Name.IsEmpty(), TEXT("A build definition requires a non-empty name."));
		AssertValidBuildFunctionName(Function, Name);
	}

	~FBuildDefinitionBuilderInternal() final = default;

	void AddConstant(FUtf8StringView Key, const FCbObject& Value) final
	{
		Add<FCbObject>(Key, Value);
	}

	void AddInputBuild(FUtf8StringView Key, const FBuildValueKey& ValueKey) final
	{
		Add<FBuildValueKey>(Key, ValueKey);
	}

	void AddInputBulkData(FUtf8StringView Key, const FGuid& BulkDataId) final
	{
		Add<FGuid>(Key, BulkDataId);
	}

	void AddInputFile(FUtf8StringView Key, FUtf8StringView Path) final
	{
		Add<FUtf8SharedString>(Key, Path);
	}

	void AddInputHash(FUtf8StringView Key, const FIoHash& RawHash) final
	{
		Add<FIoHash>(Key, RawHash);
	}

	FBuildDefinition Build() final;

	using InputType = TVariant<FCbObject, FBuildValueKey, FGuid, FUtf8SharedString, FIoHash>;

	FSharedString Name;
	FUtf8SharedString Function;
	TMap<FUtf8SharedString, InputType> Inputs;

private:
	template <typename ValueType, typename ArgType>
	inline void Add(FUtf8StringView Key, ArgType&& Value)
	{
		const uint32 KeyHash = GetTypeHash(Key);
		checkf(!Key.IsEmpty(),
			TEXT("Empty key used in definition for build of '%s' by %s."),
			*Name, *WriteToString<32>(Function));
		checkf(!Inputs.ContainsByHash(KeyHash, Key),
			TEXT("Duplicate key '%s' used in definition for build of '%s' by %s."),
			*WriteToString<64>(Key), *Name, *WriteToString<32>(Function));
		Inputs.EmplaceByHash(KeyHash, Key, InputType(TInPlaceType<ValueType>(), Forward<ArgType>(Value)));
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildDefinitionInternal final : public IBuildDefinitionInternal
{
public:
	explicit FBuildDefinitionInternal(FBuildDefinitionBuilderInternal&& DefinitionBuilder);
	explicit FBuildDefinitionInternal(const FSharedString& Name, FCbObject&& Definition, bool& bOutIsValid);

	~FBuildDefinitionInternal() final = default;

	const FBuildKey& GetKey() const final { return Key; }

	const FSharedString& GetName() const final { return Name; }
	const FUtf8SharedString& GetFunction() const final { return Function; }

	bool HasConstants() const final;
	bool HasInputs() const final;

	void IterateConstants(TFunctionRef<void (FUtf8StringView Key, FCbObject&& Value)> Visitor) const final;
	void IterateInputBuilds(TFunctionRef<void (FUtf8StringView Key, const FBuildValueKey& ValueKey)> Visitor) const final;
	void IterateInputBulkData(TFunctionRef<void (FUtf8StringView Key, const FGuid& BulkDataId)> Visitor) const final;
	void IterateInputFiles(TFunctionRef<void (FUtf8StringView Key, FUtf8StringView Path)> Visitor) const final;
	void IterateInputHashes(TFunctionRef<void (FUtf8StringView Key, const FIoHash& RawHash)> Visitor) const final;

	void Save(FCbWriter& Writer) const final;

	inline void AddRef() const final
	{
		ReferenceCount.fetch_add(1, std::memory_order_relaxed);
	}

	inline void Release() const final
	{
		if (ReferenceCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			delete this;
		}
	}

private:
	FSharedString Name;
	FUtf8SharedString Function;
	FCbObject Definition;
	FBuildKey Key;
	mutable std::atomic<uint32> ReferenceCount{0};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildDefinitionInternal::FBuildDefinitionInternal(FBuildDefinitionBuilderInternal&& DefinitionBuilder)
	: Name(MoveTemp(DefinitionBuilder.Name))
	, Function(MoveTemp(DefinitionBuilder.Function))
{
	DefinitionBuilder.Inputs.KeySort(TLess<>());

	bool bHasConstants = false;
	bool bHasBuilds = false;
	bool bHasBulkData = false;
	bool bHasFiles = false;
	bool bHasHashes = false;

	for (const TPair<FUtf8SharedString, FBuildDefinitionBuilderInternal::InputType>& Pair : DefinitionBuilder.Inputs)
	{
		if (Pair.Value.IsType<FCbObject>())
		{
			bHasConstants = true;
		}
		else if (Pair.Value.IsType<FBuildValueKey>())
		{
			bHasBuilds = true;
		}
		else if (Pair.Value.IsType<FGuid>())
		{
			bHasBulkData = true;
		}
		else if (Pair.Value.IsType<FUtf8SharedString>())
		{
			bHasFiles = true;
		}
		else if (Pair.Value.IsType<FIoHash>())
		{
			bHasHashes = true;
		}
	}

	const bool bHasInputs = bHasBuilds | bHasBulkData | bHasFiles | bHasHashes;

	TCbWriter<2048> Writer;
	Writer.BeginObject();
	Writer.AddString(ANSITEXTVIEW("Function"), Function);

	if (bHasConstants)
	{
		Writer.BeginObject(ANSITEXTVIEW("Constants"));
		for (const TPair<FUtf8SharedString, FBuildDefinitionBuilderInternal::InputType>& Pair : DefinitionBuilder.Inputs)
		{
			if (Pair.Value.IsType<FCbObject>())
			{
				Writer.AddObject(Pair.Key, Pair.Value.Get<FCbObject>());
			}
		}
		Writer.EndObject();
	}

	if (bHasInputs)
	{
		Writer.BeginObject(ANSITEXTVIEW("Inputs"));
	}

	if (bHasBuilds)
	{
		Writer.BeginObject(ANSITEXTVIEW("Builds"));
		for (const TPair<FUtf8SharedString, FBuildDefinitionBuilderInternal::InputType>& Pair : DefinitionBuilder.Inputs)
		{
			if (Pair.Value.IsType<FBuildValueKey>())
			{
				const FBuildValueKey& ValueKey = Pair.Value.Get<FBuildValueKey>();
				Writer.BeginObject(Pair.Key);
				Writer.AddHash(ANSITEXTVIEW("Build"), ValueKey.BuildKey.Hash);
				Writer.AddObjectId(ANSITEXTVIEW("Id"), ValueKey.Id);
				Writer.EndObject();
			}
		}
		Writer.EndObject();
	}

	if (bHasBulkData)
	{
		Writer.BeginObject(ANSITEXTVIEW("BulkData"));
		for (const TPair<FUtf8SharedString, FBuildDefinitionBuilderInternal::InputType>& Pair : DefinitionBuilder.Inputs)
		{
			if (Pair.Value.IsType<FGuid>())
			{
				Writer.AddUuid(Pair.Key, Pair.Value.Get<FGuid>());
			}
		}
		Writer.EndObject();
	}

	if (bHasFiles)
	{
		Writer.BeginObject(ANSITEXTVIEW("Files"));
		for (const TPair<FUtf8SharedString, FBuildDefinitionBuilderInternal::InputType>& Pair : DefinitionBuilder.Inputs)
		{
			if (Pair.Value.IsType<FUtf8SharedString>())
			{
				Writer.AddString(Pair.Key, Pair.Value.Get<FUtf8SharedString>());
			}
		}
		Writer.EndObject();
	}

	if (bHasHashes)
	{
		Writer.BeginObject(ANSITEXTVIEW("Hashes"));
		for (const TPair<FUtf8SharedString, FBuildDefinitionBuilderInternal::InputType>& Pair : DefinitionBuilder.Inputs)
		{
			if (Pair.Value.IsType<FIoHash>())
			{
				Writer.AddBinaryAttachment(Pair.Key, Pair.Value.Get<FIoHash>());
			}
		}
		Writer.EndObject();
	}

	if (bHasInputs)
	{
		Writer.EndObject();
	}

	Writer.EndObject();
	Definition = Writer.Save().AsObject();
	Key.Hash = Definition.GetHash();
}

FBuildDefinitionInternal::FBuildDefinitionInternal(const FSharedString& InName, FCbObject&& InDefinition, bool& bOutIsValid)
	: Name(InName)
	, Function(InDefinition.FindView(ANSITEXTVIEW("Function")).AsString())
	, Definition(MoveTemp(InDefinition))
	, Key{Definition.GetHash()}
{
	checkf(!Name.IsEmpty(), TEXT("A build definition requires a non-empty name."));
	Definition.MakeOwned();
	bOutIsValid = Definition
		&& IsValidBuildFunctionName(Function)
		&& Algo::AllOf(Definition.AsView()[ANSITEXTVIEW("Constants")],
			[](FCbFieldView Field) { return Field.GetName().Len() > 0 && Field.IsObject(); })
		&& Algo::AllOf(Definition.AsView()[ANSITEXTVIEW("Inputs")][ANSITEXTVIEW("Builds")], [](FCbFieldView Field)
			{
				return Field.GetName().Len() > 0 && Field.IsObject()
					&& Field[ANSITEXTVIEW("Build")].IsHash()
					&& Field[ANSITEXTVIEW("Id")].IsObjectId();
			})
		&& Algo::AllOf(Definition.AsView()[ANSITEXTVIEW("Inputs")][ANSITEXTVIEW("BulkData")],
			[](FCbFieldView Field) { return Field.GetName().Len() > 0 && Field.IsUuid(); })
		&& Algo::AllOf(Definition.AsView()[ANSITEXTVIEW("Inputs")][ANSITEXTVIEW("Files")],
			[](FCbFieldView Field) { return Field.GetName().Len() > 0 && Field.AsString().Len() > 0; })
		&& Algo::AllOf(Definition.AsView()[ANSITEXTVIEW("Inputs")][ANSITEXTVIEW("Hashes")],
			[](FCbFieldView Field) { return Field.GetName().Len() > 0 && Field.IsBinaryAttachment(); });
}

bool FBuildDefinitionInternal::HasConstants() const
{
	return Definition[ANSITEXTVIEW("Constants")].HasValue();
}

bool FBuildDefinitionInternal::HasInputs() const
{
	return Definition[ANSITEXTVIEW("Inputs")].HasValue();
}

void FBuildDefinitionInternal::IterateConstants(TFunctionRef<void (FUtf8StringView Key, FCbObject&& Value)> Visitor) const
{
	for (FCbField Field : Definition[ANSITEXTVIEW("Constants")])
	{
		Visitor(Field.GetName(), Field.AsObject());
	}
}

void FBuildDefinitionInternal::IterateInputBuilds(TFunctionRef<void (FUtf8StringView Key, const FBuildValueKey& ValueKey)> Visitor) const
{
	for (FCbFieldView Field : Definition.AsView()[ANSITEXTVIEW("Inputs")][ANSITEXTVIEW("Builds")])
	{
		const FBuildKey BuildKey{Field[ANSITEXTVIEW("Build")].AsHash()};
		const FValueId Id = Field[ANSITEXTVIEW("Id")].AsObjectId();
		Visitor(Field.GetName(), FBuildValueKey{BuildKey, Id});
	}
}

void FBuildDefinitionInternal::IterateInputBulkData(TFunctionRef<void (FUtf8StringView Key, const FGuid& BulkDataId)> Visitor) const
{
	for (FCbFieldView Field : Definition.AsView()[ANSITEXTVIEW("Inputs")][ANSITEXTVIEW("BulkData")])
	{
		Visitor(Field.GetName(), Field.AsUuid());
	}
}

void FBuildDefinitionInternal::IterateInputFiles(TFunctionRef<void (FUtf8StringView Key, FUtf8StringView Path)> Visitor) const
{
	for (FCbFieldView Field : Definition.AsView()[ANSITEXTVIEW("Inputs")][ANSITEXTVIEW("Files")])
	{
		Visitor(Field.GetName(), Field.AsString());
	}
}

void FBuildDefinitionInternal::IterateInputHashes(TFunctionRef<void (FUtf8StringView Key, const FIoHash& RawHash)> Visitor) const
{
	for (FCbFieldView Field : Definition.AsView()[ANSITEXTVIEW("Inputs")][ANSITEXTVIEW("Hashes")])
	{
		Visitor(Field.GetName(), Field.AsBinaryAttachment());
	}
}

void FBuildDefinitionInternal::Save(FCbWriter& Writer) const
{
	Writer.AddObject(Definition);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildDefinition FBuildDefinitionBuilderInternal::Build()
{
	return CreateBuildDefinition(new FBuildDefinitionInternal(MoveTemp(*this)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildDefinition CreateBuildDefinition(IBuildDefinitionInternal* Definition)
{
	return FBuildDefinition(Definition);
}

FBuildDefinitionBuilder CreateBuildDefinitionBuilder(IBuildDefinitionBuilderInternal* DefinitionBuilder)
{
	return FBuildDefinitionBuilder(DefinitionBuilder);
}

FBuildDefinitionBuilder CreateBuildDefinition(const FSharedString& Name, const FUtf8SharedString& Function)
{
	return CreateBuildDefinitionBuilder(new FBuildDefinitionBuilderInternal(Name, Function));
}

} // UE::DerivedData::Private

namespace UE::DerivedData
{

FOptionalBuildDefinition FBuildDefinition::Load(const FSharedString& Name, FCbObject&& Definition)
{
	bool bIsValid = false;
	FOptionalBuildDefinition Out = Private::CreateBuildDefinition(
		new Private::FBuildDefinitionInternal(Name, MoveTemp(Definition), bIsValid));
	if (!bIsValid)
	{
		Out.Reset();
	}
	return Out;
}

} // UE::DerivedData
