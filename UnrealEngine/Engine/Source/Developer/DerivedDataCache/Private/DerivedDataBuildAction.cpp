// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildAction.h"

#include "Algo/AllOf.h"
#include "Containers/Map.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "DerivedDataBuildKey.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataSharedString.h"
#include "IO/IoHash.h"
#include "Misc/Guid.h"
#include "Misc/StringBuilder.h"
#include "Misc/TVariant.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Templates/Tuple.h"
#include <atomic>

namespace UE::DerivedData::Private
{

class FBuildActionBuilderInternal final : public IBuildActionBuilderInternal
{
public:
	inline FBuildActionBuilderInternal(
		const FSharedString& InName,
		const FUtf8SharedString& InFunction,
		const FGuid& InFunctionVersion,
		const FGuid& InBuildSystemVersion)
		: Name(InName)
		, Function(InFunction)
		, FunctionVersion(InFunctionVersion)
		, BuildSystemVersion(InBuildSystemVersion)
	{
		checkf(!Name.IsEmpty(), TEXT("A build action requires a non-empty name."));
		AssertValidBuildFunctionName(Function, Name);
	}

	~FBuildActionBuilderInternal() final = default;

	void AddConstant(FUtf8StringView Key, const FCbObject& Value) final
	{
		Add(Key, Value);
	}

	void AddInput(FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize) final
	{
		Add(Key, MakeTuple(RawHash, RawSize));
	}

	FBuildAction Build() final;

	using InputType = TVariant<FCbObject, TTuple<FIoHash, uint64>>;

