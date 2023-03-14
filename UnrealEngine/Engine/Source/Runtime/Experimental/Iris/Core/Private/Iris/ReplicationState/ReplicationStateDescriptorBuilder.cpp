// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "CoreMinimal.h"
#include "Iris/Core/BitTwiddling.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/Core/MemoryLayoutUtil.h"
#include "Net/Core/NetBitArray.h"
#include "Net/Core/Trace/NetDebugName.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/InternalPropertyReplicationState.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorRegistry.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/NetBlob/PartialNetObjectAttachmentHandler.h"
#include "Iris/ReplicationSystem/RepTag.h"
#include "Iris/Serialization/InternalNetSerializerUtils.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetSerializers.h"
#include "HAL/ConsoleManager.h"
#include "Hash/CityHash.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/AlignmentTemplates.h"
#include "UObject/UnrealType.h"

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_ENABLE_DESCRIPTORBUILDER_LOG 0
#else
#	define UE_NET_ENABLE_DESCRIPTORBUILDER_LOG 1
#endif 

#if UE_NET_ENABLE_DESCRIPTORBUILDER_LOG
#	define UE_LOG_DESCRIPTORBUILDER(Log, Format, ...)  UE_LOG(LogIris, Log, Format, ##__VA_ARGS__)
#else
#	define UE_LOG_DESCRIPTORBUILDER(...)
#endif

#define UE_LOG_DESCRIPTORBUILDER_WARNING(Format, ...)  UE_LOG(LogIris, Warning, Format, ##__VA_ARGS__)

namespace UE::Net::Private
{

static const FName PropertyNetSerializerRegistry_NAME_ReplicationID("ReplicationID");
static const FName PropertyNetSerializerRegistry_NAME_ChangeMaskStorage("ChangeMaskStorage");
static const FName ReplicationStateDescriptorBuilder_NAME_NetCullDistanceSquared("NetCullDistanceSquared");

static TAutoConsoleVariable<int32> CVarIrisUseNativeFastArray(TEXT("net.Iris.UseNativeFastArray"), 1, TEXT("Enable or disable IrisNativeFastArray."));
static bool bWarnAboutStructsWithCustomSerialization = true;
static FAutoConsoleVariableRef CVarIrisWarnAboutStructsWithCustomSerialization(TEXT("net.Iris.WarnAboutStructsWithCustomSerialization"), bWarnAboutStructsWithCustomSerialization, TEXT("Warn when generating descriptors for structs with custom serialization."));

enum class EMemberPropertyTraits : uint32
{
	None								= 0U,
	InitOnly							= 1U << 0U,
	HasLifetimeConditionals				= InitOnly << 1U,
	RepNotifyOnChanged					= HasLifetimeConditionals << 1U,
	RepNotifyAlways						= RepNotifyOnChanged << 1U,
	NeedPreviousState					= RepNotifyAlways << 1U,
	HasDynamicState						= NeedPreviousState << 1U,
	HasObjectReference					= HasDynamicState << 1U,
	HasCustomObjectReference			= HasObjectReference << 1U,
	IsSourceTriviallyConstructible		= HasCustomObjectReference << 1U,
	IsSourceTriviallyDestructible		= IsSourceTriviallyConstructible << 1U,
	IsFastArray							= IsSourceTriviallyDestructible << 1U,
	IsNativeFastArray					= IsFastArray << 1U,
	IsFastArrayItem						= IsNativeFastArray << 1U,
	HasConnectionSpecificSerialization	= IsFastArrayItem << 1U,
	HasPushBasedDirtiness				= HasConnectionSpecificSerialization << 1U,
};
ENUM_CLASS_FLAGS(EMemberPropertyTraits);

struct FLazyGetPathNameHelper
{
	const FStringBuilderBase* GetPathName(const UObject* Object)
	{
		if (StringBuilder.Len() == 0)
		{
			Object->GetPathName(nullptr, StringBuilder);

			// Make sure that the path have consistent casing
			TCString<TCHAR>::Strupr(StringBuilder.GetData(), StringBuilder.Len());
		}
		return &StringBuilder;
	}

	void Reset()
	{
		StringBuilder.Reset();
	}

	TStringBuilder<4096> StringBuilder;
};

class FPropertyReplicationStateDescriptorBuilder
{
public:

	struct FMemberProperty
	{
		const FProperty* Property;												// The property
		const UFunction* PropertyRepNotifyFunction;								// RepNotifyFunction
		const FPropertyNetSerializerInfo* SerializerInfo;						// SerializerInfo
		CreateAndRegisterReplicationFragmentFunc CreateAndRegisterReplicationFragmentFunction; // Optional create method for fragment of properties that supports separate states (FastArrays)
		EMemberPropertyTraits Traits;											// Traits for the property
		ELifetimeCondition ReplicationCondition;								// Condition for when and where the property should be replicated
		uint16 ChangeMaskBits;													// Bits needed to track dirtiness for this property. Init state properties use no bits.
		FMemoryLayoutUtil::FSizeAndAlignment ExternalSizeAndAlignment;			// If we do not have a property we need to pass this in explicitly
	};

	struct FMemberFunction
	{
		const UFunction* Function;
	};

	// Utility methods
	static void GetSerializerTraits(FMemberProperty& OutMemberProperty, const FProperty* Property, const FPropertyNetSerializerInfo* Info);
	static bool IsSupportedProperty(FMemberProperty& OutMemberProperty, const FProperty* Property, const TArray<FLifetimeProperty>* LifeTimeProperties = nullptr, UClass* InObjectClass = nullptr);
	static bool IsStructWithCustomSerializer(FMemberProperty& OutMemberProperty, const UStruct* InStruct);
	static EMemberPropertyTraits GetConnectionFilterTrait(ELifetimeCondition Condition);
	static EMemberPropertyTraits GetInitOnlyTrait(ELifetimeCondition Condition);
	static EMemberPropertyTraits GetFastArrayPropertyTraits(const FNetSerializer* NetSerializer, const FProperty* Property);
	static SIZE_T GetFastArrayChangeMaskOffset(const FProperty* Property);
	static EMemberPropertyTraits GetHasObjectReferenceTraits(const FNetSerializer* NetSerializer);
	static void GetIrisPropertyTraits(FMemberProperty& OutMemberProperty, const FProperty* Property, const TArray<FLifetimeProperty>* LifeTimeProperties, UClass* ObjectClass);
	static bool IsFastArraySupported(const UStruct* Struct);

public:
	enum class EDescriptorType : uint32
	{
		Class,
		Struct,
		Function,
		SingleProperty,
	};

	struct FBuildParameters
	{
		// Optionally pass in the full PathName which is used to build the identifier for the descriptor
		const FStringBuilderBase* PathName = nullptr;
		// Optionally pass in pointer to source data buffer from which we should create the default state
		const uint8* DefaultStateSourceData = nullptr;
		EDescriptorType DescriptorType;
		uint32 bIsInitState : 1;
		uint32 bAllMembersAreReplicated : 1;
		uint32 bIsFastArray : 1;
	};

	void AddMemberProperty(const FMemberProperty& Info) { Members.Add(Info); }
	void AddMemberFunction(const FMemberFunction& Info) { Functions.Add(Info); }

	void SetStructSizeAndAlignment(SIZE_T Size, SIZE_T Alignment) { StructInfo.SizeAndAlignment.Size = Size; StructInfo.SizeAndAlignment.Alignment = Alignment; } 
	bool HasDataToBuild() const { return (Members.Num() > 0) || (Functions.Num() > 0U); }
	TRefCountPtr<const FReplicationStateDescriptor> Build(const FString& StateName, FReplicationStateDescriptorRegistry* DescriptorRegistry, const FBuildParameters& BuildParams);

private:

	static const SIZE_T MinAlignment = 1;

	using FSizeAndAlignment = FMemoryLayoutUtil::FSizeAndAlignment;
	using FOffsetAndSize = FMemoryLayoutUtil::FOffsetAndSize;

	// We need to cache some information before we can calculate the required size of our descriptor
	struct FMemberCacheEntry
	{
		FMemberCacheEntry() : bUseSerializerDefaultConfig(0), bIsStruct(0), bIsDynamicArray(0), bHasCustomObjectReference(0) {}

		const FNetSerializer* Serializer;							// Serializer
		TRefCountPtr<const FReplicationStateDescriptor>	Descriptor;	// Descriptor for structs or nested replication states?
		FSizeAndAlignment ExternalSizeAndAlignment;					// Size and alignment of external member data
		FSizeAndAlignment InternalSizeAndAlignment;					// Size and alignment of quantized member data
		FSizeAndAlignment SerializerConfigSizeAndAlignment;			// Size and alignment for SerializerConfig
		uint32 bUseSerializerDefaultConfig : 1;						// Whether the member can use the serializer's default config
		uint32 bIsStruct : 1;
		uint32 bIsDynamicArray : 1;
		uint32 bHasCustomObjectReference : 1;
	};

	typedef TArray<FMemberCacheEntry> FMemberCache;

	struct FMemberTagCacheEntry
	{
		FRepTag Tag;
		uint32 MemberIndex;
		uint32 InDescriptor : 1; // Tags can be found in the member's ReplicationStateDescriptor
	};

	struct FMemberTagCache
	{
		FMemberTagCache() : TagCount(0) {}

		TArray<FMemberTagCacheEntry> CacheEntries;
		uint32 TagCount;
	};
	
	struct FMemberReferenceCacheEntry
	{
		uint32 MemberIndex;
		uint32 InMemberStateDescriptor : 1; // Reference is nested and we need to look it up
	};

	struct FMemberReferenceCache
	{
		FMemberReferenceCache() : ReferenceCount(0) {}

		TArray<FMemberReferenceCacheEntry> CacheEntries;
		uint32 ReferenceCount;
	};

	struct FMemberFunctionCacheEntry
	{
		const UFunction* Function;
		TRefCountPtr<const FReplicationStateDescriptor>	Descriptor;
	};
	typedef TArray<FMemberFunctionCacheEntry> FMemberFunctionCache;

	struct FBuilderContext
	{
		FBuilderContext()
		: External({0, MinAlignment})
		, Internal({0, MinAlignment})
		, CombinedPropertyTraits(EMemberPropertyTraits::None)
		, SharedPropertyTraits(EMemberPropertyTraits::None)
		, BuildParams({})
		{
		}

		bool IsInitState() const { return BuildParams.bIsInitState; }

		FMemberCache MemberCache;
		FMemberReferenceCache ReferenceCache;
		FMemberTagCache MemberTagCache;
		FMemberFunctionCache MemberFunctionCache;

		FSizeAndAlignment External;
		FSizeAndAlignment Internal;

		// Or:ed traits
		EMemberPropertyTraits CombinedPropertyTraits;
		// And:ed traits
		EMemberPropertyTraits SharedPropertyTraits;

		FBuildParameters BuildParams;
	};

	struct FStructInfo
	{
		FSizeAndAlignment SizeAndAlignment = {};
	};

	void AddSerializerConfigMemoryBlockToLayout(FMemoryLayoutUtil::FLayout& Layout, FOffsetAndSize& OutOffsetAndSize, const FMemberCache& MemberCache) const;

	void BuildMemberCache(FBuilderContext& Context, FReplicationStateDescriptorRegistry* DescriptorRegistry);
	void BuildMemberReferenceCache(FMemberReferenceCache& MemberReferenceCache, const FMemberCache& MemberCache) const;	
	void BuildMemberTagCache(FMemberTagCache& MemberTagCache, const FMemberCache& MemberCache) const;
	void BuildMemberFunctionCache(FBuilderContext& Context, FReplicationStateDescriptorRegistry* DescriptorRegistry) const;

