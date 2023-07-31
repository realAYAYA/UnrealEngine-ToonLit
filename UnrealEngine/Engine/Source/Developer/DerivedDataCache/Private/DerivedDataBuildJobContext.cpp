// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildJobContext.h"

#include "Containers/StringConv.h"
#include "DerivedDataBuildJob.h"
#include "DerivedDataBuildOutput.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataBuildTypes.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataValue.h"
#include "Hash/Blake3.h"
#include "Misc/StringBuilder.h"
#include "UObject/NameTypes.h"

namespace UE::DerivedData::Private
{

FBuildJobContext::FBuildJobContext(
	IBuildJob& InJob,
	const FCacheKey& InCacheKey,
	const IBuildFunction& InFunction,
	FBuildOutputBuilder& InOutputBuilder)
	: Job(InJob)
	, CacheKey(InCacheKey)
	, Function(InFunction)
	, OutputBuilder(InOutputBuilder)
	, CachePolicyMask(~ECachePolicy::None)
	, BuildPolicyMask(~EBuildPolicy::None)
{
}

const FSharedString& FBuildJobContext::GetName() const
{
	return Job.GetName();
}

void FBuildJobContext::AddConstant(FUtf8StringView Key, FCbObject&& Value)
{
	Constants.EmplaceByHash(GetTypeHash(Key), Key, MoveTemp(Value));
}

void FBuildJobContext::AddInput(FUtf8StringView Key, const FCompressedBuffer& Value)
{
	Inputs.EmplaceByHash(GetTypeHash(Key), Key, Value);
}

void FBuildJobContext::AddError(FStringView Message)
{
	OutputBuilder.AddMessage({FTCHARToUTF8(Message), EBuildOutputMessageLevel::Error});
}

void FBuildJobContext::AddWarning(FStringView Message)
{
	OutputBuilder.AddMessage({FTCHARToUTF8(Message), EBuildOutputMessageLevel::Warning});
}

void FBuildJobContext::AddMessage(FStringView Message)
{
	OutputBuilder.AddMessage({FTCHARToUTF8(Message), EBuildOutputMessageLevel::Display});
}

void FBuildJobContext::ResetInputs()
{
	Constants.Empty();
	Inputs.Empty();
}

FCbObject FBuildJobContext::FindConstant(FUtf8StringView Key) const
{
	if (const FCbObject* Object = Constants.FindByHash(GetTypeHash(Key), Key))
	{
		return *Object;
	}
	return FCbObject();
}

FSharedBuffer FBuildJobContext::FindInput(FUtf8StringView Key) const
{
	if (const FCompressedBuffer* Input = Inputs.FindByHash(GetTypeHash(Key), Key))
	{
		FSharedBuffer Buffer = Input->Decompress();
		const FBlake3Hash RawHash = FBlake3::HashBuffer(Buffer);
		if (RawHash == Input->GetRawHash() && Buffer.GetSize() == Input->GetRawSize())
		{
			return Buffer;
		}
		else
		{
			TStringBuilder<256> Error;
			Error << TEXT("Input '") << Key << TEXT("' was expected to have raw hash ") << Input->GetRawHash()
				<< TEXT(" and raw size ") << Input->GetRawSize() << TEXT(" but has raw hash ") << RawHash
				<< TEXT(" and raw size ") << Buffer.GetSize() << TEXT(" after decompression for build of '")
				<< Job.GetName() << TEXT("' by ") << Job.GetFunction() << TEXT(".");
			OutputBuilder.AddLog({ANSITEXTVIEW("LogDerivedDataBuild"), FTCHARToUTF8(Error), EBuildOutputLogLevel::Error});
			UE_LOG(LogDerivedDataBuild, Error, TEXT("%.*s"), Error.Len(), Error.GetData());
		}
	}
	return FSharedBuffer();
}

void FBuildJobContext::AddValue(const FValueId& Id, const FValue& Value)
{
	OutputBuilder.AddValue(Id, Value);
}

void FBuildJobContext::AddValue(const FValueId& Id, const FCompressedBuffer& Buffer)
{
	AddValue(Id, FValue(Buffer));
}

void FBuildJobContext::AddValue(const FValueId& Id, const FCompositeBuffer& Buffer, const uint64 BlockSize)
{
	AddValue(Id, FValue::Compress(Buffer, BlockSize));
}

void FBuildJobContext::AddValue(const FValueId& Id, const FSharedBuffer& Buffer, const uint64 BlockSize)
{
	AddValue(Id, FValue::Compress(Buffer, BlockSize));
}

void FBuildJobContext::AddValue(const FValueId& Id, const FCbObject& Object)
{
	AddValue(Id, FValue::Compress(Object.GetBuffer()));
}

void FBuildJobContext::BeginBuild(IRequestOwner& InOwner, TUniqueFunction<void ()>&& InOnEndBuild)
{
	Owner = &InOwner;
	OnEndBuild = MoveTemp(InOnEndBuild);
	Function.Build(*this);
	if (!bIsAsyncBuild)
	{
		EndBuild();
	}
}

void FBuildJobContext::EndBuild()
{
	OnEndBuild();
	BuildCompleteEvent.Trigger();
	Owner = nullptr;
}

void FBuildJobContext::BeginAsyncBuild()
{
	checkf(!bIsAsyncBuild, TEXT("BeginAsyncBuild may only be called once for build of '%s' by %s."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	bIsAsyncBuild = true;
	Owner->Begin(this);
}

void FBuildJobContext::EndAsyncBuild()
{
	checkf(bIsAsyncBuild, TEXT("EndAsyncBuild may only be called after BeginAsyncBuild for build of '%s' by %s."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	checkf(!bIsAsyncBuildComplete, TEXT("EndAsyncBuild may only be called once for build of '%s' by %s."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	bIsAsyncBuildComplete = true;
	Owner->End(this, [this] { EndBuild(); });
}

void FBuildJobContext::SetCacheBucket(FCacheBucket Bucket)
{
	checkf(!Bucket.IsNull(), TEXT("Null cache bucket not allowed for build of '%s' by %s. "
		"The cache can be disabled by calling SetCachePolicyMask(~ECachePolicy::Default)."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	CacheKey.Bucket = Bucket;
}

void FBuildJobContext::SetCachePolicyMask(ECachePolicy Policy)
{
	checkf(EnumHasAllFlags(Policy, ECachePolicy::SkipData),
		TEXT("SkipData flags may not be masked out on the cache policy for build of '%s' by %s. "
		     "Flags for skipping data may be set indirectly through FBuildPolicy."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	checkf(EnumHasAllFlags(Policy, ECachePolicy::KeepAlive),
		TEXT("KeepAlive flag may not be masked out on the cache policy for build of '%s' by %s. "
		     "Flags for cache record lifetime may be set indirectly through FBuildPolicy."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	checkf(EnumHasAllFlags(Policy, ECachePolicy::PartialRecord),
		TEXT("PartialRecord flag may not be masked out on the cache policy for build of '%s' by %s."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	CachePolicyMask = Policy;
}

void FBuildJobContext::SetBuildPolicyMask(EBuildPolicy Policy)
{
	checkf(EnumHasAllFlags(Policy, EBuildPolicy::Cache),
		TEXT("Cache flags may not be masked out on the build policy for build of '%s' by %s. "
		     "Flags for modifying cache operations may be set through SetCachePolicyMask."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	checkf(EnumHasAllFlags(Policy, EBuildPolicy::CacheKeepAlive),
		TEXT("CacheKeepAlive flag may not be masked out on the build policy for build of '%s' by %s. "
		     "Flags for cache record lifetime may only be set through the build session."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	checkf(EnumHasAllFlags(Policy, EBuildPolicy::SkipData),
		TEXT("SkipData flag may not be masked out on the build policy for build of '%s' by %s. "
		     "Flags for skipping the data may only be set through the build session."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	BuildPolicyMask = Policy;
}

void FBuildJobContext::SetPriority(EPriority Priority)
{
}

void FBuildJobContext::Cancel()
{
	checkf(bIsAsyncBuild, TEXT("Cancel may only be called after BeginAsyncBuild for build of '%s' by %s."),
		*Job.GetName(), *WriteToString<32>(Job.GetFunction()));
	Function.CancelAsyncBuild(*this);
}

void FBuildJobContext::Wait()
{
	BuildCompleteEvent.Wait();
}

} // UE::DerivedData::Private
