// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/Types.h"

namespace mu
{
	//! ExtensionData represents data types that Mutable doesn't support natively.
	//!
	//! Extensions can provide data, and functionality to operate on that data, without Mutable
	//! needing to know what the data refers to.

	class ExtensionData;
	typedef Ptr<ExtensionData> ExtensionDataPtr;
	typedef Ptr<const ExtensionData> ExtensionDataPtrConst;

	class MUTABLERUNTIME_API ExtensionData : public Resource
	{
	public:
		static void Serialise(const ExtensionData* Data, OutputArchive& Archive);
		static ExtensionDataPtr StaticUnserialise(InputArchive& Archive);

		// Resource interface
		int32 GetDataSize() const override;

		//! A stable hash of the contents
		uint32 Hash() const;

		void Serialise(OutputArchive& Archive) const;
		void Unserialise(InputArchive& Archive);

		inline bool operator==(const ExtensionData& Other) const
		{
			const bool bResult =
				Other.Index == Index
				&& Other.Origin == Origin;

			return bResult;
		}

		enum class EOrigin : uint8
		{
			//! An invalid value used to indicate that this ExtensionData hasn't been initialized
			Invalid,

			//! This ExtensionData is a compile-time constant that's always loaded into memory
			ConstantAlwaysLoaded,

			//! This ExtensionData is a compile-time constant that's streamed in from disk when needed
			ConstantStreamed,

			//! This ExtensionData was generated at runtime
			Runtime
		};

		int16 Index = -1;

		EOrigin Origin = EOrigin::Invalid;
	};
}