	void BuildReplicationStateInternal(FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const;
	void BuildMemberDescriptors(FReplicationStateMemberDescriptor* MemberDescriptors, FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const;
	void BuildMemberDescriptorsForStruct(FReplicationStateMemberDescriptor* MemberDescriptors, FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const;
	void BuildMemberChangeMaskDescriptors(FReplicationStateMemberChangeMaskDescriptor* MemberChangeMaskDescriptors, FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const;
	void BuildMemberSerializerDescriptors(FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors, FReplicationStateDescriptor* Descriptor) const;
	void BuildMemberTraitsDescriptors(FReplicationStateMemberTraitsDescriptor* MemberTraitsDescriptors, FReplicationStateDescriptor* Descriptor, const FBuilderContext& Context) const;
	void BuildMemberTagDescriptors(FReplicationStateMemberTagDescriptor* MemberRepTagDescriptors, FReplicationStateDescriptor* Descriptor, const FBuilderContext& Context) const;
	void BuildMemberReferenceDescriptors(FReplicationStateMemberReferenceDescriptor* MemberReferenceDescriptors, FReplicationStateDescriptor* Descriptor, const FBuilderContext& Context) const;
	void BuildMemberFunctionDescriptors(FReplicationStateMemberFunctionDescriptor* MemberFunctionDescriptors, FReplicationStateDescriptor* Descriptor, const FBuilderContext& Context) const;
	void BuildMemberProperties(const FProperty** MemberProperties, FReplicationStateDescriptor* Descriptor) const;
	void BuildMemberPropertyDescriptors(FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors, FReplicationStateDescriptor* Descriptor) const;
	void BuildMemberLifetimeConditionDescriptors(FReplicationStateMemberLifetimeConditionDescriptor* MemberLifetimeConditionDescriptors, FReplicationStateDescriptor* Descriptor) const;
	void BuildMemberRepIndexToMemberDescriptors(FReplicationStateMemberRepIndexToMemberIndexDescriptor* MemberRepIndexToMemberIndexDescriptors, FReplicationStateDescriptor* Descriptor) const;
	void BuildMemberDebugDescriptors(FReplicationStateMemberDebugDescriptor* MemberDebugDescriptors, FReplicationStateDescriptor* Descriptor, const FMemberCache& MemberCache) const;
	void BuildMemberSerializerConfigs(uint8* SerializerConfigBuffer, FReplicationStateDescriptor* Descriptor, const FMemberCache& MemberCache) const;
	void FixupNetRoleNetSerializerConfigs(FReplicationStateDescriptor* Descriptor);
	void FixupDescriptorForNativeFastArray(FReplicationStateDescriptor* Descriptor) const;
	void AllocateAndInitializeDefaultInternalStateBuffer(const uint8* RESTRICT SrcBuffer, const FReplicationStateDescriptor* Descriptor, const uint8*& OutDefaultStateBuffer) const;
	bool CalculateDefaultStateChecksum(const FReplicationStateDescriptor* Descriptor, const uint8* OutDefaultStateBuffer, uint64& OutHashValue) const;

	EReplicationStateTraits BuildReplicationStateTraits(const FBuilderContext& Context) const;

	static FRepTag GetRepTagFromProperty(const FMemberCacheEntry& MemberCacheEntry, const FMemberProperty& Property);

	void FinalizeDescriptor(FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const;

	TRefCountPtr<const FReplicationStateDescriptor> GetDescriptorForStructProperty(const FMemberProperty& MemberProperty, FReplicationStateDescriptorRegistry* DescriptorRegistry) const;
	TRefCountPtr<const FReplicationStateDescriptor> GetDescriptorForArrayProperty(const FProperty* Property, FReplicationStateDescriptorRegistry* DescriptorRegistry) const;
	TRefCountPtr<const FReplicationStateDescriptor> GetOrCreateDescriptorForFunction(const UFunction* Function, FReplicationStateDescriptorRegistry* DescriptorRegistry) const;
	TRefCountPtr<const FReplicationStateDescriptor> CreateDescriptorForProperty(const FString& DescriptorName, const FProperty* Property, const FReplicationStateDescriptorBuilder::FParameters& Parameters) const;

	// Helpers
	static void GetPropertyPathName(const FProperty* Property, FString& PathName);

	FString Name;
	TArray<FMemberProperty> Members;
	TArray<FMemberFunction> Functions;
	FStructInfo StructInfo;
};

TRefCountPtr<const FReplicationStateDescriptor> FPropertyReplicationStateDescriptorBuilder::GetDescriptorForStructProperty(const FMemberProperty& MemberProperty, FReplicationStateDescriptorRegistry* DescriptorRegistry) const
{
	const FStructProperty* StructProp = CastFieldChecked<const FStructProperty>(MemberProperty.Property);
	const UStruct* Struct = StructProp->Struct;

	FReplicationStateDescriptorBuilder::FParameters Params;
	Params.DescriptorRegistry = DescriptorRegistry;
	Params.EnableFastArrayHandling = EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::IsFastArray);

	return FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(Struct, Params);
}

TRefCountPtr<const FReplicationStateDescriptor> FPropertyReplicationStateDescriptorBuilder::GetDescriptorForArrayProperty(const FProperty* Property, FReplicationStateDescriptorRegistry* DescriptorRegistry) const
{
	FReplicationStateDescriptorBuilder::FParameters Params;
	Params.DescriptorRegistry = DescriptorRegistry;

	const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
	const FProperty* InnerProperty = ArrayProperty->Inner;

	FString DescriptorName;
	GetPropertyPathName(ArrayProperty, DescriptorName);

	return CreateDescriptorForProperty(DescriptorName, InnerProperty, Params);
}

TRefCountPtr<const FReplicationStateDescriptor> FPropertyReplicationStateDescriptorBuilder::GetOrCreateDescriptorForFunction(const UFunction* Function, FReplicationStateDescriptorRegistry* DescriptorRegistry) const
{
	FReplicationStateDescriptorBuilder::FParameters Params;
	Params.DescriptorRegistry = DescriptorRegistry;

	return FReplicationStateDescriptorBuilder::CreateDescriptorForFunction(Function, Params);
}

TRefCountPtr<const FReplicationStateDescriptor> FPropertyReplicationStateDescriptorBuilder::CreateDescriptorForProperty(const FString& DescriptorName, const FProperty* Property, const FReplicationStateDescriptorBuilder::FParameters& Parameters) const
{
	IRIS_PROFILER_SCOPE(FPropertyReplicationStateDescriptorBuilder_CreateDescriptorForProperty);

	// Check registry if we already have created descriptors for this class
	if (Parameters.DescriptorRegistry)
	{	
		if (auto Result = Parameters.DescriptorRegistry->Find(Property))
		{
			// For structs we do only expect a single entry
			ensure(Result->Num() == 1);

			return (*Result)[0];
		}
	}

	FPropertyReplicationStateDescriptorBuilder Builder;
	FPropertyReplicationStateDescriptorBuilder::FMemberProperty MemberProperty;

	const bool bIsSupportedProperty = FPropertyReplicationStateDescriptorBuilder::IsSupportedProperty(MemberProperty, Property);
	if (!ensure(bIsSupportedProperty))
	{
		return nullptr;
	}

	Builder.AddMemberProperty(MemberProperty);

	FPropertyReplicationStateDescriptorBuilder::FBuildParameters BuildParameters = {};
	BuildParameters.DescriptorType = FPropertyReplicationStateDescriptorBuilder::EDescriptorType::SingleProperty;
	BuildParameters.bIsInitState = false;
	BuildParameters.bAllMembersAreReplicated = true;
	auto Descriptor = Builder.Build(DescriptorName, Parameters.DescriptorRegistry, BuildParameters);

	if (Parameters.DescriptorRegistry && Descriptor.IsValid())
	{
		Parameters.DescriptorRegistry->Register(Property, Descriptor);
	}

	return Descriptor;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberCache(FBuilderContext& Context, FReplicationStateDescriptorRegistry* DescriptorRegistry)
{
	EMemberPropertyTraits PropertyTraits = EMemberPropertyTraits::None;
	EMemberPropertyTraits SharedPropertyTraits = (Members.Num() > 0 ? ~EMemberPropertyTraits(EMemberPropertyTraits::None) : EMemberPropertyTraits::None);

	FMemberCache& MemberCache = Context.MemberCache;
	MemberCache.SetNum(Members.Num());
	FMemberCacheEntry* CurrentCacheEntry = MemberCache.GetData();

	for (FMemberProperty& Member : Members)
	{
		const FNetSerializer* Serializer = Member.SerializerInfo->GetNetSerializer(Member.Property);
		CurrentCacheEntry->Serializer = Serializer;

		// This is a property based struct, make sure we have a descriptor for it.
		if (IsStructNetSerializer(Serializer))
		{
			// Build descriptor for struct
			CurrentCacheEntry->Descriptor = GetDescriptorForStructProperty(Member, DescriptorRegistry);

			const FReplicationStateDescriptor* Descriptor = CurrentCacheEntry->Descriptor.GetReference();

			// Propagate some traits. Doing this earlier would be quite expensive.
			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasObjectReference))
			{
				Member.Traits |= EMemberPropertyTraits::HasObjectReference;
			}

			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
			{
				Member.Traits |= EMemberPropertyTraits::HasDynamicState;
			}

			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::IsSourceTriviallyConstructible))
			{
				Member.Traits |= EMemberPropertyTraits::IsSourceTriviallyConstructible;
			}

			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::IsSourceTriviallyDestructible))
			{
				Member.Traits |= EMemberPropertyTraits::IsSourceTriviallyDestructible;
			}

			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasConnectionSpecificSerialization))
			{
				Member.Traits |= EMemberPropertyTraits::HasConnectionSpecificSerialization;
			}