	FSharedString Name;
	FUtf8SharedString Function;
	FGuid FunctionVersion;
	FGuid BuildSystemVersion;
	TMap<FUtf8SharedString, InputType> Inputs;

private:
	template <typename ValueType>
	inline void Add(FUtf8StringView Key, const ValueType& Value)
	{
		const uint32 KeyHash = GetTypeHash(Key);
		checkf(!Key.IsEmpty(),
			TEXT("Empty key used in action for build of '%s' by %s."),
			*Name, *WriteToString<32>(Function));
		checkf(!Inputs.ContainsByHash(KeyHash, Key),
			TEXT("Duplicate key '%s' used in action for build of '%s' by %s."),
			*WriteToString<64>(Key), *Name, *WriteToString<32>(Function));
		Inputs.EmplaceByHash(KeyHash, Key, InputType(TInPlaceType<ValueType>(), Value));
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildActionInternal final : public IBuildActionInternal
{
public:
	explicit FBuildActionInternal(FBuildActionBuilderInternal&& ActionBuilder);
	explicit FBuildActionInternal(const FSharedString& Name, FCbObject&& Action, bool& bOutIsValid);

	~FBuildActionInternal() final = default;

	const FBuildActionKey& GetKey() const final { return Key; }

	const FSharedString& GetName() const final { return Name; }
	const FUtf8SharedString& GetFunction() const final { return Function; }
	const FGuid& GetFunctionVersion() const final { return FunctionVersion; }
	const FGuid& GetBuildSystemVersion() const final { return BuildSystemVersion; }

	bool HasConstants() const final;
	bool HasInputs() const final;

	void IterateConstants(TFunctionRef<void (FUtf8StringView Key, FCbObject&& Value)> Visitor) const final;
	void IterateInputs(TFunctionRef<void (FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)> Visitor) const final;

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
	FGuid FunctionVersion;
	FGuid BuildSystemVersion;
	FCbObject Action;
	FBuildActionKey Key;
	mutable std::atomic<uint32> ReferenceCount{0};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildActionInternal::FBuildActionInternal(FBuildActionBuilderInternal&& ActionBuilder)
	: Name(MoveTemp(ActionBuilder.Name))
	, Function(MoveTemp(ActionBuilder.Function))
	, FunctionVersion(MoveTemp(ActionBuilder.FunctionVersion))
	, BuildSystemVersion(MoveTemp(ActionBuilder.BuildSystemVersion))
{
	ActionBuilder.Inputs.KeySort(TLess<>());

	bool bHasConstants = false;
	bool bHasInputs = false;

	for (const TPair<FUtf8SharedString, FBuildActionBuilderInternal::InputType>& Pair : ActionBuilder.Inputs)
	{
		if (Pair.Value.IsType<FCbObject>())
		{
			bHasConstants = true;
		}
		else if (Pair.Value.IsType<TTuple<FIoHash, uint64>>())
		{
			bHasInputs = true;
		}
	}

	TCbWriter<2048> Writer;
	Writer.BeginObject();
	Writer.AddString(ANSITEXTVIEW("Function"), Function);
	Writer.AddUuid(ANSITEXTVIEW("FunctionVersion"), FunctionVersion);
	Writer.AddUuid(ANSITEXTVIEW("BuildSystemVersion"), BuildSystemVersion);

	if (bHasConstants)
	{
		Writer.BeginObject(ANSITEXTVIEW("Constants"));
		for (const TPair<FUtf8SharedString, FBuildActionBuilderInternal::InputType>& Pair : ActionBuilder.Inputs)
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
		for (const TPair<FUtf8SharedString, FBuildActionBuilderInternal::InputType>& Pair : ActionBuilder.Inputs)
		{
			if (Pair.Value.IsType<TTuple<FIoHash, uint64>>())
			{
				const TTuple<FIoHash, uint64>& Input = Pair.Value.Get<TTuple<FIoHash, uint64>>();
				Writer.BeginObject(Pair.Key);
				Writer.AddBinaryAttachment(ANSITEXTVIEW("RawHash"), Input.Get<FIoHash>());
				Writer.AddInteger(ANSITEXTVIEW("RawSize"), Input.Get<uint64>());
				Writer.EndObject();
			}
		}
		Writer.EndObject();
	}

	Writer.EndObject();
	Action = Writer.Save().AsObject();
	Key.Hash = Action.GetHash();
}

FBuildActionInternal::FBuildActionInternal(const FSharedString& InName, FCbObject&& InAction, bool& bOutIsValid)
	: Name(InName)
	, Function(InAction.FindView(ANSITEXTVIEW("Function")).AsString())
	, FunctionVersion(InAction.FindView(ANSITEXTVIEW("FunctionVersion")).AsUuid())
	, BuildSystemVersion(InAction.FindView(ANSITEXTVIEW("BuildSystemVersion")).AsUuid())
	, Action(MoveTemp(InAction))
	, Key{Action.GetHash()}
{
	checkf(!Name.IsEmpty(), TEXT("A build action requires a non-empty name."));
	Action.MakeOwned();
	bOutIsValid = Action
		&& IsValidBuildFunctionName(Function)
		&& FunctionVersion.IsValid()
		&& BuildSystemVersion.IsValid()
		&& Algo::AllOf(Action.AsView()[ANSITEXTVIEW("Constants")],
			[](FCbFieldView Field) { return Field.GetName().Len() > 0 && Field.IsObject(); })
		&& Algo::AllOf(Action.AsView()[ANSITEXTVIEW("Inputs")], [](FCbFieldView Field)
			{
				return Field.GetName().Len() > 0 && Field.IsObject()
					&& Field[ANSITEXTVIEW("RawHash")].IsBinaryAttachment()
					&& Field[ANSITEXTVIEW("RawSize")].IsInteger();
			});
}

bool FBuildActionInternal::HasConstants() const
{
	return Action[ANSITEXTVIEW("Constants")].HasValue();
}

bool FBuildActionInternal::HasInputs() const
{
	return Action[ANSITEXTVIEW("Inputs")].HasValue();
}

void FBuildActionInternal::IterateConstants(TFunctionRef<void (FUtf8StringView Key, FCbObject&& Value)> Visitor) const
{
	for (FCbField Field : Action[ANSITEXTVIEW("Constants")])
	{
		Visitor(Field.GetName(), Field.AsObject());
	}
}

void FBuildActionInternal::IterateInputs(TFunctionRef<void (FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)> Visitor) const
{
	for (FCbFieldView Field : Action.AsView()[ANSITEXTVIEW("Inputs")])
	{
		Visitor(Field.GetName(), Field[ANSITEXTVIEW("RawHash")].AsHash(), Field[ANSITEXTVIEW("RawSize")].AsUInt64());
	}
}

void FBuildActionInternal::Save(FCbWriter& Writer) const
{
	Writer.AddObject(Action);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildAction FBuildActionBuilderInternal::Build()
{
	return CreateBuildAction(new FBuildActionInternal(MoveTemp(*this)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildAction CreateBuildAction(IBuildActionInternal* Action)
{
	return FBuildAction(Action);
}

FBuildActionBuilder CreateBuildActionBuilder(IBuildActionBuilderInternal* ActionBuilder)
{
	return FBuildActionBuilder(ActionBuilder);
}

FBuildActionBuilder CreateBuildAction(
	const FSharedString& Name,
	const FUtf8SharedString& Function,
	const FGuid& FunctionVersion,
	const FGuid& BuildSystemVersion)
{
	return CreateBuildActionBuilder(new FBuildActionBuilderInternal(Name, Function, FunctionVersion, BuildSystemVersion));
}

} // UE::DerivedData::Private

namespace UE::DerivedData
{

FOptionalBuildAction FBuildAction::Load(const FSharedString& Name, FCbObject&& Action)
{
	bool bIsValid = false;
	FOptionalBuildAction Out = Private::CreateBuildAction(
		new Private::FBuildActionInternal(Name, MoveTemp(Action), bIsValid));
	if (!bIsValid)
	{
		Out.Reset();
	}
	return Out;
}

} // UE::DerivedData
