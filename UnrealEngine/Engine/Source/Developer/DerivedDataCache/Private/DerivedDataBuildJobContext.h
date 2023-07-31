// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compression/CompressedBuffer.h"
#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "DerivedDataBuildFunction.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataRequest.h"
#include "DerivedDataSharedString.h"
#include "Experimental/Async/LazyEvent.h"
#include "Memory/MemoryFwd.h"
#include "Serialization/CompactBinary.h"
#include "Templates/Function.h"

namespace UE::DerivedData { class FBuildOutputBuilder; }
namespace UE::DerivedData { class IBuildJob; }
namespace UE::DerivedData { class IRequestOwner; }
namespace UE::DerivedData { enum class EBuildPolicy : uint32; }
namespace UE::DerivedData { enum class ECachePolicy : uint32; }
namespace UE::DerivedData { enum class EPriority : uint8; }

namespace UE::DerivedData::Private
{

class FBuildJobContext final : public FBuildContext, public FBuildConfigContext, public FRequestBase
{
public:
	FBuildJobContext(
		IBuildJob& Job,
		const FCacheKey& CacheKey,
		const IBuildFunction& Function,
		FBuildOutputBuilder& OutputBuilder);

	void BeginBuild(IRequestOwner& Owner, TUniqueFunction<void ()>&& OnEndBuild);

	const FSharedString& GetName() const final;

	inline const FCacheKey& GetCacheKey() const { return CacheKey; }

	inline ECachePolicy GetCachePolicyMask() const final { return CachePolicyMask; }
	inline EBuildPolicy GetBuildPolicyMask() const final { return BuildPolicyMask; }
	inline uint64 GetRequiredMemory() const { return RequiredMemory; }
	inline bool ShouldCheckDeterministicOutput() const { return bDeterministicOutputCheck; }

	void AddConstant(FUtf8StringView Key, FCbObject&& Value);
	void AddInput(FUtf8StringView Key, const FCompressedBuffer& Value);

	void AddError(FStringView Message) final;
	void AddWarning(FStringView Message) final;
	void AddMessage(FStringView Message) final;

	void ResetInputs();

private:
	FCbObject FindConstant(FUtf8StringView Key) const final;
	FSharedBuffer FindInput(FUtf8StringView Key) const final;

	void AddValue(const FValueId& Id, const FValue& Value) final;
	void AddValue(const FValueId& Id, const FCompressedBuffer& Buffer) final;
	void AddValue(const FValueId& Id, const FCompositeBuffer& Buffer, uint64 BlockSize) final;
	void AddValue(const FValueId& Id, const FSharedBuffer& Buffer, uint64 BlockSize) final;
	void AddValue(const FValueId& Id, const FCbObject& Object) final;

	void EndBuild();
	void BeginAsyncBuild() final;
	void EndAsyncBuild() final;

	void SetCacheBucket(FCacheBucket Bucket) final;
	void SetCachePolicyMask(ECachePolicy Policy) final;
	void SetBuildPolicyMask(EBuildPolicy Policy) final;
	void SetRequiredMemory(uint64 InRequiredMemory) final { RequiredMemory = InRequiredMemory; }
	void SkipDeterministicOutputCheck() final { bDeterministicOutputCheck = false; }

	void SetPriority(EPriority Priority) final;
	void Cancel() final;
	void Wait() final;

private:
	IBuildJob& Job;
	FCacheKey CacheKey;
	const IBuildFunction& Function;
	FBuildOutputBuilder& OutputBuilder;
	TMap<FUtf8SharedString, FCbObject> Constants;
	TMap<FUtf8SharedString, FCompressedBuffer> Inputs;
	UE::FLazyEvent BuildCompleteEvent{EEventMode::ManualReset};
	TUniqueFunction<void ()> OnEndBuild;
	IRequestOwner* Owner{};
	uint64 RequiredMemory{};
	ECachePolicy CachePolicyMask;
	EBuildPolicy BuildPolicyMask;
	bool bIsAsyncBuild{false};
	bool bIsAsyncBuildComplete{false};
	bool bDeterministicOutputCheck{true};
};

} // UE::DerivedData::Private
