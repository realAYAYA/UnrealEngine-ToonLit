// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"


namespace mu
{

    //! \brief Container for text string data.
	//! \ingroup runtime
    class MUTABLERUNTIME_API String : public Resource
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		String(const FString&);

		//! Deep clone this string.
		Ptr<String> Clone() const;

		//! Serialisation
		//static void Serialise( const String* p, OutputArchive& arch );
		//static Ptr<String> StaticUnserialise( InputArchive& arch );

		// Resource interface
		int32 GetDataSize() const override;

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Get the string data.
		const FString& GetValue() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~String() {}

	public:

		//!
		FString Value;

		//!
		//void Serialise(OutputArchive& arch) const
		//{
		//	uint32 ver = 1;
		//	arch << ver;

		//	arch << Value;
		//}

		////!
		//void Unserialise(InputArchive& arch)
		//{
		//	uint32 ver;
		//	arch >> ver;
		//	check(ver <= 1);

		//	if (ver == 0)
		//	{
		//		std::string Temp;
		//		arch >> Temp;
		//		Value = Temp.c_str();
		//	}
		//	else
		//	{
		//		arch >> Value;
		//	}
		//}

		//!
		inline bool operator==(const String& o) const
		{
			return (Value == o.Value);
		}

	};

}