			if (!EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::AllMembersAreReplicated))
			{
				// If we are building a struct containing a struct that isn't fully replicated then this struct isn't fully replicated either.
				Context.BuildParams.bAllMembersAreReplicated = false;
			}

			/**
			  * Use ElementSize instead of GetSize() since GetSize() will multiply by array size and currently we treat
			  * each element individually. 
			  */
			CurrentCacheEntry->ExternalSizeAndAlignment = { (SIZE_T)Member.Property->ElementSize, (SIZE_T)Member.Property->GetMinAlignment() };
			CurrentCacheEntry->InternalSizeAndAlignment = { Descriptor->InternalSize, Descriptor->InternalAlignment };

			CurrentCacheEntry->bIsStruct = 1;
		}
		else if (IsArrayPropertyNetSerializer(Serializer))
		{
			// Build descriptor for inner property
			CurrentCacheEntry->Descriptor = GetDescriptorForArrayProperty(Member.Property, DescriptorRegistry);

			const FReplicationStateDescriptor* Descriptor = CurrentCacheEntry->Descriptor.GetReference();

			// Propagate some traits. Doing this earlier would be quite expensive.
			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasObjectReference))
			{
				Member.Traits |= EMemberPropertyTraits::HasObjectReference;
			}

			// We propagate this up so that a struct wrapping an array that is not fully replicated will also have this set
			if (!EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::AllMembersAreReplicated))
			{
				// If we are building a struct containing a struct that isn't fully replicated then this struct isn't fully replicated either.
				Context.BuildParams.bAllMembersAreReplicated = false;
			}

			CurrentCacheEntry->ExternalSizeAndAlignment = { (SIZE_T)Member.Property->ElementSize, (SIZE_T)Member.Property->GetMinAlignment() };
			CurrentCacheEntry->InternalSizeAndAlignment = { Serializer->QuantizedTypeSize, Serializer->QuantizedTypeAlignment };

			CurrentCacheEntry->bIsDynamicArray = 1;
		}
		else if (Member.Property)
		{
			CurrentCacheEntry->ExternalSizeAndAlignment = { (SIZE_T)Member.Property->ElementSize, (SIZE_T)Member.Property->GetMinAlignment() };
			CurrentCacheEntry->InternalSizeAndAlignment = { Serializer->QuantizedTypeSize, Serializer->QuantizedTypeAlignment };
		}
		else
		{
			CurrentCacheEntry->ExternalSizeAndAlignment = Member.ExternalSizeAndAlignment;
			CurrentCacheEntry->InternalSizeAndAlignment = { Serializer->QuantizedTypeSize, Serializer->QuantizedTypeAlignment };
		}

		CurrentCacheEntry->SerializerConfigSizeAndAlignment = { Serializer->ConfigTypeSize, Serializer->ConfigTypeAlignment };
		CurrentCacheEntry->bUseSerializerDefaultConfig = Member.SerializerInfo->CanUseDefaultConfig(Member.Property);

		// Combined Traits. Traits can be propagated from structs so keep this near the end of the loop.
		PropertyTraits |= Member.Traits;
		SharedPropertyTraits &= Member.Traits;

		++CurrentCacheEntry;
	}

	Context.CombinedPropertyTraits = PropertyTraits;
	Context.SharedPropertyTraits = SharedPropertyTraits;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberTagCache(FMemberTagCache& MemberTagCache, const FMemberCache& MemberCache) const
{
	const FMemberProperty* MemberProperties = Members.GetData();

	uint32 EntryIndex = 0;
	for (const FMemberCacheEntry& MemberCacheEntry : MemberCache)
	{
		const FMemberProperty& MemberProperty = MemberProperties[EntryIndex];

		// Find tags set on the member itself.
		{
			FRepTag Tag = GetRepTagFromProperty(MemberCacheEntry, MemberProperty);
			if (Tag != GetInvalidRepTag())
			{
				FMemberTagCacheEntry TagEntry;
				TagEntry.Tag = Tag;
				TagEntry.InDescriptor = 0U;
				TagEntry.MemberIndex = EntryIndex;

				++MemberTagCache.TagCount;
				MemberTagCache.CacheEntries.Add(TagEntry);
			}
		}

		// If the member is a struct it can have tags on its members. We want to expose those as well.
		if (const FReplicationStateDescriptor* Descriptor = MemberCacheEntry.Descriptor)
		{
			// Pull out tags to our descriptor. We expect few tags and we want to find them as quickly as possible when needed.
			// We do NOT support a tag on something that require a separate descriptor. That would require us to store 
			// additional information per tag. Easy to implement but not a priority at the moment.
			if (!MemberCacheEntry.bIsDynamicArray && Descriptor->TagCount)
			{
				FMemberTagCacheEntry TagEntry;
				TagEntry.Tag = 0;
				TagEntry.InDescriptor = 1U;
				TagEntry.MemberIndex = EntryIndex;

				MemberTagCache.TagCount += Descriptor->TagCount;
				MemberTagCache.CacheEntries.Add(TagEntry);
			}
		}

		++EntryIndex;
	}
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberReferenceCache(FMemberReferenceCache& MemberReferenceCache, const FMemberCache& MemberCache) const
{
	const FMemberProperty* MemberProperties = Members.GetData();

	uint32 EntryIndex = 0;
	for (const FMemberCacheEntry& MemberCacheEntry : MemberCache)
	{
		const FMemberProperty& MemberProperty = MemberProperties[EntryIndex];

		// If the member does not have any references we skip it
		if (!EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::HasObjectReference))
		{
			++EntryIndex;
			continue;
		}

		// If we are a struct we unroll references in the outer
		if (MemberCacheEntry.bIsStruct)
		{
			const FReplicationStateDescriptor* Descriptor = MemberCacheEntry.Descriptor;

			FMemberReferenceCacheEntry Entry;
		
			Entry.InMemberStateDescriptor = 1U;
			Entry.MemberIndex = EntryIndex;

			MemberReferenceCache.ReferenceCount += Descriptor->ObjectReferenceCount;
			MemberReferenceCache.CacheEntries.Add(Entry);
		}
		// For dynamic arrays and serializers with custom references we must always use the descriptor to get nested references
		else if (MemberCacheEntry.bIsDynamicArray || EnumHasAnyFlags(MemberCacheEntry.Serializer->Traits, ENetSerializerTraits::HasCustomNetReference))
		{
			FMemberReferenceCacheEntry Entry;
		
			Entry.InMemberStateDescriptor = 1U;
			Entry.MemberIndex = EntryIndex;

			++MemberReferenceCache.ReferenceCount;
			MemberReferenceCache.CacheEntries.Add(Entry);
		}
		else
		{
			FMemberReferenceCacheEntry Entry;
		
			Entry.InMemberStateDescriptor = 0U;
			Entry.MemberIndex = EntryIndex;

			++MemberReferenceCache.ReferenceCount;
			MemberReferenceCache.CacheEntries.Add(Entry);
		}
		++EntryIndex;
	}
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberFunctionCache(FBuilderContext& Context, FReplicationStateDescriptorRegistry* DescriptorRegistry) const
{
	FMemberFunctionCache& MemberFunctionCache = Context.MemberFunctionCache;
	MemberFunctionCache.SetNum(Functions.Num());
	
	FMemberFunctionCacheEntry* CurrentCacheEntry = MemberFunctionCache.GetData();
	for (const FMemberFunction& MemberFunction : Functions)
	{
		CurrentCacheEntry->Function = MemberFunction.Function;
		CurrentCacheEntry->Descriptor = GetOrCreateDescriptorForFunction(MemberFunction.Function, DescriptorRegistry);
		++CurrentCacheEntry;
	}
}

void FPropertyReplicationStateDescriptorBuilder::AllocateAndInitializeDefaultInternalStateBuffer(const uint8* RESTRICT SrcBuffer, const FReplicationStateDescriptor* Descriptor, const uint8*& OutDefaultStateBuffer) const
{
	// Allocate storage for incoming data, it will be freed when we destroy the descriptor
	uint8* DstStateBuffer = (uint8*)FMemory::Malloc(Descriptor->InternalSize, Descriptor->InternalAlignment);
	// Initialize to zero
	FMemory::Memzero(DstStateBuffer, Descriptor->InternalSize);

	// We setup a default context, we can however not capture object references as they need to resolve to the same value
	// on both server and client
	FNetSerializationContext Context;
	FInternalNetSerializationContext InternalContext;
	Context.SetInternalContext(&InternalContext);
	Context.SetIsInitializingDefaultState(true);

	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
	const FProperty** MemberProperties = Descriptor->MemberProperties;
	const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = Descriptor->MemberPropertyDescriptors;
	const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->MemberSerializerDescriptors;
	const uint32 MemberCount = Descriptor->MemberCount;

	for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
	{
		const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
		const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor = MemberSerializerDescriptors[MemberIt];
		const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
		const FProperty* Property = MemberProperties[MemberIt];

		FNetQuantizeArgs Args;
		Args.Version = 0;
		Args.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
		Args.Source = reinterpret_cast<NetSerializerValuePointer>(SrcBuffer + Property->GetOffset_ForGC() + Property->ElementSize * MemberPropertyDescriptor.ArrayIndex);
		Args.Target = reinterpret_cast<NetSerializerValuePointer>(DstStateBuffer + MemberDescriptor.InternalMemberOffset);

		MemberSerializerDescriptor.Serializer->Quantize(Context, Args);
	}

	OutDefaultStateBuffer = DstStateBuffer;
}

bool FPropertyReplicationStateDescriptorBuilder::CalculateDefaultStateChecksum(const FReplicationStateDescriptor* Descriptor, const uint8* OutDefaultStateBuffer, uint64& OutHashValue) const
{
	const UPartialNetObjectAttachmentHandlerConfig* PartialNetObjectAttachmentHandlerConfig = GetDefault<UPartialNetObjectAttachmentHandlerConfig>();

	constexpr uint32 SmallBufferSize = 1024;
	const uint32 MaxBufferSize = PartialNetObjectAttachmentHandlerConfig->GetTotalMaxPayloadBitCount()/8U;

	// Setup small buffer first as it might be written to when the writer's destructor is called.
	TArray<uint32, TInlineAllocator<SmallBufferSize/sizeof(uint32)> > TempBuffer;
	TempBuffer.SetNum(SmallBufferSize/sizeof(uint32));

	// We setup a default context
	FNetBitStreamWriter Writer;
	FNetSerializationContext Context(&Writer);
	FInternalNetSerializationContext InternalContext;
	Context.SetInternalContext(&InternalContext);

	// First try to serialize to small inline buffer
	Writer.InitBytes((uint8*)TempBuffer.GetData(), SmallBufferSize);
	FReplicationStateOperations::Serialize(Context, Descriptor->DefaultStateBuffer, Descriptor);

	// Try again with largest supported buffer
	if (Writer.IsOverflown())
	{
		TempBuffer.SetNum(MaxBufferSize/sizeof(uint32));
		Writer.InitBytes((uint8*)TempBuffer.GetData(), MaxBufferSize);
		FReplicationStateOperations::Serialize(Context, Descriptor->DefaultStateBuffer, Descriptor);
	}

	// Make sure last byte has well defined data as we include it in hash
	if (const uint32 BitsToFill = (8U - (Writer.GetPosBits() & 7U)) & 7U)
	{
		Writer.WriteBits(0U, BitsToFill);
	}

	// State is too large, we cannot calculate a default hash for it
	if (Writer.IsOverflown())
	{
		return false;
	}

	Writer.CommitWrites();

	// Hash the serialized buffer
	OutHashValue = CityHash64((const char*)TempBuffer.GetData(), Writer.GetPosBytes());

	return true;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberDescriptors(FReplicationStateMemberDescriptor* MemberDescriptors, FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const
{
	const FMemberCache& MemberCache = Context.MemberCache;

	SIZE_T ExternalOffset = Context.External.Size;
	SIZE_T ExternalBufferAlignment = Context.External.Alignment;

	SIZE_T InternalOffset = Context.Internal.Size;
	SIZE_T InternalBufferAlignment = Context.Internal.Alignment;

	FReplicationStateMemberDescriptor* CurrentMemberDescriptor = MemberDescriptors;

	const FMemberCacheEntry* MemberCacheEntry = MemberCache.GetData();
	for (const FMemberProperty& Member : Members)
	{
		// External
		const SIZE_T ExternalMemberAlignment = MemberCacheEntry->ExternalSizeAndAlignment.Alignment;
		const SIZE_T ExternalMemberSize = MemberCacheEntry->ExternalSizeAndAlignment.Size;

		ExternalBufferAlignment = FMath::Max(ExternalBufferAlignment, ExternalMemberAlignment);
		ExternalOffset = Align(ExternalOffset, ExternalMemberAlignment);
		CurrentMemberDescriptor->ExternalMemberOffset = ExternalOffset;
		ExternalOffset += ExternalMemberSize;

		// Internal
		const FNetSerializer* MemberSerializer = MemberCacheEntry->Serializer;
		const SIZE_T InternalMemberAlignment = MemberCacheEntry->InternalSizeAndAlignment.Alignment;
		const SIZE_T InternalMemberSize = MemberCacheEntry->InternalSizeAndAlignment.Size;

		InternalBufferAlignment = FMath::Max(InternalBufferAlignment, InternalMemberAlignment);
		InternalOffset = Align(InternalOffset, InternalMemberAlignment);
		CurrentMemberDescriptor->InternalMemberOffset = InternalOffset;
		InternalOffset += InternalMemberSize;

		++CurrentMemberDescriptor;
		++MemberCacheEntry;
	}

	// Fill in descriptor
	Descriptor->MemberDescriptors = MemberDescriptors;
	Descriptor->MemberCount = Members.Num();

	// Update Context
	Context.External.Alignment = ExternalBufferAlignment;
	Context.External.Size = ExternalOffset;

	Context.Internal.Alignment = InternalBufferAlignment;
	Context.Internal.Size = InternalOffset;
}

/**
  * A struct's external state will be the same size as the struct. This in order to be able to handle rep notifies with the previous value as parameter.
  * That means we cannot compact the representation. We must use the offsets stored in the properties.
  */
void FPropertyReplicationStateDescriptorBuilder::BuildMemberDescriptorsForStruct(FReplicationStateMemberDescriptor* MemberDescriptors, FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const
{
	const FMemberCache& MemberCache = Context.MemberCache;

	SIZE_T ExternalOffset = Context.External.Size;
	// When the struct is a function we get incorrect information regarding alignment requirements. This means we have to calculate it ourselves.
	SIZE_T ExternalBufferAlignment = FMath::Max(Context.External.Alignment, StructInfo.SizeAndAlignment.Alignment);

	SIZE_T InternalOffset = Context.Internal.Size;
	SIZE_T InternalBufferAlignment = Context.Internal.Alignment;

	FReplicationStateMemberDescriptor* CurrentMemberDescriptor = MemberDescriptors;

	const FProperty* LastProperty = nullptr;
	uint16 ArrayIndex = 0;

	const FMemberCacheEntry* MemberCacheEntry = MemberCache.GetData();
	for (const FMemberProperty& Member : Members)
	{
		// External
		const FProperty* Property = Member.Property;

		ArrayIndex = (Property == LastProperty ? uint16(ArrayIndex + 1) : uint16(0));
		LastProperty = Property;

		ExternalBufferAlignment = FMath::Max(ExternalBufferAlignment, MemberCacheEntry->ExternalSizeAndAlignment.Alignment);
		CurrentMemberDescriptor->ExternalMemberOffset = Property ? (Property->GetOffset_ForGC() + (ArrayIndex * (uint32)Property->ElementSize)) : 0U;

		// Internal
		const FNetSerializer* MemberSerializer = MemberCacheEntry->Serializer;
		const SIZE_T InternalMemberAlignment = MemberCacheEntry->InternalSizeAndAlignment.Alignment;
		const SIZE_T InternalMemberSize = MemberCacheEntry->InternalSizeAndAlignment.Size;

		InternalBufferAlignment = FMath::Max(InternalBufferAlignment, InternalMemberAlignment);
		InternalOffset = Align(InternalOffset, InternalMemberAlignment);
		CurrentMemberDescriptor->InternalMemberOffset = InternalOffset;
		InternalOffset += InternalMemberSize;

		++CurrentMemberDescriptor;
		++MemberCacheEntry;
	}

	// Fill in descriptor
	Descriptor->MemberDescriptors = MemberDescriptors;
	Descriptor->MemberCount = Members.Num();

	// Update Context
	Context.External.Alignment = FMath::Max(Context.External.Alignment, ExternalBufferAlignment);
	Context.External.Size += StructInfo.SizeAndAlignment.Size;

	Context.Internal.Alignment = InternalBufferAlignment;
	Context.Internal.Size = InternalOffset;
}

void FPropertyReplicationStateDescriptorBuilder::BuildReplicationStateInternal(FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const
{
	// Reserve space for internals
	const SIZE_T InternalStateAlignment = alignof(FReplicationStateHeader);
	const SIZE_T InternalStateOffset = Align(Context.External.Size, InternalStateAlignment);

	// This should be the first member of a replication state
	check(InternalStateOffset == 0);

	Context.External.Size = InternalStateOffset + sizeof(FReplicationStateHeader);
	Context.External.Alignment =  FMath::Max(Context.External.Alignment, InternalStateAlignment);
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberChangeMaskDescriptors(FReplicationStateMemberChangeMaskDescriptor* MemberChangeMaskDescriptors, FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const
{	
	check(Descriptor->MemberDescriptors == nullptr);

	uint32 CurrentBitOffset = 0;

	FReplicationStateMemberChangeMaskDescriptor* CurrentMemberChangeMaskDescriptor = MemberChangeMaskDescriptors;
	const bool bIsInitState = Context.IsInitState();

	for (const FMemberProperty& Info : Members)
	{
		const uint32 BitCount = (bIsInitState ? 0U : Info.ChangeMaskBits);

		CurrentMemberChangeMaskDescriptor->BitCount = BitCount;
		CurrentMemberChangeMaskDescriptor->BitOffset = (bIsInitState ? 0U : CurrentBitOffset);

		CurrentBitOffset += BitCount;
		++CurrentMemberChangeMaskDescriptor;
	}

	Descriptor->MemberChangeMaskDescriptors = MemberChangeMaskDescriptors;

	// Info about the state mask for this state
	const SIZE_T ChangeMaskMemberAlignment = alignof(FNetBitArrayView::StorageWordType);
	const SIZE_T ChangeMaskMemberSize = FNetBitArrayView::CalculateRequiredWordCount(CurrentBitOffset) * sizeof(FNetBitArrayView::StorageWordType);
	// If state has custom conditionals we need an additional changemask.
	const SIZE_T ConditionalChangeMaskMemberSize = (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasLifetimeConditionals) ? ChangeMaskMemberSize : 0U);

	Descriptor->ChangeMaskBitCount = CurrentBitOffset;
	Descriptor->ChangeMasksExternalOffset = Align(Context.External.Size, ChangeMaskMemberAlignment);

	// Update context
	Context.External.Alignment = FMath::Max(Context.External.Alignment, ChangeMaskMemberAlignment);
	Context.External.Size = Descriptor->ChangeMasksExternalOffset + ChangeMaskMemberSize + ConditionalChangeMaskMemberSize;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberSerializerDescriptors(FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors, FReplicationStateDescriptor* Descriptor) const
{
	check(Descriptor->MemberDescriptors);

	uint32 CurrentBitOffset = 0;

	FReplicationStateMemberSerializerDescriptor* CurrentMemberSerializerDescriptor = MemberSerializerDescriptors;	
	for (const FMemberProperty& Info : Members)
	{
		CurrentMemberSerializerDescriptor->Serializer = Info.SerializerInfo->GetNetSerializer(Info.Property);
		// The config is set in BuildMemberSerializerConfigs
		CurrentMemberSerializerDescriptor->SerializerConfig = nullptr;
		++CurrentMemberSerializerDescriptor;
	}

	// Fill in descriptor
	Descriptor->MemberSerializerDescriptors = MemberSerializerDescriptors;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberTraitsDescriptors(FReplicationStateMemberTraitsDescriptor* MemberTraitsDescriptors, FReplicationStateDescriptor* Descriptor, const FBuilderContext& Context) const
{
	FReplicationStateMemberTraitsDescriptor* CurrentMemberTraitsDescriptor = MemberTraitsDescriptors;	
	for (const FMemberProperty& Info : Members)
	{
		CurrentMemberTraitsDescriptor->Traits |= (EnumHasAnyFlags(Info.Traits, EMemberPropertyTraits::HasDynamicState) ? EReplicationStateMemberTraits::HasDynamicState : EReplicationStateMemberTraits::None);
		CurrentMemberTraitsDescriptor->Traits |= (EnumHasAnyFlags(Info.Traits, EMemberPropertyTraits::HasObjectReference) ? EReplicationStateMemberTraits::HasObjectReference : EReplicationStateMemberTraits::None);
		++CurrentMemberTraitsDescriptor;
	}

	// Fill in descriptor
	Descriptor->MemberTraitsDescriptors = MemberTraitsDescriptors;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberTagDescriptors(FReplicationStateMemberTagDescriptor* MemberTagDescriptors, FReplicationStateDescriptor* Descriptor, const FBuilderContext& Context) const
{
	check(Descriptor->MemberDescriptors);

	const FMemberCacheEntry* MemberCacheEntries = Context.MemberCache.GetData();
	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;

	FReplicationStateMemberTagDescriptor* CurrentMemberTagDescriptor = MemberTagDescriptors;	
	for (const FMemberTagCacheEntry& TagEntry : Context.MemberTagCache.CacheEntries)
	{
		const FMemberCacheEntry& MemberEntry = MemberCacheEntries[TagEntry.MemberIndex];
		const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[TagEntry.MemberIndex];
		if (TagEntry.InDescriptor)
		{
			// Copy tags from descriptor
			const FReplicationStateDescriptor* MemberStateDescriptor = MemberEntry.Descriptor; 
			for (uint32 TagIt = 0, TagEndIt = MemberStateDescriptor->TagCount; TagIt != TagEndIt; ++TagIt)
			{
				CurrentMemberTagDescriptor->Tag = MemberStateDescriptor->MemberTagDescriptors[TagIt].Tag;
				CurrentMemberTagDescriptor->MemberIndex = TagEntry.MemberIndex;
				CurrentMemberTagDescriptor->InnerTagIndex = TagIt;
				++CurrentMemberTagDescriptor;
			}
		}
		else
		{
			CurrentMemberTagDescriptor->Tag = TagEntry.Tag;
			CurrentMemberTagDescriptor->MemberIndex = TagEntry.MemberIndex;
			CurrentMemberTagDescriptor->InnerTagIndex = MAX_uint16;
			++CurrentMemberTagDescriptor;
		}
	}

	// Fill in descriptor
	Descriptor->MemberTagDescriptors = MemberTagDescriptors;
	Descriptor->TagCount = static_cast<uint16>(Context.MemberTagCache.TagCount);
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberReferenceDescriptors(FReplicationStateMemberReferenceDescriptor* MemberReferenceDescriptors, FReplicationStateDescriptor* Descriptor, const FBuilderContext& Context) const
{
	check(Descriptor->MemberDescriptors);

	const FMemberCacheEntry* MemberCacheEntries = Context.MemberCache.GetData();
	const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;

	FReplicationStateMemberReferenceDescriptor* CurrentMemberReferenceDescriptor = MemberReferenceDescriptors;	
	for (const FMemberReferenceCacheEntry& Entry : Context.ReferenceCache.CacheEntries)
	{
		const FMemberCacheEntry& MemberEntry = MemberCacheEntries[Entry.MemberIndex];
		const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[Entry.MemberIndex];
		if (Entry.InMemberStateDescriptor)
		{
			if (MemberEntry.bIsStruct)
			{
				// Flatten out references from nested members with static storage
				const FReplicationStateDescriptor* MemberStateDescriptor = MemberEntry.Descriptor; 
				for (uint32 InnerIt = 0, InnerEndIt = MemberStateDescriptor->ObjectReferenceCount; InnerIt != InnerEndIt; ++InnerIt)
				{
					// Copy inner info
					CurrentMemberReferenceDescriptor->Info = MemberStateDescriptor->MemberReferenceDescriptors[InnerIt].Info;

					// Update offset to point to inner offset
					CurrentMemberReferenceDescriptor->Offset = MemberDescriptors[Entry.MemberIndex].InternalMemberOffset + MemberStateDescriptor->MemberReferenceDescriptors[InnerIt].Offset;

					CurrentMemberReferenceDescriptor->MemberIndex = Entry.MemberIndex;
					CurrentMemberReferenceDescriptor->InnerReferenceIndex = InnerIt;

					++CurrentMemberReferenceDescriptor;
				}
			}
			else if (MemberEntry.bIsDynamicArray || EnumHasAnyFlags(MemberEntry.Serializer->Traits, ENetSerializerTraits::HasCustomNetReference))
			{
				// store entry indicating that this is a dynamic array that have references
				// We use an invalid resolve-type to indicate that we need to look this up from dynamic data
				CurrentMemberReferenceDescriptor->Info.ResolveType = FNetReferenceInfo::EResolveType::Invalid;

				// Offset to member containing data about the array
				CurrentMemberReferenceDescriptor->Offset = MemberDescriptors[Entry.MemberIndex].InternalMemberOffset;
	
				CurrentMemberReferenceDescriptor->MemberIndex = Entry.MemberIndex;
				CurrentMemberReferenceDescriptor->InnerReferenceIndex = MAX_uint16;

				++CurrentMemberReferenceDescriptor;
			}
		}
		else
		{
			CurrentMemberReferenceDescriptor->Info.ResolveType = FNetReferenceInfo::EResolveType::ResolveOnClient;

			// Offset to reference
			CurrentMemberReferenceDescriptor->Offset = MemberDescriptors[Entry.MemberIndex].InternalMemberOffset;

			CurrentMemberReferenceDescriptor->MemberIndex = Entry.MemberIndex;
			CurrentMemberReferenceDescriptor->InnerReferenceIndex = MAX_uint16;
			++CurrentMemberReferenceDescriptor;
		}
	}

	// Fill in descriptor
	Descriptor->MemberReferenceDescriptors = MemberReferenceDescriptors;
	Descriptor->ObjectReferenceCount = Context.ReferenceCache.ReferenceCount;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberFunctionDescriptors(FReplicationStateMemberFunctionDescriptor* MemberFunctionDescriptors, FReplicationStateDescriptor* Descriptor, const FBuilderContext& Context) const
{
	FReplicationStateMemberFunctionDescriptor* CurrentMemberFunctionDescriptor = MemberFunctionDescriptors;
	for (const FMemberFunctionCacheEntry& MemberFunction : Context.MemberFunctionCache)
	{
		CurrentMemberFunctionDescriptor->Function = MemberFunction.Function;
		CurrentMemberFunctionDescriptor->Descriptor = MemberFunction.Descriptor;
		// We store raw pointers in the descriptor that gets Released() when the descriptor is released.
		CurrentMemberFunctionDescriptor->Descriptor->AddRef();
		++CurrentMemberFunctionDescriptor;

		UE_LOG_DESCRIPTORBUILDER(Verbose, TEXT("FPropertyReplicationStateDescriptorBuilder::BuildMemberFunctionDescriptors AddingFunction %s"), ToCStr(MemberFunction.Function->GetName()));		
	}

	Descriptor->MemberFunctionDescriptors = MemberFunctionDescriptors;
	Descriptor->FunctionCount = static_cast<uint16>(Context.MemberFunctionCache.Num());
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberProperties(const FProperty** MemberProperties, FReplicationStateDescriptor* Descriptor) const
{
	const FProperty** CurrentMemberProperty = MemberProperties;
	for (const FMemberProperty& Info : Members)
	{		
		*CurrentMemberProperty = Info.Property;

		++CurrentMemberProperty;
	}

	// Fill in descriptor
	Descriptor->MemberProperties = MemberProperties;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberPropertyDescriptors(FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors, FReplicationStateDescriptor* Descriptor) const
{
	FReplicationStateMemberPropertyDescriptor* CurrentMemberPropertyDescriptor = MemberPropertyDescriptors;

	const FProperty* LastProperty = nullptr;
	uint16 ArrayIndex = 0;
	for (const FMemberProperty& Info : Members)
	{
		ArrayIndex = (Info.Property == LastProperty ? uint16(ArrayIndex + 1) : uint16(0));
		LastProperty = Info.Property;

		CurrentMemberPropertyDescriptor->RepNotifyFunction = Info.PropertyRepNotifyFunction;
		CurrentMemberPropertyDescriptor->ArrayIndex = ArrayIndex;
		++CurrentMemberPropertyDescriptor;
	}

	// Fill in descriptor
	Descriptor->MemberPropertyDescriptors = MemberPropertyDescriptors;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberLifetimeConditionDescriptors(FReplicationStateMemberLifetimeConditionDescriptor* MemberLifetimeConditionDescriptors, FReplicationStateDescriptor* Descriptor) const
{
	FReplicationStateMemberLifetimeConditionDescriptor* CurrentMemberLifetimeConditionDescriptors = MemberLifetimeConditionDescriptors;
	for (const FMemberProperty& Info : Members)
	{
		CurrentMemberLifetimeConditionDescriptors->Condition = static_cast<int8>(Info.ReplicationCondition);
		++CurrentMemberLifetimeConditionDescriptors;
	}

	Descriptor->MemberLifetimeConditionDescriptors = MemberLifetimeConditionDescriptors;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberRepIndexToMemberDescriptors(FReplicationStateMemberRepIndexToMemberIndexDescriptor* MemberRepIndexToMemberIndexDescriptors, FReplicationStateDescriptor* Descriptor) const
{
	uint16 MaxRepIndex = 0;
	for (const FMemberProperty& Member : Members)
	{
		MaxRepIndex = FPlatformMath::Max(MaxRepIndex, Member.Property->RepIndex);
	}
	const uint32 RepIndexCount = static_cast<uint32>(MaxRepIndex) + 1U;

	// Faster way of setting all the MemberIndex values to InvalidEntry
	{
		static_assert(FReplicationStateMemberRepIndexToMemberIndexDescriptor::InvalidEntry == 65535U, "");
		FPlatformMemory::Memset(MemberRepIndexToMemberIndexDescriptors, 0xFF, RepIndexCount*sizeof(FReplicationStateMemberRepIndexToMemberIndexDescriptor));
	}

	for (const FMemberProperty& Info : Members)
	{

		const SIZE_T MemberIndex = &Info - Members.GetData();
		const uint16 RepIndex = Info.Property->RepIndex;
		checkSlow(RepIndex < RepIndexCount);
		MemberRepIndexToMemberIndexDescriptors[RepIndex].MemberIndex = MemberIndex;
	}

	Descriptor->MemberRepIndexToMemberIndexDescriptors = MemberRepIndexToMemberIndexDescriptors;
	Descriptor->RepIndexCount = static_cast<uint16>(RepIndexCount);
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberDebugDescriptors(FReplicationStateMemberDebugDescriptor* MemberDebugDescriptors, FReplicationStateDescriptor* Descriptor, const FMemberCache& MemberCache) const
{
	TCHAR StringBuffer[1024];

	FReplicationStateMemberDebugDescriptor* CurrentMemberDebugDescriptor = MemberDebugDescriptors;
	for (const FMemberProperty& Info : Members)
	{
		Info.Property->GetFName().ToString(StringBuffer, 1024);
		CurrentMemberDebugDescriptor->DebugName = CreatePersistentNetDebugName(StringBuffer);
		
		++CurrentMemberDebugDescriptor;
	}

	// Fill in descriptor
	Descriptor->MemberDebugDescriptors = MemberDebugDescriptors;
}

void FPropertyReplicationStateDescriptorBuilder::AddSerializerConfigMemoryBlockToLayout(FMemoryLayoutUtil::FLayout& Layout, FOffsetAndSize& OutOffsetAndSize, const FMemberCache& MemberCache) const
{
	SIZE_T OffsetOfFirstSerializerWithCustomConfig = ~SIZE_T(0);

	FOffsetAndSize DummyConfigOffsetAndSize;
	for (const FMemberCacheEntry& MemberCacheEntry : MemberCache)
	{
		if (MemberCacheEntry.bUseSerializerDefaultConfig)
		{
			continue;
		}

		// Store offset to config of first member in need of a custom config
		if (OffsetOfFirstSerializerWithCustomConfig == ~SIZE_T(0))
		{
			OffsetOfFirstSerializerWithCustomConfig = Align(Layout.CurrentOffset, MemberCacheEntry.SerializerConfigSizeAndAlignment.Alignment);
		}
		
		FMemoryLayoutUtil::AddToLayout(Layout, DummyConfigOffsetAndSize, MemberCacheEntry.SerializerConfigSizeAndAlignment.Size, MemberCacheEntry.SerializerConfigSizeAndAlignment.Alignment, 1);
	}

	// Calculate size of the section
	OutOffsetAndSize.Offset = (OffsetOfFirstSerializerWithCustomConfig == ~SIZE_T(0) ? Layout.CurrentOffset : OffsetOfFirstSerializerWithCustomConfig);
	OutOffsetAndSize.Size = Layout.CurrentOffset - OutOffsetAndSize.Offset;
}

void FPropertyReplicationStateDescriptorBuilder::BuildMemberSerializerConfigs(uint8* SerializerConfigBuffer, FReplicationStateDescriptor* Descriptor, const FMemberCache& MemberCache) const
{
	const FMemberCacheEntry* MemberCacheEntry = MemberCache.GetData();
	FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptor = const_cast<FReplicationStateMemberSerializerDescriptor*>(Descriptor->MemberSerializerDescriptors);

	for (const FMemberProperty& Member : Members)
	{
		// Construct config in provided memory and update descriptor
		if (MemberCacheEntry->bUseSerializerDefaultConfig)
		{
			checkf(MemberCacheEntry->Serializer->DefaultConfig != nullptr, TEXT("Expected NetSerializer %s to have a default config for property %s::%s"), MemberCacheEntry->Serializer->Name, *Member.Property->GetOwner<UObject>()->GetFName().ToString(), *Member.Property->GetFName().ToString());
			MemberSerializerDescriptor->SerializerConfig = MemberCacheEntry->Serializer->DefaultConfig;
		}
		else
		{
			SerializerConfigBuffer = Align(SerializerConfigBuffer, MemberCacheEntry->SerializerConfigSizeAndAlignment.Alignment);

			// Special case for structs
			if (IsStructNetSerializer(MemberCacheEntry->Serializer))
			{
				FStructNetSerializerConfig* SerializerConfig = new (SerializerConfigBuffer) FStructNetSerializerConfig();
				SerializerConfig->StateDescriptor = MemberCacheEntry->Descriptor;
				MemberSerializerDescriptor->SerializerConfig = SerializerConfig;
			}
			else if (IsArrayPropertyNetSerializer(MemberCacheEntry->Serializer))
			{
				FArrayPropertyNetSerializerConfig* SerializerConfig = new (SerializerConfigBuffer) FArrayPropertyNetSerializerConfig();
				SerializerConfig->MaxElementCount = 65535U;
				SerializerConfig->ElementCountBitCount = GetBitsNeeded(SerializerConfig->MaxElementCount);
				SerializerConfig->Property = TFieldPath<FArrayProperty>(const_cast<FArrayProperty*>(CastField<FArrayProperty>(Member.Property)));
				SerializerConfig->StateDescriptor = MemberCacheEntry->Descriptor;
				MemberSerializerDescriptor->SerializerConfig = SerializerConfig;
			}
			else
			{
				const FNetSerializerConfig* SerializerConfig = Member.SerializerInfo->BuildNetSerializerConfig(SerializerConfigBuffer, Member.Property);
				MemberSerializerDescriptor->SerializerConfig = SerializerConfig;
			}

			SerializerConfigBuffer += MemberCacheEntry->SerializerConfigSizeAndAlignment.Size;
		}

		++MemberCacheEntry;
		++MemberSerializerDescriptor;
	}	
}

// Need to do this to support role swapping.
void FPropertyReplicationStateDescriptorBuilder::FixupNetRoleNetSerializerConfigs(FReplicationStateDescriptor* Descriptor)
{
	FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = const_cast<FReplicationStateMemberSerializerDescriptor*>(Descriptor->MemberSerializerDescriptors);

	uint32 FirstNetRoleMemberIndex = ~0U;
	uint32 SecondNetRoleMemberIndex = ~0U;

	for (const FReplicationStateMemberSerializerDescriptor& SerializerDescriptor : MakeArrayView(MemberSerializerDescriptors, Descriptor->MemberCount))
	{
		if (SerializerDescriptor.Serializer == &UE_NET_GET_SERIALIZER(FNetRoleNetSerializer))
		{
			const uint32 MemberIndex = &SerializerDescriptor - MemberSerializerDescriptors;
			if (FirstNetRoleMemberIndex == ~0U)
			{
				FirstNetRoleMemberIndex = MemberIndex;
			}
			else
			{
				SecondNetRoleMemberIndex = MemberIndex;
				break;
			}
		}
	}

	if (SecondNetRoleMemberIndex != ~0U)
	{
		const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
		const int32 InternalOffsetFromFirstToSecondNetRole = MemberDescriptors[SecondNetRoleMemberIndex].InternalMemberOffset - MemberDescriptors[FirstNetRoleMemberIndex].InternalMemberOffset;
		const int32 ExternalOffsetFromFirstToSecondNetRole = MemberDescriptors[SecondNetRoleMemberIndex].ExternalMemberOffset - MemberDescriptors[FirstNetRoleMemberIndex].ExternalMemberOffset;

		FReplicationStateMemberSerializerDescriptor& FirstSerializerDescriptor = MemberSerializerDescriptors[FirstNetRoleMemberIndex];
		FReplicationStateMemberSerializerDescriptor& SecondSerializerDescriptor = MemberSerializerDescriptors[SecondNetRoleMemberIndex];

		FNetRoleNetSerializerConfig* FirstConfig = static_cast<FNetRoleNetSerializerConfig*>(const_cast<FNetSerializerConfig*>(FirstSerializerDescriptor.SerializerConfig));
		FNetRoleNetSerializerConfig* SecondConfig = static_cast<FNetRoleNetSerializerConfig*>(const_cast<FNetSerializerConfig*>(SecondSerializerDescriptor.SerializerConfig));

		FirstConfig->RelativeInternalOffsetToOtherRole = InternalOffsetFromFirstToSecondNetRole;
		FirstConfig->RelativeExternalOffsetToOtherRole = ExternalOffsetFromFirstToSecondNetRole;

		SecondConfig->RelativeInternalOffsetToOtherRole = -InternalOffsetFromFirstToSecondNetRole;
		SecondConfig->RelativeExternalOffsetToOtherRole = -ExternalOffsetFromFirstToSecondNetRole;
	}
}

void FPropertyReplicationStateDescriptorBuilder::FixupDescriptorForNativeFastArray(FReplicationStateDescriptor* Descriptor) const
{
	// Native fast arrays quantized directly from the src state data which is the fastarray itself so the offset must be set to zero.
	const_cast<FReplicationStateMemberDescriptor*>(&Descriptor->MemberDescriptors[0])->ExternalMemberOffset = 0U;
	Descriptor->ChangeMasksExternalOffset = GetFastArrayChangeMaskOffset(Descriptor->MemberProperties[0]);
}

EReplicationStateTraits FPropertyReplicationStateDescriptorBuilder::BuildReplicationStateTraits(const FBuilderContext& Context) const
{
	const EMemberPropertyTraits PropertyTraits = Context.CombinedPropertyTraits;
	const EMemberPropertyTraits SharedPropertyTraits = Context.SharedPropertyTraits;
	const bool bIsInitState = Context.IsInitState();

	EReplicationStateTraits Traits = EReplicationStateTraits::None;

	// Init state does not support conditionals as it has no changemask.
	if (EnumHasAnyFlags(PropertyTraits, EMemberPropertyTraits::HasLifetimeConditionals) && !bIsInitState)
	{
		Traits |= EReplicationStateTraits::HasLifetimeConditionals;
	}

	if (bIsInitState)
	{
		Traits |= EReplicationStateTraits::InitOnly;
	}

	// Has Rep notifies
	if (EnumHasAnyFlags(PropertyTraits, EMemberPropertyTraits::RepNotifyAlways | EMemberPropertyTraits::RepNotifyOnChanged))
	{
		Traits |= EReplicationStateTraits::HasRepNotifies;

		// Do we need to keep a copy of the previous state for rep notifies?
		if (EnumHasAnyFlags(PropertyTraits, EMemberPropertyTraits::NeedPreviousState))
		{
			Traits |= EReplicationStateTraits::KeepPreviousState;
		}
	}

	if (EnumHasAnyFlags(PropertyTraits, EMemberPropertyTraits::HasDynamicState))
	{
		Traits |= EReplicationStateTraits::HasDynamicState;
	}

	if (EnumHasAnyFlags(PropertyTraits, EMemberPropertyTraits::HasObjectReference))
	{
		Traits |= EReplicationStateTraits::HasObjectReference;
	}

	if (EnumHasAnyFlags(PropertyTraits, EMemberPropertyTraits::HasConnectionSpecificSerialization))
	{
		Traits |= EReplicationStateTraits::HasConnectionSpecificSerialization;
	}

	// Only set the PushBasedDirtiness trait if all properties are push based. An alternative could be to split into multiple states.
	if (EnumHasAnyFlags(SharedPropertyTraits, EMemberPropertyTraits::HasPushBasedDirtiness))
	{
		Traits |= EReplicationStateTraits::HasPushBasedDirtiness;
	}

	// Special traits when all members are replicated
	if (Context.BuildParams.bAllMembersAreReplicated)
	{
		// We cannot determine whether all properties are trivially constructible or destructible unless they're all replicated.
		if (EnumHasAnyFlags(SharedPropertyTraits, EMemberPropertyTraits::IsSourceTriviallyConstructible))
		{
			Traits |= EReplicationStateTraits::IsSourceTriviallyConstructible;
		}

		if (EnumHasAnyFlags(SharedPropertyTraits, EMemberPropertyTraits::IsSourceTriviallyDestructible))
		{
			Traits |= EReplicationStateTraits::IsSourceTriviallyDestructible;
		}

		Traits |= EReplicationStateTraits::AllMembersAreReplicated;
	}

	if (EnumHasAnyFlags(SharedPropertyTraits, EMemberPropertyTraits::IsFastArray) && Members.Num() == 1)
	{
		Traits |= EReplicationStateTraits::IsFastArrayReplicationState;
		Traits |= EnumHasAnyFlags(PropertyTraits, EMemberPropertyTraits::IsNativeFastArray) ? EReplicationStateTraits::IsNativeFastArrayReplicationState : EReplicationStateTraits::None;
	}

	// There might be cases where there are members but none of them consumes bandwidth.
	// It's ok to mark such states as supporting delta compression even though it makes little sense.
	// At this stage it's not possible to know it.
	if (!bIsInitState && Context.BuildParams.DescriptorType != EDescriptorType::Function && Members.Num() > 0)
	{
		Traits |= EReplicationStateTraits::SupportsDeltaCompression;
	}

	Traits |= EReplicationStateTraits::NeedsRefCount;

	return Traits;
}

void FPropertyReplicationStateDescriptorBuilder::FinalizeDescriptor(FReplicationStateDescriptor* Descriptor, FBuilderContext& Context) const
{
	Descriptor->ExternalAlignment = Context.External.Alignment;
	Descriptor->ExternalSize = Context.External.Size;

	Descriptor->InternalAlignment = Context.Internal.Alignment;
	Descriptor->InternalSize = Context.Internal.Size;	
}

TRefCountPtr<const FReplicationStateDescriptor>
FPropertyReplicationStateDescriptorBuilder::Build(const FString& StateName, FReplicationStateDescriptorRegistry* DescriptorRegistry, const FBuildParameters& BuildParams)
{
	// Cache information required to calculate layout
	const SIZE_T MemberCount = Members.Num();
	const SIZE_T FunctionCount = Functions.Num();

	FBuilderContext Context;
	Context.BuildParams = BuildParams;
	BuildMemberCache(Context, DescriptorRegistry);
	BuildMemberReferenceCache(Context.ReferenceCache, Context.MemberCache);
	BuildMemberTagCache(Context.MemberTagCache, Context.MemberCache);
	BuildMemberFunctionCache(Context, DescriptorRegistry);

	// We need to setup the traits early on as we use them to decide what sections we need to include in the descriptor
	const EReplicationStateTraits Traits = BuildReplicationStateTraits(Context);

	// Layout for our memory allocation
	struct FReplicationDescriptorLayout
	{
		FOffsetAndSize ReplicationStateDescriptorSizeAndOffset;
		FOffsetAndSize MemberDescriptorSizeAndOffset;
		FOffsetAndSize MemberChangeMaskDescriptorSizeAndOffset;	
		FOffsetAndSize MemberSerializerDescriptorSizeAndOffset;
		FOffsetAndSize MemberTraitsDescriptorSizeAndOffset;
		FOffsetAndSize MemberTagDescriptorSizeAndOffset;
		FOffsetAndSize MemberReferenceDescriptorSizeAndOffset;
		FOffsetAndSize MemberFunctionDescriptorSizeAndOffset;
		FOffsetAndSize MemberPropertiesSizeAndOffset;
		FOffsetAndSize MemberPropertyDescriptorsSizeAndOffset;
		FOffsetAndSize MemberLifetimeConditionDescriptorsSizeAndOffset;
		FOffsetAndSize MemberRepIndexToMemberIndexDescriptorsSizeAndOffset;
		FOffsetAndSize MemberDebugDescriptorsSizeAndOffset;
		FOffsetAndSize MemberSerializerConfigSizeAndOffset;
	};
	FReplicationDescriptorLayout LayoutData = {};
	
	// Memory layout 
	FMemoryLayoutUtil::FLayout Layout;

	// The descriptor itself
	FMemoryLayoutUtil::AddToLayout<FReplicationStateDescriptor>(Layout, LayoutData.ReplicationStateDescriptorSizeAndOffset, 1);

	// MemberDescriptors
	FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberDescriptor>(Layout, LayoutData.MemberDescriptorSizeAndOffset, MemberCount);

	// MemberChangeMaskDescriptors
	if (Context.BuildParams.DescriptorType == FPropertyReplicationStateDescriptorBuilder::EDescriptorType::Class)
	{
		FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberChangeMaskDescriptor>(Layout, LayoutData.MemberChangeMaskDescriptorSizeAndOffset, MemberCount);
	}

	// MemberSerializerDescriptor
	FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberSerializerDescriptor>(Layout, LayoutData.MemberSerializerDescriptorSizeAndOffset, MemberCount);

	// MemberTraitsDescriptor
	FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberTraitsDescriptor>(Layout, LayoutData.MemberTraitsDescriptorSizeAndOffset, MemberCount);

	// MemberTagDescriptor
	FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberTagDescriptor>(Layout, LayoutData.MemberTagDescriptorSizeAndOffset, Context.MemberTagCache.TagCount);

	// MemberReferenceDescriptor
	FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberReferenceDescriptor>(Layout, LayoutData.MemberReferenceDescriptorSizeAndOffset, Context.ReferenceCache.ReferenceCount);

	// MemberFunctionDescriptors
	FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberFunctionDescriptor>(Layout, LayoutData.MemberFunctionDescriptorSizeAndOffset, FunctionCount);

	// MemberProperties
	FMemoryLayoutUtil::AddToLayout<FProperty*>(Layout, LayoutData.MemberPropertiesSizeAndOffset, MemberCount);

	// MemberPropertyDescriptors
	FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberPropertyDescriptor>(Layout, LayoutData.MemberPropertyDescriptorsSizeAndOffset, MemberCount);

	// MemberLifetimeConditionDescriptors
	if (EnumHasAnyFlags(Traits, EReplicationStateTraits::HasLifetimeConditionals))
	{
		FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberLifetimeConditionDescriptor>(Layout, LayoutData.MemberLifetimeConditionDescriptorsSizeAndOffset, MemberCount);
	}

	// MemberRepIndexToMemberIndexDescriptors
	if (Context.BuildParams.DescriptorType == EDescriptorType::Class || Context.BuildParams.DescriptorType == EDescriptorType::SingleProperty)
	{
		if (Members.Num() > 0)
		{
			uint16 MaxRepIndex = 0;
			for (const FMemberProperty& Member : Members)
			{
				MaxRepIndex = FPlatformMath::Max(MaxRepIndex, Member.Property->RepIndex);
			}
			const uint32 RepIndexCount = static_cast<uint32>(MaxRepIndex) + 1U;
			FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberRepIndexToMemberIndexDescriptor>(Layout, LayoutData.MemberRepIndexToMemberIndexDescriptorsSizeAndOffset, RepIndexCount);
		}
	}

	// $TODO: generate this based on config/define 
	if (1)
	{
		FMemoryLayoutUtil::AddToLayout<FReplicationStateMemberDebugDescriptor>(Layout, LayoutData.MemberDebugDescriptorsSizeAndOffset, MemberCount);
	}

	// Reserve space all NetSerializerConfig instances we need to allocate
	AddSerializerConfigMemoryBlockToLayout(Layout, LayoutData.MemberSerializerConfigSizeAndOffset, Context.MemberCache);

	// Allocate required memory for descriptor and space for all sections
	uint8* Buffer = (uint8*)FMemory::Malloc(Layout.CurrentOffset, Layout.MaxAlignment);

	if (Buffer)
	{
		FMemory::Memzero(Buffer, Layout.CurrentOffset);
	
		FReplicationStateDescriptor* Descriptor = new (Buffer + LayoutData.ReplicationStateDescriptorSizeAndOffset.Offset) FReplicationStateDescriptor;

		check((void*)Descriptor == (void*)Buffer);
		
		// Setup Traits early on
		Descriptor->Traits = Traits;

		// Setup descriptor
		if (Context.BuildParams.DescriptorType == FPropertyReplicationStateDescriptorBuilder::EDescriptorType::Class)
		{
			// Reserve space for internal state data
			BuildReplicationStateInternal(Descriptor, Context);

			// Build member statemask descriptors
			BuildMemberChangeMaskDescriptors(reinterpret_cast<FReplicationStateMemberChangeMaskDescriptor*>(Buffer + LayoutData.MemberChangeMaskDescriptorSizeAndOffset.Offset), Descriptor, Context);
		}
			
		// Build member descriptors
		{
			// There's some logic in here that need to be executed even if there are zero replicated members
			FReplicationStateMemberDescriptor* MemberDescriptor = nullptr;
			if (LayoutData.MemberDescriptorSizeAndOffset.Size > 0)
			{
				MemberDescriptor = reinterpret_cast<FReplicationStateMemberDescriptor*>(Buffer + LayoutData.MemberDescriptorSizeAndOffset.Offset);
			}

			if ((Context.BuildParams.DescriptorType == EDescriptorType::Struct) || (Context.BuildParams.DescriptorType == EDescriptorType::Function))
			{
				BuildMemberDescriptorsForStruct(MemberDescriptor, Descriptor, Context);
			}
			else
			{
				BuildMemberDescriptors(MemberDescriptor, Descriptor, Context);
			}
		}

		// Build member serializer descriptors
		if (LayoutData.MemberSerializerDescriptorSizeAndOffset.Size > 0)
		{
			BuildMemberSerializerDescriptors(reinterpret_cast<FReplicationStateMemberSerializerDescriptor*>(Buffer + LayoutData.MemberSerializerDescriptorSizeAndOffset.Offset), Descriptor);
		}
	
		// Build member traits descriptors
		if (LayoutData.MemberTraitsDescriptorSizeAndOffset.Size > 0)
		{
			BuildMemberTraitsDescriptors(reinterpret_cast<FReplicationStateMemberTraitsDescriptor*>(Buffer + LayoutData.MemberTraitsDescriptorSizeAndOffset.Offset), Descriptor, Context);
		}

		// Build member RepTag descriptors
		if (LayoutData.MemberTagDescriptorSizeAndOffset.Size > 0)
		{
			BuildMemberTagDescriptors(reinterpret_cast<FReplicationStateMemberTagDescriptor*>(Buffer + LayoutData.MemberTagDescriptorSizeAndOffset.Offset), Descriptor, Context);
		}

		// Build member Reference descriptors
		if (LayoutData.MemberReferenceDescriptorSizeAndOffset.Size > 0)
		{
			BuildMemberReferenceDescriptors(reinterpret_cast<FReplicationStateMemberReferenceDescriptor*>(Buffer + LayoutData.MemberReferenceDescriptorSizeAndOffset.Offset), Descriptor, Context);
		}

		if (LayoutData.MemberFunctionDescriptorSizeAndOffset.Size > 0)
		{
			BuildMemberFunctionDescriptors(reinterpret_cast<FReplicationStateMemberFunctionDescriptor*>(Buffer + LayoutData.MemberFunctionDescriptorSizeAndOffset.Offset), Descriptor, Context);
		}

		// Init property pointers
		if (LayoutData.MemberPropertiesSizeAndOffset.Size > 0)
		{
			BuildMemberProperties(reinterpret_cast<const FProperty**>(Buffer + LayoutData.MemberPropertiesSizeAndOffset.Offset), Descriptor);
		}

		// Build MemberPropertyDescriptors
		if (LayoutData.MemberPropertyDescriptorsSizeAndOffset.Size > 0)
		{
			BuildMemberPropertyDescriptors(reinterpret_cast<FReplicationStateMemberPropertyDescriptor*>(Buffer + LayoutData.MemberPropertyDescriptorsSizeAndOffset.Offset), Descriptor);
		}

		// Build MemberLifetimeConditionDescriptors
		if (LayoutData.MemberLifetimeConditionDescriptorsSizeAndOffset.Size > 0)
		{
			BuildMemberLifetimeConditionDescriptors(reinterpret_cast<FReplicationStateMemberLifetimeConditionDescriptor*>(Buffer + LayoutData.MemberLifetimeConditionDescriptorsSizeAndOffset.Offset), Descriptor);
		}

		// Build MemberRepIndexToMemberDescriptors
		if (LayoutData.MemberRepIndexToMemberIndexDescriptorsSizeAndOffset.Size > 0)
		{
			BuildMemberRepIndexToMemberDescriptors(reinterpret_cast<FReplicationStateMemberRepIndexToMemberIndexDescriptor*>(Buffer + LayoutData.MemberRepIndexToMemberIndexDescriptorsSizeAndOffset.Offset), Descriptor);
		}

		Descriptor->DebugName = CreatePersistentNetDebugName(StateName.GetCharArray().GetData());
		if (LayoutData.MemberDebugDescriptorsSizeAndOffset.Size > 0)
		{
			BuildMemberDebugDescriptors(reinterpret_cast<FReplicationStateMemberDebugDescriptor*>(Buffer + LayoutData.MemberDebugDescriptorsSizeAndOffset.Offset), Descriptor, Context.MemberCache);
		}

		// Construct all MemberSerializerConfigs and update pointers in MemberSerializerDescriptors.
		// Should be called even if LayoutData.MemberSerializerConfigSizeAndOffset.Size is zero.
		BuildMemberSerializerConfigs(Buffer + LayoutData.MemberSerializerConfigSizeAndOffset.Offset, Descriptor, Context.MemberCache);

		if (Context.BuildParams.DescriptorType == EDescriptorType::Class)
		{
			FixupNetRoleNetSerializerConfigs(Descriptor);
		}

		// Need a way to detect that we should fixup descriptor for native use
		if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::IsNativeFastArrayReplicationState))
		{
			FixupDescriptorForNativeFastArray(Descriptor);
		}

		// Finalize the descriptor and fill in remaining information
		FinalizeDescriptor(Descriptor, Context);

		// Pad up the size to the alignment of the state
		Descriptor->ExternalSize = Align(Descriptor->ExternalSize, Descriptor->ExternalAlignment);
		Descriptor->InternalSize = Align(Descriptor->InternalSize, Descriptor->InternalAlignment);

		// Setup function pointers to required methods
		Descriptor->ConstructReplicationState = ConstructPropertyReplicationState;
		Descriptor->DestructReplicationState = DestructPropertyReplicationState;
		if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::IsFastArrayReplicationState))
		{
			Descriptor->CreateAndRegisterReplicationFragmentFunction = Members[0].CreateAndRegisterReplicationFragmentFunction;
		}

		// Use the full pathname as part of the identifier to ensure that it is unique
		uint64 Seed = 0;
		const uint32 PathLen(BuildParams.PathName ? BuildParams.PathName->Len() : 0);
		if (PathLen)
		{
			Seed = CityHash64((const char*)BuildParams.PathName->GetData(), PathLen * sizeof(FStringBuilderBase::ElementType));
		}
		FString UpperCaseNameForHash(StateName);
		UpperCaseNameForHash.ToUpperInline();
		Descriptor->DescriptorIdentifier.Value = CityHash64WithSeed((const char*)UpperCaseNameForHash.GetCharArray().GetData(), sizeof(TCHAR) * UpperCaseNameForHash.GetCharArray().Num(), Seed);

		UE_LOG_DESCRIPTORBUILDER(Verbose, TEXT("Generated identifier(0x%" UINT64_x_FMT ") From %s:%s"), Descriptor->DescriptorIdentifier.Value, PathLen ? BuildParams.PathName->ToString() : TEXT(""), *StateName);

		if (Descriptor->InternalSize && BuildParams.DefaultStateSourceData)
		{
			// Initialize default state, we need to do this after finalizing the descriptor as we use the descriptor to iterate over the members
			AllocateAndInitializeDefaultInternalStateBuffer(BuildParams.DefaultStateSourceData, Descriptor, Descriptor->DefaultStateBuffer);

			uint64 DefaultStateHash = uint64(0);
			if (CalculateDefaultStateChecksum(Descriptor, Descriptor->DefaultStateBuffer, DefaultStateHash))
			{
				// Store the default state checksum for later use when verifying protocol
				Descriptor->DescriptorIdentifier.DefaultStateHash = DefaultStateHash;
			}
		
		}

		return TRefCountPtr<const FReplicationStateDescriptor>(Descriptor);
	}
	else
	{
		return TRefCountPtr<const FReplicationStateDescriptor>(nullptr);
	}
}

// Helper for GetConnectionFilterTrait. Needs to live outside of function until we can use constepr lambdas (C++17).
template<typename ConditionPair, SIZE_T Size>
constexpr static bool TConditionPairIsSorted(const ConditionPair (&Array)[Size])
{
	if (Size == 0)
	{
		return true;
	}

	for (SIZE_T It = 0, EndIt = Size - 1; It < EndIt; ++It)
	{
		if (Array[It].Condition >= Array[It + 1].Condition)
		{
			return false;
		}
	}

	return true;
};

// Util methods
EMemberPropertyTraits FPropertyReplicationStateDescriptorBuilder::GetConnectionFilterTrait(ELifetimeCondition Condition)
{
	EMemberPropertyTraits Traits = EMemberPropertyTraits::None;

	struct FConditionPair
	{
		ELifetimeCondition Condition;
		EMemberPropertyTraits PropertyTrait;
	};

	constexpr FConditionPair ConditionPairs[] =
	{
		{COND_OwnerOnly, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_SkipOwner, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_SimulatedOnly, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_AutonomousOnly, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_SimulatedOrPhysics, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_InitialOrOwner, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_Custom, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_ReplayOrOwner, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_ReplayOnly, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_SimulatedOnlyNoReplay, EMemberPropertyTraits::HasLifetimeConditionals},
		{COND_SimulatedOrPhysicsNoReplay, EMemberPropertyTraits::HasLifetimeConditionals},
	};

	static_assert(TConditionPairIsSorted<>(ConditionPairs), "ConditionPairs order needs to be updated.");
	
	const int32 ConditionPairIndex = Algo::BinarySearchBy(ConditionPairs, Condition, [](const FConditionPair& Pair) { return Pair.Condition; });

	if (ConditionPairIndex >= 0)
	{
		Traits |= ConditionPairs[ConditionPairIndex].PropertyTrait;
	}
	
	return Traits;
}

EMemberPropertyTraits FPropertyReplicationStateDescriptorBuilder::GetInitOnlyTrait(ELifetimeCondition Condition)
{ 
	bool bResult = 
		Condition == COND_InitialOnly ||
		Condition == COND_InitialOrOwner;

	return bResult ? EMemberPropertyTraits::InitOnly : EMemberPropertyTraits::None;
}

EMemberPropertyTraits FPropertyReplicationStateDescriptorBuilder::GetHasObjectReferenceTraits(const FNetSerializer* NetSerializer)
{
	EMemberPropertyTraits Traits = EMemberPropertyTraits::None;
	Traits |= EnumHasAnyFlags(NetSerializer->Traits, ENetSerializerTraits::HasCustomNetReference)  ? (EMemberPropertyTraits::HasObjectReference | EMemberPropertyTraits::HasCustomObjectReference) : EMemberPropertyTraits::None;
	Traits |= IsObjectReferenceNetSerializer(NetSerializer) ? EMemberPropertyTraits::HasObjectReference : EMemberPropertyTraits::None;

	return Traits;
}

void FPropertyReplicationStateDescriptorBuilder::GetIrisPropertyTraits(FMemberProperty& OutMemberProperty, const FProperty* Property, const TArray<FLifetimeProperty>* LifeTimeProperties, UClass* ObjectClass)
{
	EMemberPropertyTraits Traits = EMemberPropertyTraits::None;

	const uint16 RepIndex = Property->RepIndex;

	// We assume we can use the LifetimeProperty data even if the RepIndex would not match.
	const FLifetimeProperty* Data = LifeTimeProperties ? &(*LifeTimeProperties)[RepIndex] : nullptr;

	const UFunction* PropertyRepNotifyFunction = nullptr;
	CreateAndRegisterReplicationFragmentFunc CreateAndRegisterReplicationFragmentFunction = nullptr;

	Traits |= ((Property->PropertyFlags & (CPF_NoDestructor | CPF_IsPlainOldData)) != 0 ? EMemberPropertyTraits::IsSourceTriviallyDestructible : EMemberPropertyTraits::None);
	Traits |= ((Property->PropertyFlags & (CPF_ZeroConstructor | CPF_IsPlainOldData)) != 0 ? EMemberPropertyTraits::IsSourceTriviallyConstructible : EMemberPropertyTraits::None);

	if (Data)
	{
		Traits |= GetConnectionFilterTrait(Data->Condition);
		Traits |= GetInitOnlyTrait(Data->Condition);
		Traits |= (Data->bIsPushBased ? EMemberPropertyTraits::HasPushBasedDirtiness : EMemberPropertyTraits::None);
		
		if (Property->RepNotifyFunc.IsNone() == false)
		{
			PropertyRepNotifyFunction = ObjectClass->FindFunctionByName(Property->RepNotifyFunc);

			Traits |= Data->RepNotifyCondition == ELifetimeRepNotifyCondition::REPNOTIFY_OnChanged ? EMemberPropertyTraits::RepNotifyOnChanged : EMemberPropertyTraits::None;
			Traits |= Data->RepNotifyCondition == ELifetimeRepNotifyCondition::REPNOTIFY_Always ? EMemberPropertyTraits::RepNotifyAlways : EMemberPropertyTraits::None;
			Traits |= PropertyRepNotifyFunction->NumParms > 0 ? EMemberPropertyTraits::NeedPreviousState : EMemberPropertyTraits::None;

			// We currently DO NOT support the random case where NumParms is more than one (MetaData?)
			check(PropertyRepNotifyFunction->NumParms < 2)
		}

		CreateAndRegisterReplicationFragmentFunction = Data->CreateAndRegisterReplicationFragmentFunction;
	}

	OutMemberProperty.PropertyRepNotifyFunction = PropertyRepNotifyFunction;
	OutMemberProperty.Traits = Traits;
	OutMemberProperty.ReplicationCondition = (Data ? Data->Condition : COND_None);
	OutMemberProperty.CreateAndRegisterReplicationFragmentFunction = CreateAndRegisterReplicationFragmentFunction;
}

void FPropertyReplicationStateDescriptorBuilder::GetSerializerTraits(FMemberProperty& OutMemberProperty, const FProperty* Property, const FPropertyNetSerializerInfo* NetSerializerInfo)
{
	const FNetSerializer* NetSerializer = NetSerializerInfo->GetNetSerializer(Property);

	OutMemberProperty.Traits |= GetHasObjectReferenceTraits(NetSerializer);
	OutMemberProperty.Traits |= EnumHasAnyFlags(NetSerializer->Traits, ENetSerializerTraits::HasDynamicState) ? EMemberPropertyTraits::HasDynamicState : EMemberPropertyTraits::None;
	OutMemberProperty.Traits |= EnumHasAnyFlags(NetSerializer->Traits, ENetSerializerTraits::HasConnectionSpecificSerialization) ? EMemberPropertyTraits::HasConnectionSpecificSerialization : EMemberPropertyTraits::None;
	OutMemberProperty.Traits |= GetFastArrayPropertyTraits(NetSerializer, Property);
}

bool FPropertyReplicationStateDescriptorBuilder::IsSupportedProperty(FMemberProperty& OutMemberProperty, const FProperty* Property, const TArray<FLifetimeProperty>* LifeTimeProperties, UClass* InObjectClass)
{
	// A property is supported if it has a registered SerializerInfo.
	const FPropertyNetSerializerInfo* NetSerializerInfo = FPropertyNetSerializerInfoRegistry::FindSerializerInfo(Property);
	if (!NetSerializerInfo)
	{
		return false;
	}

	// Init info
	OutMemberProperty.Property = Property;
	OutMemberProperty.SerializerInfo = NetSerializerInfo;
	OutMemberProperty.ChangeMaskBits = 1u;
	OutMemberProperty.ExternalSizeAndAlignment = { 0, 0 };

	// Init traits
	GetIrisPropertyTraits(OutMemberProperty, Property, LifeTimeProperties, InObjectClass);
	GetSerializerTraits(OutMemberProperty, Property, NetSerializerInfo);

	// $IRIS: $TODO: Add property attribute to control how many changemask bits we want to use per property
	if (EnumHasAnyFlags(OutMemberProperty.Traits, EMemberPropertyTraits::IsFastArray))
	{
		OutMemberProperty.ChangeMaskBits = 1U + FIrisFastArraySerializer::IrisFastArrayChangeMaskBits;
	}

	return true;
}

bool FPropertyReplicationStateDescriptorBuilder::IsStructWithCustomSerializer(FMemberProperty& OutMemberProperty, const UStruct* InStruct)
{
	// See if we got a matching custom serializer for the entire struct
	const FPropertyNetSerializerInfo* NetSerializerInfo = FPropertyNetSerializerInfoRegistry::FindStructSerializerInfo(InStruct->GetFName());
	if (!NetSerializerInfo)
	{
		if (InStruct->GetSuperStruct())
		{
			const bool bSuperHasCustomNetSerializer = IsStructWithCustomSerializer(OutMemberProperty, InStruct->GetSuperStruct());

			// Warn if one of our supers have a CustomNetSerializer, if that is the case user must be made aware and either declare an explicit forwarding NetSerializer
			// or implement a custom NetSerializer
			const bool bIsAllowedToWarn = ShouldUseIrisReplication() || FApp::IsGame() || IsRunningDedicatedServer() || IsRunningClientOnly();
			UE_CLOG(bSuperHasCustomNetSerializer && bIsAllowedToWarn && bWarnAboutStructsWithCustomSerialization, LogIris, Warning, TEXT("Detected derived struct %s which do not have a custom NetSerializer while Super %s has a custom NetSerializer. Silence warning by declaring an explicit forwarding serializer or implement a custom NetSerialzier for the derived struct"), ToCStr(InStruct->GetName()), ToCStr(InStruct->GetSuperStruct()->GetName()));

			return bSuperHasCustomNetSerializer;
		}
		return false;
	}

	OutMemberProperty.Property = nullptr;
	OutMemberProperty.SerializerInfo = NetSerializerInfo;
	OutMemberProperty.ChangeMaskBits = 1U;
	OutMemberProperty.ExternalSizeAndAlignment = { (SIZE_T)InStruct->GetStructureSize(), (SIZE_T)InStruct->GetMinAlignment() };
	OutMemberProperty.ReplicationCondition = ELifetimeCondition::COND_None;
	OutMemberProperty.PropertyRepNotifyFunction = nullptr;
	
	// Init traits
	OutMemberProperty.Traits = EMemberPropertyTraits::None;
	GetSerializerTraits(OutMemberProperty, nullptr, NetSerializerInfo);

	return true;
}

// RepTags are not intended to be derived from arbitrary properties, but rather be explicitly declared on a member. This is a hack.
FRepTag FPropertyReplicationStateDescriptorBuilder::GetRepTagFromProperty(const FMemberCacheEntry& MemberCacheEntry, const FMemberProperty& MemberProperty)
{
	const FProperty* Property = MemberProperty.Property;
	const FName PropertyName = Property->GetFName();
	if (PropertyName == NAME_Role || PropertyName == NAME_RemoteRole)
	{
		if (MemberCacheEntry.Serializer == &UE_NET_GET_SERIALIZER(FNetRoleNetSerializer))
		{
			return PropertyName == NAME_Role ? RepTag_NetRole : RepTag_NetRemoteRole;
		}
	}
	else if (PropertyName == ReplicationStateDescriptorBuilder_NAME_NetCullDistanceSquared)
	{
		const FFieldClass* Class = Property->GetClass();
		if (Class->GetFName() == NAME_FloatProperty)
		{
			return RepTag_CullDistanceSqr;
		}
		else
		{
			UE_LOG_DESCRIPTORBUILDER_WARNING(TEXT("Found NetCullDistanceSquared property that is of type %s instead of a float. This prevents the property from being accessed by networking systems."), ToCStr(Class->GetFName().ToString()));
		}
	}
	// For unit testing purposes. Keep this code last.
#if IS_PROGRAM
	else
	{
		ANSICHAR PropertyNameString[NAME_SIZE];
		PropertyName.GetPlainANSIString(PropertyNameString);
		if (0 == FCStringAnsi::Strncmp(PropertyNameString, "NetTest_", 8))
		{
			return MakeRepTag(PropertyNameString);
		}
	}
#endif

	return GetInvalidRepTag();
}

void FPropertyReplicationStateDescriptorBuilder::GetPropertyPathName(const FProperty* Property, FString& PathName)
{
	constexpr uint32 MaxHierarchyDepth = 64;
	FFieldVariant PropertyChain[MaxHierarchyDepth];

	const UClass* Class = UClass::StaticClass();
	const UClass* Struct = UStruct::StaticClass();

	uint32 PropertyChainIndex = MaxHierarchyDepth;
	for (FFieldVariant Object = Property; Object.IsValid(); Object = Object.GetOwnerVariant())
	{
		if (!ensureAlwaysMsgf(PropertyChainIndex > 0, TEXT("Property hieararchy depth exceeds %u"), MaxHierarchyDepth))
		{
			break;
		}
		PropertyChain[--PropertyChainIndex] = Object;
		if (Object.IsA(Class) || Object.IsA(Struct))
		{
			break;
		}
	}

	PathName.Empty(512);
	for (; PropertyChainIndex < MaxHierarchyDepth; ++PropertyChainIndex)
	{
		const FFieldVariant& FieldVariant = PropertyChain[PropertyChainIndex];
		if (const UObject* Object = FieldVariant.ToUObject())
		{
			PathName.Append(Object->GetName()).AppendChar(TEXT('.'));
		}
		else if (const FField* Field = FieldVariant.ToField())
		{
			PathName.Append(Field->GetName()).Append((PropertyChainIndex == MaxHierarchyDepth - 1U) ? TEXT("") : TEXT("."));
		}
	}
}

SIZE_T FPropertyReplicationStateDescriptorBuilder::GetFastArrayChangeMaskOffset(const FProperty* Property)
{
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		const UScriptStruct* Struct = StructProperty->Struct;

		check(Struct->IsChildOf(FIrisFastArraySerializer::StaticStruct()));

		// Need to figure out if we are native, currently we do this by checking if we have a FastArrayChangeMaskMember
		const FProperty* ChangeMaskProperty = Struct->FindPropertyByName(PropertyNetSerializerRegistry_NAME_ChangeMaskStorage);
		if (ChangeMaskProperty)
		{
			// ChangeMask member must be repskip
			check(EnumHasAnyFlags(ChangeMaskProperty->PropertyFlags, EPropertyFlags::CPF_RepSkip));

			return ChangeMaskProperty->GetOffset_ForGC();
		}
	}

	return 0U;
}

EMemberPropertyTraits FPropertyReplicationStateDescriptorBuilder::GetFastArrayPropertyTraits(const FNetSerializer* NetSerializer, const FProperty* Property)
{
	if (IsStructNetSerializer(NetSerializer))
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			const UScriptStruct* Struct = StructProperty->Struct;
			const bool bUseNativeFastArray = CVarIrisUseNativeFastArray.GetValueOnAnyThread() != 0;

			if (bUseNativeFastArray && Struct->IsChildOf(FIrisFastArraySerializer::StaticStruct()))
			{
				return IsFastArraySupported(Struct) ? EMemberPropertyTraits::HasPushBasedDirtiness | EMemberPropertyTraits::IsFastArray | EMemberPropertyTraits::IsNativeFastArray : EMemberPropertyTraits::None;
			}
			else if (Struct->IsChildOf(FFastArraySerializer::StaticStruct()))
			{
				return IsFastArraySupported(Struct) ? EMemberPropertyTraits::IsFastArray : EMemberPropertyTraits::None;
			}
			else if (Struct->IsChildOf(FFastArraySerializerItem::StaticStruct()))
			{
				return EMemberPropertyTraits::IsFastArrayItem;
			}
		}
	}
	else if (IsArrayPropertyNetSerializer(NetSerializer))
	{
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
			{
				const UScriptStruct* Struct = StructProperty->Struct;
				if (Struct->IsChildOf(FFastArraySerializerItem::StaticStruct()))
				{
					return EMemberPropertyTraits::IsFastArrayItem;
				}
			}
		}
	}

	return EMemberPropertyTraits::None;
}

bool FPropertyReplicationStateDescriptorBuilder::IsFastArraySupported(const UStruct* Struct)
{
	uint32 ReplicatedPropertyCount = 0;

	FMemberProperty MemberProperty;
	if (IsStructWithCustomSerializer(MemberProperty, Struct))
	{
		return false;
	}

	// Try to figure out if this is a FastArraySerializer we can support. It is assumed the struct has already been determined to inherit from FastArraySerializer.
	// A proper implementation would contain a TArray of a struct that is a subclass of FastArraySerializerItem.
	for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		const FProperty* Property = *It;
		if (EnumHasAnyFlags(Property->PropertyFlags, EPropertyFlags::CPF_RepSkip))
		{
			continue;
		}

		++ReplicatedPropertyCount;
		if (ReplicatedPropertyCount > 1)
		{
			return false;
		}

		if (!FPropertyReplicationStateDescriptorBuilder::IsSupportedProperty(MemberProperty, Property))
		{
			return false;
		}

		if (!EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::IsFastArrayItem))
		{
			return false;
		}

		if (CastField<FArrayProperty>(MemberProperty.Property) == nullptr)
		{
			return false;
		}
	}

	return true;
}

}

namespace UE::Net
{

FReplicationStateDescriptorBuilder::FParameters::FParameters()
: DescriptorRegistry(nullptr),
  DefaultStateSource(nullptr),
  IncludeSuper(1U),
  GetLifeTimeProperties(1U),
  EnableFastArrayHandling(1U),
  SkipCheckForCustomNetSerializerForStruct(0U),
  SinglePropertyIndex(-1)
{
}

/**
 * FReplicationStateDescriptorBuilder Implementation
 */

// For a struct we only create a single state.
TRefCountPtr<const FReplicationStateDescriptor> FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(const UStruct* InStruct, const FParameters& Parameters)
{
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(FReplicationStateDescriptorBuilder_CreateDescriptorForStruct);

	// Check registry if we already have created descriptors for this class
	if (Parameters.DescriptorRegistry)
	{	
		if (auto Result = Parameters.DescriptorRegistry->Find(InStruct))
		{
			// For structs we do only expect a single entry
			ensure(Result->Num() == 1);

			return (*Result)[0];
		}
	}

	bool bIsFastArraySerializerItem = false;
	bool bIsFastArraySerializer = false;
	bool bIsStructWithCustomSerialization = false;
	if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InStruct))
	{
		bIsStructWithCustomSerialization = EnumHasAnyFlags(ScriptStruct->StructFlags, STRUCT_NetSerializeNative);

		bIsFastArraySerializerItem = ScriptStruct->IsChildOf(FFastArraySerializerItem::StaticStruct());
		bIsFastArraySerializer = ScriptStruct->IsChildOf(FFastArraySerializer::StaticStruct());
		if (bIsFastArraySerializer && !Parameters.EnableFastArrayHandling)
		{
			bIsFastArraySerializer = false;
			UE_LOG_DESCRIPTORBUILDER_WARNING(TEXT("FReplicationStateDescriptorBuilder ::CreateDescriptorForStruct Generating descriptor for FastArraySerializer as generic struct due to incompatible members. Struct is %s."), ToCStr(InStruct->GetName()));
		}
	}

	FPropertyReplicationStateDescriptorBuilder Builder;
	FPropertyReplicationStateDescriptorBuilder::FMemberProperty MemberProperty;

	// Struct members can opt out of replication.
	bool bAllMembersAreReplicated = true;

	// We have a special case for structs with custom serializers
	if (!Parameters.SkipCheckForCustomNetSerializerForStruct && FPropertyReplicationStateDescriptorBuilder::IsStructWithCustomSerializer(MemberProperty, InStruct))
	{
		Builder.AddMemberProperty(MemberProperty);
	}
	else
	{
		const bool bIsAllowedToWarn = ShouldUseIrisReplication() || FApp::IsGame() || IsRunningDedicatedServer() || IsRunningClientOnly();
		UE_CLOG(!Parameters.SkipCheckForCustomNetSerializerForStruct && bIsStructWithCustomSerialization && bIsAllowedToWarn && bWarnAboutStructsWithCustomSerialization, LogIris, Warning, TEXT("Generating descriptor for struct %s that has custom serialization."), ToCStr(InStruct->GetName()));

		for (TFieldIterator<FProperty> It(InStruct, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			const FProperty* Property = *It;
			if (EnumHasAnyFlags(Property->PropertyFlags, EPropertyFlags::CPF_RepSkip))
			{
				// For FastArraySerializerItems we currently include the ReplicationId even though it is not replicated
				if (!(bIsFastArraySerializerItem && (Property->GetFName() == PropertyNetSerializerRegistry_NAME_ReplicationID)))
				{
					bAllMembersAreReplicated = false;
					continue;
				}
			}

			if (FPropertyReplicationStateDescriptorBuilder::IsSupportedProperty(MemberProperty, Property))
			{
				// We do not support nested fast arrays
				if (EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::IsFastArray))
				{
					UE_LOG_DESCRIPTORBUILDER_WARNING(TEXT("FReplicationStateDescriptorBuilder ::CreateDescriptorForStruct Skipping FastArraySerializer %s.%s of type %s in struct which is not supported."), ToCStr(InStruct->GetName()), ToCStr(Property->GetName()), ToCStr(Property->GetCPPType()));
					continue;
				}

				// For FastArraySerializer we do not expect other properties than FastArrayItems
				if (bIsFastArraySerializer && !EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::IsFastArrayItem))
				{
					UE_LOG_DESCRIPTORBUILDER_WARNING(TEXT("FReplicationStateDescriptorBuilder ::CreateDescriptorForStruct FastArraySerializerStructs can only hold a single replicated property derived from FastArraySerializerItem. Skipping property: %s.%s of type %s."), ToCStr(InStruct->GetName()), ToCStr(Property->GetName()), ToCStr(Property->GetCPPType()));
					continue;
				}

				for (uint32 ArrayIt = 0, ArrayEndIt = Property->ArrayDim; ArrayIt < ArrayEndIt; ++ArrayIt)
				{
					Builder.AddMemberProperty(MemberProperty);
				}
			}
			else
			{
				UE_LOG_DESCRIPTORBUILDER_WARNING(TEXT("FReplicationStateDescriptorBuilder : Skipping unsupported struct member %s.%s of type %s."), ToCStr(InStruct->GetName()), ToCStr(Property->GetName()), ToCStr(Property->GetCPPType()));
			}
		}
	}

	Builder.SetStructSizeAndAlignment(InStruct->GetStructureSize(), InStruct->GetMinAlignment());

	FLazyGetPathNameHelper LazyPathHelper;
	FPropertyReplicationStateDescriptorBuilder::FBuildParameters BuildParameters = {};
	BuildParameters.PathName = LazyPathHelper.GetPathName(InStruct);
	BuildParameters.DescriptorType = FPropertyReplicationStateDescriptorBuilder::EDescriptorType::Struct;
	BuildParameters.bIsInitState = false;
	BuildParameters.bAllMembersAreReplicated = bAllMembersAreReplicated;

	auto Descriptor = Builder.Build(InStruct->GetName(), Parameters.DescriptorRegistry, BuildParameters);

	if (Parameters.DescriptorRegistry && Descriptor.IsValid())
	{
		Parameters.DescriptorRegistry->Register(InStruct, Descriptor);
	}

	return Descriptor;
};

TRefCountPtr<const FReplicationStateDescriptor> FReplicationStateDescriptorBuilder::CreateDescriptorForFunction(const UFunction* Function, const FParameters& Parameters)
{
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(FReplicationStateDescriptorBuilder_CreateDescriptorForFunction);

	// Check registry if we already have created descriptors for this function
	if (Parameters.DescriptorRegistry)
	{	
		if (auto Result = Parameters.DescriptorRegistry->Find(Function))
		{
			// For function we expect a single entry
			ensure(Result->Num() == 1);

			return (*Result)[0];
		}
	}

	FPropertyReplicationStateDescriptorBuilder Builder;
	FPropertyReplicationStateDescriptorBuilder::FMemberProperty MemberProperty;

	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++It)
	{
		const FProperty* Property = *It;
		if (Property->PropertyFlags & CPF_RepSkip)
		{
			continue;
		}
		
		checkfSlow(Property->ArrayDim == 1, TEXT("Unexpected parameter with array size. Function %s parameter %s."), ToCStr(Function->GetName()), ToCStr(Property->GetName()));

		if (FPropertyReplicationStateDescriptorBuilder::IsSupportedProperty(MemberProperty, Property))
		{
			Builder.AddMemberProperty(MemberProperty);
		}
		else
		{
			UE_LOG_DESCRIPTORBUILDER_WARNING(TEXT("FPropertyReplicationStateDescriptorBuilder: Skipping unsupported function parameter %s.%s of type %s."), ToCStr(Function->GetName()), ToCStr(Property->GetName()), ToCStr(Property->GetCPPType()));
		}
	}

	Builder.SetStructSizeAndAlignment(Function->ParmsSize, Function->GetMinAlignment());

	FPropertyReplicationStateDescriptorBuilder::FBuildParameters BuildParameters = {};
	BuildParameters.DescriptorType = FPropertyReplicationStateDescriptorBuilder::EDescriptorType::Function;
	BuildParameters.bIsInitState = false;
	// For this use case we don't care about function parameters that aren't replicated.
	BuildParameters.bAllMembersAreReplicated = true;
	auto Descriptor = Builder.Build(Function->GetName(), Parameters.DescriptorRegistry, BuildParameters);

	if (Parameters.DescriptorRegistry && Descriptor.IsValid())
	{
		Parameters.DescriptorRegistry->Register(Function, Descriptor);
	}

	return Descriptor;
}

struct FPropertyReplicationStateType
{
	const TCHAR* NamePostFix;
};

static constexpr FPropertyReplicationStateType InternalPropertyReplicationStateTypes[] = 
{
	{ TEXT("_Functions"), },
	{ TEXT("_InitOnly"), },
	{ TEXT("_LifetimeConditionals"), },
	{ TEXT("_State"), },
};

static constexpr SIZE_T FunctionsPropertyReplicationStateBuilderIndex = 0;
static constexpr SIZE_T InitPropertyReplicationStateBuilderIndex = 1;
static constexpr SIZE_T LifetimeConditionalsReplicationStateBuilderIndex = 2;
static constexpr SIZE_T RegularPropertyReplicationStateBuilderIndex = 3;

SIZE_T FReplicationStateDescriptorBuilder::CreateDescriptorsForClass(FResult& CreatedDescriptors, UClass* InObjectClass, const FParameters& Parameters)
{
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(FReplicationStateDescriptorBuilder_CreateDescriptorForClass);

	// Just to make sure we don't mess up the parameters when building descriptor for single property
	//check(Parameters.SinglePropertyIndex == -1 || Parameters.DescriptorRegistry == nullptr)

	const UObject* ObjectClassOrArchetypeUsedAsKey = Parameters.DefaultStateSource ? Parameters.DefaultStateSource : InObjectClass;

	// Check registry first to see if we already have created descriptors for this class
	if (Parameters.DescriptorRegistry)
	{
		if (auto Result = Parameters.DescriptorRegistry->Find(ObjectClassOrArchetypeUsedAsKey, InObjectClass))
		{
			CreatedDescriptors.Insert(*Result, 0);
			return CreatedDescriptors.Num();
		}
	}

	const bool bIncludeSuper = !!Parameters.IncludeSuper;
	const bool bGetLifeTimeProperties = !!Parameters.GetLifeTimeProperties;
	const bool bSingleProperty =  Parameters.SinglePropertyIndex != -1;
	const bool bRegisterCreatedDescriptors = Parameters.DescriptorRegistry && !bSingleProperty;

	{
		IRIS_PROFILER_SCOPE(FReplicationStateDescriptorBuilder_SetUpRuntimeReplicationData);
		// This is a bit unfortunate, but for now we need to do this since we need the replication indices and the ClassReps
		InObjectClass->SetUpRuntimeReplicationData();
	}

	// We need the lifetime properties array to get conditionals
	TArray<FLifetimeProperty> LifeTimeProperties;
	if (bGetLifeTimeProperties)
	{
		IRIS_PROFILER_SCOPE(FReplicationStateDescriptorBuilder_GetLifeTimeProperties);

		TArray<FLifetimeProperty> TempLifetimeProperties;
		TempLifetimeProperties.Reserve(InObjectClass->ClassReps.Num());
		UObject* Object = InObjectClass->GetDefaultObject();
		Object->GetLifetimeReplicatedProps(TempLifetimeProperties);

		// Lifetime properties aren't sorted and searching for a property with a specific RepIndex in the array is linear.
		// Let's put them where we want them.
		static_assert(TIsZeroConstructType<FLifetimeProperty>::Value, "Need to use SetNum() instead of SetNumZeroed()");
		LifeTimeProperties.SetNumZeroed(InObjectClass->ClassReps.Num());
		for (const FLifetimeProperty& LifetimeProperty : TempLifetimeProperties)
		{
			LifeTimeProperties[LifetimeProperty.RepIndex] = LifetimeProperty;
		}
	}

	// For a UClass we split up our state into separate replication states based on common traits (Conditionals)
	constexpr uint32 BuilderTypeCount = UE_ARRAY_COUNT(InternalPropertyReplicationStateTypes);
	FPropertyReplicationStateDescriptorBuilder Builders[BuilderTypeCount];

	// We add separate descriptors for all fastarrays as they need special treatment
	TArray<FPropertyReplicationStateDescriptorBuilder::FMemberProperty, TInlineAllocator<8>> FastArrayProperties;

	// Add properties
	for (const FRepRecord& RepRecord : InObjectClass->ClassReps)
	{
		FPropertyReplicationStateDescriptorBuilder::FMemberProperty MemberProperty;
		const FProperty* Property = RepRecord.Property;
	
		// If we are building a class descriptor for a single property we only include the property matching the index
		if (bSingleProperty && ((int32)Property->RepIndex != Parameters.SinglePropertyIndex))
		{
			continue;
		}

		// Skip super class properties depending on parameters. C array elements will be treated individually.
		if ((bIncludeSuper || Property->GetOwnerVariant() == InObjectClass))
		{
			if (FPropertyReplicationStateDescriptorBuilder::IsSupportedProperty(MemberProperty, Property, &LifeTimeProperties, InObjectClass))
			{
				// Ignore disabled properties
				if (MemberProperty.ReplicationCondition == COND_Never)
				{
					continue;
				}

				if (EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::IsFastArray))
				{
					if (MemberProperty.CreateAndRegisterReplicationFragmentFunction)
					{
						FastArrayProperties.Add(MemberProperty);
						continue;
					}
				}

				// A property can end up in multiple states, such as when a condition is InitialOrOwner.
				// $IRIS TODO Need NoInit on properties to avoid double send on init.
				if (EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::InitOnly))
				{
					Builders[InitPropertyReplicationStateBuilderIndex].AddMemberProperty(MemberProperty);
				}

				constexpr EMemberPropertyTraits ConditionTraits = EMemberPropertyTraits::HasLifetimeConditionals;
				if (EnumHasAnyFlags(MemberProperty.Traits, ConditionTraits))
				{
					if (EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::HasLifetimeConditionals))
					{
						Builders[LifetimeConditionalsReplicationStateBuilderIndex].AddMemberProperty(MemberProperty);
					}
				}

				if (!EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::InitOnly | ConditionTraits))
				{
					Builders[RegularPropertyReplicationStateBuilderIndex].AddMemberProperty(MemberProperty);
				}
			}
			else
			{
				UE_LOG_DESCRIPTORBUILDER_WARNING(TEXT("FPropertyReplicationStateDescriptorBuilder: Skipping unsupported property %s.%s of type %s."), ToCStr(InObjectClass->GetName()), ToCStr(Property->GetName()), ToCStr(Property->GetCPPType()));
			}
		}
	}

	// Add special Actor properties
	{
		constexpr bool bExactMatch = true;
		UClass* ActorClass = CastChecked<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/Engine.Actor"), bExactMatch));
		if (InObjectClass->IsChildOf(ActorClass))
		{
			if (const FProperty* NetCullDistanceSquaredProperty = InObjectClass->FindPropertyByName(ReplicationStateDescriptorBuilder_NAME_NetCullDistanceSquared))
			{
				FPropertyReplicationStateDescriptorBuilder::FMemberProperty NetCullDistanceSqrMemberProperty;
				NetCullDistanceSqrMemberProperty.ChangeMaskBits = 1U;
				NetCullDistanceSqrMemberProperty.Property = NetCullDistanceSquaredProperty;
				NetCullDistanceSqrMemberProperty.ExternalSizeAndAlignment = {};
				NetCullDistanceSqrMemberProperty.PropertyRepNotifyFunction = nullptr;
				NetCullDistanceSqrMemberProperty.ReplicationCondition = COND_None;
				NetCullDistanceSqrMemberProperty.SerializerInfo = FPropertyNetSerializerInfoRegistry::GetNopNetSerializerInfo();
				NetCullDistanceSqrMemberProperty.Traits = EMemberPropertyTraits::None;

				FPropertyReplicationStateDescriptorBuilder::GetIrisPropertyTraits(NetCullDistanceSqrMemberProperty, NetCullDistanceSquaredProperty, nullptr /* lifetime conditions */, InObjectClass);
				FPropertyReplicationStateDescriptorBuilder::GetSerializerTraits(NetCullDistanceSqrMemberProperty, NetCullDistanceSquaredProperty, NetCullDistanceSqrMemberProperty.SerializerInfo);

				// These needs to be set/fixed after the Traits calls.
				NetCullDistanceSqrMemberProperty.Traits |= EMemberPropertyTraits::InitOnly;
				NetCullDistanceSqrMemberProperty.ReplicationCondition = COND_InitialOnly;

				Builders[InitPropertyReplicationStateBuilderIndex].AddMemberProperty(NetCullDistanceSqrMemberProperty);
			}
		}
	}

	// Add RPCs
	if (!bSingleProperty)
	{
		FPropertyReplicationStateDescriptorBuilder& Builder = Builders[FunctionsPropertyReplicationStateBuilderIndex];
		FPropertyReplicationStateDescriptorBuilder::FMemberFunction MemberFunction;
		const SIZE_T MaxClassHierarchyDepth = 64;
		const UClass* Classes[MaxClassHierarchyDepth];
		SIZE_T ClassIndex = MaxClassHierarchyDepth;
		
		// NetFields only contain the class specific networked fields, excluding super class fields.
		for (const UClass* CurrentClass = InObjectClass; CurrentClass != nullptr; CurrentClass = CurrentClass->GetSuperClass())
		{
			if (!ensureAlwaysMsgf(ClassIndex > 0, TEXT("Class hieararchy depth exceeds %u"), MaxClassHierarchyDepth))
			{
				break;
			}
			Classes[--ClassIndex] = CurrentClass;
		}

		for (const UClass* CurrentClass : MakeArrayView(Classes + ClassIndex, MaxClassHierarchyDepth - ClassIndex))
		{
			for (const UField* NetField : MakeArrayView(CurrentClass->NetFields))
			{
				if (const UFunction* Function = Cast<UFunction>(NetField))
				{
					MemberFunction.Function = Function;
					Builder.AddMemberFunction(MemberFunction);
				}
			}
		}
	}

	// For-loop with CurrentClass initialized to InObjectClass causes static analysis warnings.
	CA_ASSUME(InObjectClass != nullptr);

	// Build state descriptors
	FLazyGetPathNameHelper LazyPathName;

	const uint8* DefaultStateSourceData = reinterpret_cast<const uint8*>(Parameters.DefaultStateSource ? Parameters.DefaultStateSource : InObjectClass->GetDefaultObject());
	for (uint32 BuilderTypeIndex = 0; BuilderTypeIndex < BuilderTypeCount; ++BuilderTypeIndex)
	{
		if (Builders[BuilderTypeIndex].HasDataToBuild())
		{
			FPropertyReplicationStateDescriptorBuilder::FBuildParameters BuildParameters = {};
			BuildParameters.PathName = LazyPathName.GetPathName(InObjectClass);
			BuildParameters.DefaultStateSourceData = DefaultStateSourceData;
			BuildParameters.DescriptorType = FPropertyReplicationStateDescriptorBuilder::EDescriptorType::Class;
			BuildParameters.bIsInitState = (BuilderTypeIndex == InitPropertyReplicationStateBuilderIndex);
			// We set this to false for the time being
			BuildParameters.bAllMembersAreReplicated = false;
			TRefCountPtr<const FReplicationStateDescriptor> Descriptor = Builders[BuilderTypeIndex].Build(InObjectClass->GetName() + InternalPropertyReplicationStateTypes[BuilderTypeIndex].NamePostFix, Parameters.DescriptorRegistry, BuildParameters);
			CreatedDescriptors.Add(Descriptor);
		}
	}

	// If we have properties that are FastArrays, build separate descriptors for each property
	for (uint32 FastArrayPropertyIt = 0, FastArrayPropertyEndIt = FastArrayProperties.Num(); FastArrayPropertyIt != FastArrayPropertyEndIt; ++FastArrayPropertyIt)
	{
		const FPropertyReplicationStateDescriptorBuilder::FMemberProperty& MemberProperty = FastArrayProperties[FastArrayPropertyIt];

		// Verify some assumptions, for now we do not support putting fast arrays in multiple states
		check(!EnumHasAllFlags(MemberProperty.Traits, EMemberPropertyTraits::InitOnly | EMemberPropertyTraits::HasLifetimeConditionals));

		FPropertyReplicationStateDescriptorBuilder Builder;		
		FPropertyReplicationStateDescriptorBuilder::FBuildParameters BuildParameters = {};
		BuildParameters.PathName = LazyPathName.GetPathName(InObjectClass);
		BuildParameters.DefaultStateSourceData = DefaultStateSourceData;
		BuildParameters.DescriptorType = FPropertyReplicationStateDescriptorBuilder::EDescriptorType::Class;
		BuildParameters.bIsInitState = EnumHasAnyFlags(MemberProperty.Traits, EMemberPropertyTraits::InitOnly);
		
		// Treat as FastArray
		BuildParameters.bIsFastArray = true;

		// We set this to false for the time being
		BuildParameters.bAllMembersAreReplicated = false;

		// Add the property
		Builder.AddMemberProperty(MemberProperty);
		
		FString PropertyName;
		MemberProperty.Property->GetName(PropertyName);

		TRefCountPtr<const FReplicationStateDescriptor> Descriptor = Builder.Build(InObjectClass->GetName() + ToCStr(PropertyName), Parameters.DescriptorRegistry, BuildParameters);
		CreatedDescriptors.Add(Descriptor);
	}

	// If we have a registry, register the created descriptors
	if (bRegisterCreatedDescriptors && CreatedDescriptors.Num())
	{
		UE_LOG_DESCRIPTORBUILDER(Verbose, TEXT("Registering descriptors for  Class %s : ArcheTypeOrKey: %s"), *InObjectClass->GetName(), *ObjectClassOrArchetypeUsedAsKey->GetPathName());
		Parameters.DescriptorRegistry->Register(ObjectClassOrArchetypeUsedAsKey, InObjectClass, CreatedDescriptors);
	}
	
	return CreatedDescriptors.Num();
}

}
