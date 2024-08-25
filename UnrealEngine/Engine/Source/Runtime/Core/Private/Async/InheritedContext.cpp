// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/InheritedContext.h"

namespace UE
{
	// this method must be defined in a .cpp file to avoid exporting dependency on trace module
	[[nodiscard]] FInheritedContextScope FInheritedContextBase::RestoreInheritedContext()
	{
		return FInheritedContextScope{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
				InheritedLLMTags
	#if (UE_MEMORY_TAGS_TRACE_ENABLED||UE_TRACE_METADATA_ENABLED) && UE_TRACE_ENABLED
				,
	#endif
#endif
#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED
				InheritedMemTag
	#if UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED
				,
	#endif
#endif
#if UE_TRACE_METADATA_ENABLED && UE_TRACE_ENABLED
				InheritedMetadataId
#endif
		};
	}
}