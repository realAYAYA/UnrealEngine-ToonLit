// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/ErrorLog.h"

#include "MuR/MemoryPrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Operations.h"

#include <memory>

namespace mu
{
	MUTABLETOOLS_API extern const TCHAR* s_opNames[(int)OP_TYPE::COUNT];

	class ErrorLog::Private : public Base
	{
	public:

		Private()
		{
		}

        struct DATA
        {
            TArray< float > m_unassignedUVs;
        };

		struct MSG
		{
			MSG()
			{
                m_type = ELMT_NONE;
				m_context = 0;
			}

			ErrorLogMessageType m_type;
			FString m_text;
            std::shared_ptr< DATA > m_data;
			const void* m_context;
		};

		TArray< MSG > m_messages;


		//-----------------------------------------------------------------------------------------

		//!
		void Add(const FString& Message, ErrorLogMessageType Type, const void* Context);

		//!
		void Add(const char* strMessage, ErrorLogMessageType type, const void* context);

        //!
        void Add( const char* strMessage, const ErrorLogMessageAttachedDataView& data, 
                  ErrorLogMessageType type, const void* context );
	};


	//---------------------------------------------------------------------------------------------
	class Model;
	typedef Ptr<Model> ModelPtr;
	typedef Ptr<const Model> ModelPtrConst;

    extern const TCHAR* GetOpName( OP_TYPE type );
    extern FString GetOpDesc( const PROGRAM& program, OP::ADDRESS at );

}

