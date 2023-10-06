// Copyright Epic Games, Inc. All Rights Reserved.

#include "StructUtilsTypes.h"
#include "Serialization/ArchiveCrc32.h"
#include "StructView.h"
#include "InstancedStruct.h"
#include "SharedStruct.h"

namespace UE::StructUtils
{
	STRUCTUTILS_API uint32 GetStructCrc32(const UScriptStruct& ScriptStruct, const uint8* StructMemory, const uint32 CRC /*= 0*/)
	{
		FArchiveCrc32 Ar(HashCombine(CRC, PointerHash(&ScriptStruct)));
		if (StructMemory)
		{
			UScriptStruct& NonConstScriptStruct = const_cast<UScriptStruct&>(ScriptStruct);
			NonConstScriptStruct.SerializeItem(Ar, const_cast<uint8*>(StructMemory), nullptr);
		}
		return Ar.GetCrc();
	}

	template<typename T>
	uint32 GetStructCrc32Helper(const T& Struct, const uint32 CRC /*= 0*/)
	{
		if (const UScriptStruct* ScriptStruct = Struct.GetScriptStruct())
		{
			return GetStructCrc32(*ScriptStruct, Struct.GetMemory(), CRC);
		}
		return 0;
	}

	STRUCTUTILS_API uint32 GetStructCrc32(const FStructView& StructView, const uint32 CRC /*= 0*/)
	{
		return GetStructCrc32Helper(StructView, CRC);
	}

	STRUCTUTILS_API uint32 GetStructCrc32(const FConstStructView& StructView, const uint32 CRC /*= 0*/)
	{
		return GetStructCrc32Helper(StructView, CRC);
	}

	STRUCTUTILS_API uint32 GetStructCrc32(const FSharedStruct& SharedView, const uint32 CRC /*= 0*/)
	{
		return GetStructCrc32Helper(SharedView, CRC);
	}

	STRUCTUTILS_API uint32 GetStructCrc32(const FConstSharedStruct& SharedView, const uint32 CRC /*= 0*/)
	{
		return GetStructCrc32Helper(SharedView, CRC);
	}

	namespace Private
	{
#if WITH_ENGINE && WITH_EDITOR
		const UUserDefinedStruct* GStructureToReinstance = nullptr;
		UObject* GCurrentReinstanceOuterObject = nullptr;

		FStructureToReinstanceScope::FStructureToReinstanceScope(const UUserDefinedStruct* StructureToReinstance)
		{
			OldStructureToReinstance = GStructureToReinstance;
			GStructureToReinstance = StructureToReinstance;
		}
	
		FStructureToReinstanceScope::~FStructureToReinstanceScope()
		{
			GStructureToReinstance = OldStructureToReinstance;
		}

		FCurrentReinstanceOuterObjectScope::FCurrentReinstanceOuterObjectScope(UObject* CurrentReinstanceOuterObject)
		{
			OldCurrentReinstanceOuterObject = GCurrentReinstanceOuterObject;
			GCurrentReinstanceOuterObject = CurrentReinstanceOuterObject;
		}

		FCurrentReinstanceOuterObjectScope::~FCurrentReinstanceOuterObjectScope()
		{
			GCurrentReinstanceOuterObject = OldCurrentReinstanceOuterObject;
		}

		const UUserDefinedStruct* GetStructureToReinstance()
		{
			return GStructureToReinstance;
		}
	
		UObject* GetCurrentReinstanceOuterObject()
		{
			return GCurrentReinstanceOuterObject;
		}

#endif
	}
}
