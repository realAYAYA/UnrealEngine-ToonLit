// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundThreadLocalDebug.h"

#include "MetasoundNodeInterface.h"
#include "Containers/UnrealString.h"

#define ENABLE_METASOUND_THREAD_LOCAL_DEBUG (DO_CHECK || DO_ENSURE)

namespace Metasound
{
	namespace ThreadLocalDebug
	{
		namespace ThreadLocalDebugPrivate
		{
#if ENABLE_METASOUND_THREAD_LOCAL_DEBUG
			static thread_local FString ActiveNodeClassNameAndVersion;
#endif
		}

		void SetActiveNodeClass(const FNodeClassMetadata& InMetadata)
		{
#if ENABLE_METASOUND_THREAD_LOCAL_DEBUG
			ThreadLocalDebugPrivate::ActiveNodeClassNameAndVersion = FString::Format(TEXT("{0} v{1}.{2}"), {InMetadata.ClassName.GetFullName().ToString(), InMetadata.MajorVersion, InMetadata.MinorVersion});
#endif
		}

		void ResetActiveNodeClass()
		{
#if ENABLE_METASOUND_THREAD_LOCAL_DEBUG
			ThreadLocalDebugPrivate::ActiveNodeClassNameAndVersion = TEXT("");
#endif
		}

		const TCHAR* GetActiveNodeClassNameAndVersion()
		{
#if ENABLE_METASOUND_THREAD_LOCAL_DEBUG
			return *ThreadLocalDebugPrivate::ActiveNodeClassNameAndVersion;	
#else
			return TEXT("");
#endif
		}
	}
}
