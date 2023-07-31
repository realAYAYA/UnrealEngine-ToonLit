// Copyright Epic Games, Inc. All Rights Reserved.
#include "ProfilingDebugging/TagTrace.h"

#include "Experimental/Containers/GrowOnlyLockFreeHash.h"
#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "ProfilingDebugging/MemoryTrace.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/LowLevelMemTrackerPrivate.h"
#include "Trace/Trace.inl"
#include "UObject/NameTypes.h"

#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED

////////////////////////////////////////////////////////////////////////////////

UE_TRACE_CHANNEL_EXTERN(MemAllocChannel);

UE_TRACE_EVENT_BEGIN(Memory, TagSpec, Important|NoSync)
	UE_TRACE_EVENT_FIELD(int32, Tag)
	UE_TRACE_EVENT_FIELD(int32, Parent)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Display)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Memory, MemoryScope, NoSync)
	UE_TRACE_EVENT_FIELD(int32, Tag)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Memory, MemoryScopePtr, NoSync)
	UE_TRACE_EVENT_FIELD(uint64, Ptr)
UE_TRACE_EVENT_END()



////////////////////////////////////////////////////////////////////////////////
// Per thread active tag, i.e. the top level FMemScope
thread_local int32 GActiveTag;

////////////////////////////////////////////////////////////////////////////////
FMemScope::FMemScope(int32 InTag, bool bShouldActivate /*= true*/)
{
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MemAllocChannel) & bShouldActivate)
	{
		ActivateScope(InTag);
	}
}

////////////////////////////////////////////////////////////////////////////////
FMemScope::FMemScope(ELLMTag InTag, bool bShouldActivate /*= true*/)
{
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MemAllocChannel) & bShouldActivate)
	{
		ActivateScope(static_cast<int32>(InTag));
	}
}

////////////////////////////////////////////////////////////////////////////////
FMemScope::FMemScope(const FName& InName, bool bShouldActivate /*= true*/)
{
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MemAllocChannel) & bShouldActivate)
	{
		ActivateScope(MemoryTrace_AnnounceFNameTag(InName));
	}
}

////////////////////////////////////////////////////////////////////////////////
FMemScope::FMemScope(const UE::LLMPrivate::FTagData* TagData, bool bShouldActivate /*= true*/)
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MemAllocChannel) & bShouldActivate && TagData)
	{
		ActivateScope(MemoryTrace_AnnounceFNameTag(TagData->GetName()));
	}
#endif // ENABLE_LOW_LEVEL_MEM_TRACKER
}

