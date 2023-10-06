// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/ErrorLog.h"

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

        struct FErrorData
        {
            TArray< float > m_unassignedUVs;
        };

		struct FMessage
		{
			ErrorLogMessageType m_type = ELMT_NONE;
			ErrorLogMessageSpamBin m_spam = ELMSB_ALL;
			FString m_text;
            TSharedPtr<FErrorData> m_data;
			const void* m_context = nullptr;
		};

		TArray<FMessage> m_messages;


		//-----------------------------------------------------------------------------------------

		//!
		void Add(const FString& Message, ErrorLogMessageType Type, const void* Context, ErrorLogMessageSpamBin SpamBin = ELMSB_ALL);

        //!
        void Add(const FString& Message, const ErrorLogMessageAttachedDataView& Data, ErrorLogMessageType Type, const void* Context, ErrorLogMessageSpamBin SpamBin = ELMSB_ALL);
	};


    extern const TCHAR* GetOpName( OP_TYPE type );

}

