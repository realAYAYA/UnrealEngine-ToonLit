// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Serializable.h"
#include "Chaos/ImplicitObject.h"
#include "Serialization/MemoryWriter.h"

class FOutputDevice;

namespace Chaos
{
	class CHAOS_API FTrackedGeometryManager
	{
	public:
		static FTrackedGeometryManager& Get();
		
		void DumpMemoryUsage(FOutputDevice* Ar) const;
		
	private:
		TMap<TSerializablePtr<FImplicitObject>, FString> SharedGeometry;
		FCriticalSection CriticalSection;
		
		friend FImplicitObject;
		
		//These are private because of various threading considerations. ImplicitObject does the cleanup because it needs extra information
		void AddGeometry(TSerializablePtr<FImplicitObject> Geometry, const FString& DebugInfo);
		
		void RemoveGeometry(const FImplicitObject* Geometry);
	};
}
