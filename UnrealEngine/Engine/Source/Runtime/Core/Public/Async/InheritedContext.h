// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/LowLevelMemTracker.h"
#include "ProfilingDebugging/MetadataTrace.h"
#include "ProfilingDebugging/TagTrace.h"

namespace UE
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	/** Structure representing the captured LLM Tags */
	struct FLLMActiveTagsCapture
	{
		/** Fixed array of LLMTagSets captured */
		const UE::LLMPrivate::FTagData* LLMTags[static_cast<uint32>(ELLMTagSet::Max)];

		void CaptureActiveTagData()
		{
			for (int32 TagSetIndex = 0; TagSetIndex < static_cast<int32>(ELLMTagSet::Max); ++TagSetIndex)
			{
				LLMTags[TagSetIndex] = FLowLevelMemTracker::IsEnabled() ?
					FLowLevelMemTracker::Get().GetActiveTagData(ELLMTracker::Default, static_cast<ELLMTagSet>(TagSetIndex))
					: nullptr;
			}
		}
	};

	/** Structure holding the captured LLM scopes */
	struct FLLMActiveTagsScope
	{
		/** Fixed size array which holds a copy of the LLM scopes for later recall */
		/** Order doesn't matter so we can use a FixedAllocator and not require a default constructor */
		TArray<FLLMScope, TFixedAllocator<static_cast<uint32>(ELLMTagSet::Max)>> LLMScopes;

		explicit FLLMActiveTagsScope(const FLLMActiveTagsCapture& InActiveTagsCapture)
		{
			CaptureLLMScopes(InActiveTagsCapture);
		}

		void CaptureLLMScopes(const FLLMActiveTagsCapture& InActiveTagsCapture)
		{
			for (int32 TagSetIndex = 0; TagSetIndex < static_cast<int32>(ELLMTagSet::Max); ++TagSetIndex)
			{
				LLMScopes.Emplace(InActiveTagsCapture.LLMTags[TagSetIndex], false /* bIsStatTag */, static_cast<ELLMTagSet>(TagSetIndex), ELLMTracker::Default);
			}
		}
	};
#endif

	// Restores an inherited contex for the current scope.
	// An instance must be obtained by calling `FInheritedContextBase::RestoreInheritedContext()`
	class FInheritedContextScope
	{
	private:
		UE_NONCOPYABLE(FInheritedContextScope);

		friend class FInheritedContextBase; // allow construction only by `FInheritedContextBase`
		
		FInheritedContextScope(
#if ENABLE_LOW_LEVEL_MEM_TRACKER
			const FLLMActiveTagsCapture& InInheritedLLMTag
	#if UE_TRACE_ENABLED && (UE_MEMORY_TAGS_TRACE_ENABLED || UE_TRACE_METADATA_ENABLED)
			,
	#endif
#endif
#if (UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED)
			int32 InInheritedMemTag
	#if (UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED)
			,
	#endif
#endif
#if (UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED)
			uint32 InInheritedMetadataId
#endif
		)
#if ENABLE_LOW_LEVEL_MEM_TRACKER
			: LLMScopes(InInheritedLLMTag)
	#if UE_TRACE_ENABLED && (UE_MEMORY_TAGS_TRACE_ENABLED || UE_TRACE_METADATA_ENABLED)
			,
	#endif
#endif
#if (UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED)
	#if !ENABLE_LOW_LEVEL_MEM_TRACKER
			:
	#endif
			MemScope(InInheritedMemTag)
	#if (UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED)
			,
	#endif
#endif
#if (UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED)
	#if !ENABLE_LOW_LEVEL_MEM_TRACKER && !(UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED)
			:
	#endif
			MetaScope(InInheritedMetadataId)
#endif
		{
		}

	private:
#if ENABLE_LOW_LEVEL_MEM_TRACKER
		FLLMActiveTagsScope LLMScopes;
#endif

#if (UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED)
		FMemScope MemScope;
#endif

#if (UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED) 
		FMetadataRestoreScope MetaScope;
#endif
	};

	// this class extends the inherited context (see private members for what the inherited context is) to cover async execution. 
	// Is intended to be used as a base class, if the inherited context is compiled out it takes 0 space
	class FInheritedContextBase
	{
	public:
		// must be called in the inherited context, e.g. on launching an async task
		void CaptureInheritedContext()
		{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
			InheritedLLMTags.CaptureActiveTagData();
#endif

#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED
			InheritedMemTag = MemoryTrace_GetActiveTag();
#endif

#if UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED
			InheritedMetadataId = UE_TRACE_METADATA_SAVE_STACK();
#endif
		}

		// must be called where the inherited context should be restored, e.g. at the start of an async task execution
		[[nodiscard]] CORE_API FInheritedContextScope RestoreInheritedContext();

	private:
#if ENABLE_LOW_LEVEL_MEM_TRACKER
		FLLMActiveTagsCapture InheritedLLMTags;
#endif

#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED
		int32 InheritedMemTag;
#endif

#if UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED
		uint32 InheritedMetadataId;
#endif
	};
}
