// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"


namespace mu
{

    //! \brief Container for text string data.
	//! \ingroup runtime
    class MUTABLERUNTIME_API String : public RefCounted
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		String(const char* = nullptr);

		//! Deep clone this string.
		Ptr<String> Clone() const;

		//! Serialisation
		static void Serialise( const String* p, OutputArchive& arch );
		static Ptr<String> StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Get the string data.
		const char* GetValue() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~String() {}

	public:

		//!
		string m_value;

		//!
		void Serialise(OutputArchive& arch) const
		{
			uint32 ver = 0;
			arch << ver;

			arch << m_value;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
			uint32 ver;
			arch >> ver;
			check(ver <= 0);

			arch >> m_value;
		}

		//!
		inline bool operator==(const String& o) const
		{
			return (m_value == o.m_value);
		}

	};

}