////////////////////////////////////////////////////////////////////////////////
void FMemScope::ActivateScope(int32 InTag)
{
	if (auto LogScope = FMemoryMemoryScopeFields::LogScopeType::ScopedEnter<FMemoryMemoryScopeFields>())
	{
		if (const auto& __restrict MemoryScope = *(FMemoryMemoryScopeFields*)(&LogScope))
		{
			Inner.SetActive();
			LogScope += LogScope << MemoryScope.Tag(InTag);
			PrevTag = GActiveTag;
			GActiveTag = InTag;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
FMemScope::~FMemScope()
{
	if (Inner.bActive)
	{
		GActiveTag = PrevTag;
	}
}

////////////////////////////////////////////////////////////////////////////////
FMemScopePtr::FMemScopePtr(uint64 InPtr)
{
	if (InPtr != 0 && TRACE_PRIVATE_CHANNELEXPR_IS_ENABLED(MemAllocChannel))
	{
		if (auto LogScope = FMemoryMemoryScopePtrFields::LogScopeType::ScopedEnter<FMemoryMemoryScopePtrFields>())
		{
			if (const auto& __restrict MemoryScope = *(FMemoryMemoryScopePtrFields*)(&LogScope))
			{
				Inner.SetActive(), LogScope += LogScope << MemoryScope.Ptr(InPtr);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
FMemScopePtr::~FMemScopePtr()
{
}

/////////////////////////////////////////////////////////////////////////////////

/**
 * Utility class that manages tracing the specification of unique LLM tags 
 * and custom name based tags.
 */
class FTagTrace
{
public:
					FTagTrace(FMalloc* InMalloc);
	void			AnnounceGenericTags() const;
	void 			AnnounceTagDeclarations();
					void AnnounceSpecialTags() const;
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	static void 	OnAnnounceTagDeclaration(FLLMTagDeclaration& TagDeclaration);
#endif
	int32			AnnounceCustomTag(int32 Tag, int32 ParentTag, const ANSICHAR* Display) const;
	int32 			AnnounceFNameTag(const FName& TagName);

private:

	static constexpr int32 FNAME_INDEX_OFFSET = 512;
	
	struct FTagNameSetEntry
	{
		std::atomic_int32_t Data;

		int32 GetKey() const { return Data.load(std::memory_order_relaxed); }
		bool GetValue() const { return true; }
		bool IsEmpty() const { return Data.load(std::memory_order_relaxed) == 0; }			// NAME_None is treated as empty
		void SetKeyValue(int32 Key, bool Value) { Data.store(Key, std::memory_order_relaxed); }
		static uint32 KeyHash(int32 Key) { return static_cast<uint32>(Key); }
		static void ClearEntries(FTagNameSetEntry* Entries, int32 EntryCount)
		{
			memset(Entries, 0, EntryCount * sizeof(FTagNameSetEntry));
		}
	};
	typedef TGrowOnlyLockFreeHash<FTagNameSetEntry, int32, bool> FTagNameSet;

	FTagNameSet				AnnouncedNames;
	static FMalloc* 		Malloc;
};

FMalloc* FTagTrace::Malloc = nullptr;
static FTagTrace* GTagTrace = nullptr;

////////////////////////////////////////////////////////////////////////////////
FTagTrace::FTagTrace(FMalloc* InMalloc)
	: AnnouncedNames(InMalloc)
{
	Malloc = InMalloc;
	AnnouncedNames.Reserve(1024);
	AnnounceGenericTags();
	AnnounceTagDeclarations();
	AnnounceSpecialTags();
}

////////////////////////////////////////////////////////////////////////////////
void FTagTrace::AnnounceGenericTags() const
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	#define TRACE_TAG_SPEC(Enum,Str,Stat,Group,ParentTag)\
	{\
		const uint32 DisplayLen = FCStringAnsi::Strlen(Str);\
		UE_TRACE_LOG(Memory, TagSpec, MemAllocChannel, DisplayLen * sizeof(ANSICHAR))\
			<< TagSpec.Tag((int32) ELLMTag::Enum)\
			<< TagSpec.Parent((int32) ParentTag)\
			<< TagSpec.Display(Str, DisplayLen);\
	}
	LLM_ENUM_GENERIC_TAGS(TRACE_TAG_SPEC);
	#undef TRACE_TAG_SPEC
#endif
}

////////////////////////////////////////////////////////////////////////////////
void FTagTrace::AnnounceTagDeclarations()
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	FLLMTagDeclaration* List = FLLMTagDeclaration::GetList();
	while (List)
	{
		OnAnnounceTagDeclaration(*List);
		List = List->Next;
	}
	FLLMTagDeclaration::AddCreationCallback(FTagTrace::OnAnnounceTagDeclaration);
#endif
}

////////////////////////////////////////////////////////////////////////////////
void FTagTrace::AnnounceSpecialTags() const
{
	auto EmitTag = [](const TCHAR* DisplayString, int32 Tag, int32 ParentTag)
	{
		const int32 DisplayLen = FCString::Strlen(DisplayString);
		UE_TRACE_LOG(Memory, TagSpec, MemAllocChannel, DisplayLen * sizeof(ANSICHAR))
			 << TagSpec.Tag(Tag)
			 << TagSpec.Parent(ParentTag)
			 << TagSpec.Display(DisplayString, DisplayLen);
	};

	EmitTag(TEXT("Trace"), TRACE_TAG, -1);

}

////////////////////////////////////////////////////////////////////////////////
#if ENABLE_LOW_LEVEL_MEM_TRACKER
void FTagTrace::OnAnnounceTagDeclaration(FLLMTagDeclaration& TagDeclaration)
{
	TagDeclaration.ConstructUniqueName();
	GTagTrace->AnnounceFNameTag(TagDeclaration.GetUniqueName());
}
#endif

////////////////////////////////////////////////////////////////////////////////
int32 FTagTrace::AnnounceFNameTag(const FName& Name)
{
	const int32 NameIndex = Name.GetDisplayIndex().ToUnstableInt() + FNAME_INDEX_OFFSET;

	// Find or add the item
	bool bAlreadyInTable;
	AnnouncedNames.FindOrAdd(NameIndex, true, &bAlreadyInTable);
	if (bAlreadyInTable)
	{
		return NameIndex;
	}

	// First time encountering this name, announce it
	ANSICHAR NameString[NAME_SIZE];
	Name.GetPlainANSIString(NameString);

	int32 ParentTag = -1;
	FAnsiStringView NameView(NameString);
	int32 LeafStart;
	if (NameView.FindLastChar('/', LeafStart))
	{
		FName ParentName(NameView.Left(LeafStart));
		ParentTag = AnnounceFNameTag(ParentName);
	}

	return AnnounceCustomTag(NameIndex, ParentTag, NameString);
}

////////////////////////////////////////////////////////////////////////////////
int32 FTagTrace::AnnounceCustomTag(int32 Tag, int32 ParentTag, const ANSICHAR* Display) const
{		
	const uint32 DisplayLen = FCStringAnsi::Strlen(Display);
	UE_TRACE_LOG(Memory, TagSpec, MemAllocChannel, DisplayLen * sizeof(ANSICHAR))
		<< TagSpec.Tag(Tag)
		<< TagSpec.Parent(ParentTag)
		<< TagSpec.Display(Display, DisplayLen);
	return Tag;
}

#endif //UE_MEMORY_TAGS_TRACE_ENABLED


////////////////////////////////////////////////////////////////////////////////
void MemoryTrace_InitTags(FMalloc* InMalloc)
{
#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED
	GTagTrace = (FTagTrace*)InMalloc->Malloc(sizeof(FTagTrace), alignof(FTagTrace));
	new (GTagTrace) FTagTrace(InMalloc);
#endif
}

////////////////////////////////////////////////////////////////////////////////
int32 MemoryTrace_AnnounceCustomTag(int32 Tag, int32 ParentTag, const TCHAR* Display)
{
#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED
	//todo: How do we check if tag trace is active?
	if (GTagTrace)
	{
		return GTagTrace->AnnounceCustomTag(Tag, ParentTag, TCHAR_TO_ANSI(Display));
	}
#endif
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int32 MemoryTrace_AnnounceFNameTag(const FName& TagName)
{
#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED
	if (GTagTrace)
	{
		return GTagTrace->AnnounceFNameTag(TagName);
	}
#endif
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int32 MemoryTrace_GetActiveTag()
{
#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED
	return GActiveTag;
#else
	return -1;
#endif
}

