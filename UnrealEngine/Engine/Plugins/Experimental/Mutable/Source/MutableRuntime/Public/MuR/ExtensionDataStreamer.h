// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/Types.h"
#include "Templates/SharedPointerFwd.h"

namespace mu
{

//! Allows runtime code to track the progress of an Extension Data load
class FExtensionDataLoadHandle
{
public:
	ExtensionDataPtrConst Data;

	enum class ELoadState : uint8
	{
		Pending,
		FailedToLoad,
		Loaded
	};
	ELoadState LoadState;
};

//! \brief Interface to allow the runtime to load ExtensionData for use
//!
//! An object implementing this interface must be provided to the System for any Models that use
//! ExtensionData.
//! 
//! All functions must be safe to call from the Mutable thread.
//! 
//! \ingroup runtime
class MUTABLERUNTIME_API ExtensionDataStreamer
{
public:

	//! Ensure virtual destruction.
	virtual ~ExtensionDataStreamer() = default;

	//! Make a copy of the actual data referenced by the ExtensionData, and return a new
	//! runtime-generated ExtensionData that references it.
	//! 
	//! Source can either be a constant or another runtime-generated ExtensionData.
	//! 
	//! The new ExtensionData should be completely independent of the source ExtensionData, such
	//! that if the source instance is deleted, the new instance should still be valid.
	virtual ExtensionDataPtr CloneExtensionData(const ExtensionDataPtrConst& Source) = 0;
	
	//! Start asynchronously loading the streamed constant data referenced by the given
	//! ExtensionData.
	//! 
	//! \param Data The ExtensionData to load. Its Origin must be ConstantStreamed.
	//! 
	//! \param OutUnloadedConstants An array of ConstantStreamed ExtensionData that was unloaded
	//! to make space for the requested data. Guaranteed to only contain valid pointers to
	//! ConstantStreamed data from this program.
	//! 
	//! \return A handle that allows the caller to poll the load state. When the state is no longer
	//! Pending, the load is either complete or has failed.
	virtual TSharedRef<const FExtensionDataLoadHandle> StartLoad(
		const ExtensionDataPtrConst& Data,
		TArray<ExtensionDataPtrConst>& OutUnloadedConstants) = 0;
};

}
