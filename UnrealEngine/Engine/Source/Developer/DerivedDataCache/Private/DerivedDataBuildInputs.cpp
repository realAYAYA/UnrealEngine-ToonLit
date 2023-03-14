// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildInputs.h"

#include "Compression/CompressedBuffer.h"
#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataSharedString.h"
#include "Misc/StringBuilder.h"
#include <atomic>

namespace UE::DerivedData::Private
{

class FBuildInputsBuilderInternal final : public IBuildInputsBuilderInternal
{
public:
	inline explicit FBuildInputsBuilderInternal(const FSharedString& InName)
		: Name(InName)
	{
		checkf(!Name.IsEmpty(), TEXT("Build inputs require a non-empty name."));
	}

	~FBuildInputsBuilderInternal() final = default;

	void AddInput(FUtf8StringView Key, const FCompressedBuffer& Buffer) final
	{
		const uint32 KeyHash = GetTypeHash(Key);
		checkf(Buffer, TEXT("Null buffer used in input for build of '%s'."), *Name);
		checkf(!Key.IsEmpty(), TEXT("Empty key used in input for build of '%s'."), *Name);
		checkf(!Inputs.ContainsByHash(KeyHash, Key),
			TEXT("Duplicate key '%s' used in input for build of '%s'."),
			*WriteToString<64>(Key), *Name);
		Inputs.EmplaceByHash(KeyHash, Key, Buffer);
	}

	FBuildInputs Build() final;

	FSharedString Name;
	TMap<FUtf8SharedString, FCompressedBuffer> Inputs;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildInputsInternal final : public IBuildInputsInternal
{
public:
	explicit FBuildInputsInternal(FBuildInputsBuilderInternal&& InputsBuilder);

	~FBuildInputsInternal() final = default;

	const FSharedString& GetName() const final { return Name; }

	const FCompressedBuffer& FindInput(FUtf8StringView Key) const final;
	void IterateInputs(TFunctionRef<void (FUtf8StringView Key, const FCompressedBuffer& Buffer)> Visitor) const final;

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
	TMap<FUtf8SharedString, FCompressedBuffer> Inputs;
	mutable std::atomic<uint32> ReferenceCount{0};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildInputsInternal::FBuildInputsInternal(FBuildInputsBuilderInternal&& InputsBuilder)
	: Name(MoveTemp(InputsBuilder.Name))
	, Inputs(MoveTemp(InputsBuilder.Inputs))
{
	Inputs.KeySort(TLess<>());
}

const FCompressedBuffer& FBuildInputsInternal::FindInput(FUtf8StringView Key) const
{
	if (const FCompressedBuffer* Buffer = Inputs.FindByHash(GetTypeHash(Key), Key))
	{
		return *Buffer;
	}
	return FCompressedBuffer::Null;
}

void FBuildInputsInternal::IterateInputs(TFunctionRef<void (FUtf8StringView Key, const FCompressedBuffer& Buffer)> Visitor) const
{
	for (const TPair<FUtf8SharedString, FCompressedBuffer>& Input : Inputs)
	{
		Input.ApplyAfter(Visitor);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildInputs FBuildInputsBuilderInternal::Build()
{
	return CreateBuildInputs(new FBuildInputsInternal(MoveTemp(*this)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildInputs CreateBuildInputs(IBuildInputsInternal* Inputs)
{
	return FBuildInputs(Inputs);
}

FBuildInputsBuilder CreateBuildInputsBuilder(IBuildInputsBuilderInternal* InputsBuilder)
{
	return FBuildInputsBuilder(InputsBuilder);
}

FBuildInputsBuilder CreateBuildInputs(const FSharedString& Name)
{
	return CreateBuildInputsBuilder(new FBuildInputsBuilderInternal(Name));
}

} // UE::DerivedData::Private
