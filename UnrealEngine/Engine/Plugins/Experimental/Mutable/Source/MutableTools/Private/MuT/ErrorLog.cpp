// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ErrorLog.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Skeleton.h"
#include "MuR/System.h"
#include "MuR/MutableRuntimeModule.h"
#include "MuT/ErrorLogPrivate.h"
#include "Templates/Tuple.h"
#include "Trace/Detail/Channel.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
	ErrorLog::ErrorLog()
	{
		m_pD = new Private();
	}


	//---------------------------------------------------------------------------------------------
	ErrorLog::~ErrorLog()
	{
        check( m_pD );
		delete m_pD;
		m_pD = 0;
	}


	//---------------------------------------------------------------------------------------------
	ErrorLog::Private* ErrorLog::GetPrivate() const
	{
		return m_pD;
	}


	//---------------------------------------------------------------------------------------------
    int ErrorLog::GetMessageCount( ErrorLogMessageType ) const
	{
        // \todo: by type argument
		return (int)m_pD->m_messages.Num();
	}


	//---------------------------------------------------------------------------------------------
	const FString& ErrorLog::GetMessageText( int index ) const
	{
		const char* strResult = "";

		if ( index >=0 && index<(int)m_pD->m_messages.Num() )
		{
			return m_pD->m_messages[index].m_text;
		}

		static FString Empty;
		return Empty;
	}


	//---------------------------------------------------------------------------------------------
	const void* ErrorLog::GetMessageContext( int index ) const
	{
		const void* result = 0;

		if ( index >=0 && index<(int)m_pD->m_messages.Num() )
		{
			result = m_pD->m_messages[index].m_context;
		}

		return result;
	}


	//---------------------------------------------------------------------------------------------
	ErrorLogMessageType ErrorLog::GetMessageType( int index ) const
	{
		ErrorLogMessageType result = ELMT_NONE;

		if ( index >=0 && index<(int)m_pD->m_messages.Num() )
		{
			result = m_pD->m_messages[index].m_type;
		}

		return result;
	}


	//---------------------------------------------------------------------------------------------
	ErrorLogMessageSpamBin ErrorLog::GetMessageSpamBin(int index) const
	{
		ErrorLogMessageSpamBin result = ELMSB_ALL;

		if (index >= 0 && index < (int)m_pD->m_messages.Num())
		{
			result = m_pD->m_messages[index].m_spam;
		}

		return result;
	}

	//---------------------------------------------------------------------------------------------
    ErrorLogMessageAttachedDataView ErrorLog::GetMessageAttachedData( int index ) const
	{
        ErrorLogMessageAttachedDataView result;

		if ( index >=0 && index<(int)m_pD->m_messages.Num() )
		{
			const Private::FMessage& message = m_pD->m_messages[index];
            
            if ( message.m_data ) 
            {
                result.m_unassignedUVs = message.m_data->m_unassignedUVs.GetData();
			    result.m_unassignedUVsSize = message.m_data->m_unassignedUVs.Num();
            }
		}

		return result;
	}

	//---------------------------------------------------------------------------------------------
	void ErrorLog::Private::Add(const FString& InMessage,
								ErrorLogMessageType InType,
								const void* InContext,
								ErrorLogMessageSpamBin InSpamBin)
	{
		m_messages.Add(FMessage());
		m_messages.Last().m_type = InType;
		m_messages.Last().m_spam = InSpamBin;
		m_messages.Last().m_text = InMessage;
		m_messages.Last().m_context = InContext;
	}

	//---------------------------------------------------------------------------------------------
	void ErrorLog::Private::Add(const FString& InMessage,
                                const ErrorLogMessageAttachedDataView& InDataView,
                                ErrorLogMessageType InType, const void* InContext,
								ErrorLogMessageSpamBin InSpamBin)
	{
		m_messages.Add(FMessage() );
		m_messages.Last().m_type = InType;
		m_messages.Last().m_spam = InSpamBin;
		m_messages.Last().m_text = InMessage;
		m_messages.Last().m_context = InContext;
		m_messages.Last().m_data = MakeShared<FErrorData>();

        if ( InDataView.m_unassignedUVs && InDataView.m_unassignedUVsSize > 0 )
        {
			// \TODO: Review
			m_messages.Last().m_data->m_unassignedUVs.Append(InDataView.m_unassignedUVs, InDataView.m_unassignedUVsSize);
        }
	}
	
	
	//---------------------------------------------------------------------------------------------
	void ErrorLog::Log() const
	{
		UE_LOG(LogMutableCore, Log, TEXT(" Error Log :\n"));

		for ( const Private::FMessage& msg : m_pD->m_messages )
		{
			switch ( msg.m_type )
			{
			case ELMT_ERROR: 	UE_LOG(LogMutableCore, Log, TEXT("  ERR  %s\n"), *msg.m_text); break;
			case ELMT_WARNING: 	UE_LOG(LogMutableCore, Log, TEXT("  WRN  %s\n"), *msg.m_text); break;
			case ELMT_INFO: 	UE_LOG(LogMutableCore, Log, TEXT("  INF  %s\n"), *msg.m_text); break;
			default: 			UE_LOG(LogMutableCore, Log, TEXT("  NON  %s\n"), *msg.m_text); break;
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void ErrorLog::Merge( const ErrorLog* pOther )
	{
		m_pD->m_messages.Append(pOther->GetPrivate()->m_messages);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
    
    // clang-format off

    const TCHAR* s_opNames[ int32(OP_TYPE::COUNT) ] =
	{
		TEXT("NONE             "),

		TEXT("BO_CONSTANT      "),
        TEXT("NU_CONSTANT      "),
        TEXT("SC_CONSTANT      "),
        TEXT("CO_CONSTANT      "),
		TEXT("IM_CONSTANT      "),
		TEXT("ME_CONSTANT      "),
		TEXT("LA_CONSTANT      "),
		TEXT("PR_CONSTANT      "),
		TEXT("ST_CONSTANT      "),
		TEXT("ED_CONSTANT      "),

		TEXT("BO_PARAMETER     "),
		TEXT("NU_PARAMETER     "),
		TEXT("SC_PARAMETER     "),
		TEXT("CO_PARAMETER     "),
		TEXT("PR_PARAMETER     "),
		TEXT("IM_PARAMETER     "),
		TEXT("ST_PARAMETER     "),
		
		TEXT("IM_REFERENCE     "),

		TEXT("NU_CONDITIONAL   "),
		TEXT("SC_CONDITIONAL   "),
		TEXT("CO_CONDITIONAL   "),
		TEXT("IM_CONDITIONAL   "),
		TEXT("ME_CONDITIONAL   "),
		TEXT("LA_CONDITIONAL   "),
		TEXT("IN_CONDITIONAL   "),
		TEXT("ED_CONDITIONAL   "),
		
		TEXT("NU_SWITCH        "),
		TEXT("SC_SWITCH        "),
		TEXT("CO_SWITCH        "),
		TEXT("IM_SWITCH        "),
		TEXT("ME_SWITCH        "),
		TEXT("LA_SWITCH        "),
		TEXT("IN_SWITCH        "),
		TEXT("ED_SWITCH        "),

		TEXT("BO_LESS          "),
		TEXT("BO_EQUAL_SC_CONST"),
		TEXT("BO_AND           "),
		TEXT("BO_OR            "),
		TEXT("BO_NOT           "),
		
		TEXT("SC_MULTIPLYADD   "),
		TEXT("SC_ARITHMETIC    "),
		TEXT("SC_CURVE         "),
		
		TEXT("CO_SAMPLEIMAGE   "),
		TEXT("CO_SWIZZLE       "),
		TEXT("CO_FROMSCALARS   "),
		TEXT("CO_ARITHMETIC    "),

		TEXT("IM_LAYER         "),
		TEXT("IM_LAYERCOLOUR   "),
		TEXT("IM_PIXELFORMAT   "),
		TEXT("IM_MIPMAP        "),
		TEXT("IM_RESIZE        "),
		TEXT("IM_RESIZELIKE    "),
		TEXT("IM_RESIZEREL     "),
		TEXT("IM_BLANKLAYOUT   "),
		TEXT("IM_COMPOSE       "),
		TEXT("IM_INTERPOLATE   "),
		TEXT("IM_SATURATE      "),
		TEXT("IM_LUMINANCE     "),
		TEXT("IM_SWIZZLE       "),
		TEXT("IM_COLOURMAP     "),
		TEXT("IM_GRADIENT      "),
		TEXT("IM_BINARISE      "),
		TEXT("IM_PLAINCOLOUR   "),
		TEXT("IM_CROP          "),
		TEXT("IM_PATCH         "),
		TEXT("IM_RASTERMESH    "),
		TEXT("IM_MAKEGROWMAP   "),
		TEXT("IM_DISPLACE      "),
		TEXT("IM_MULTILAYER    "),
		TEXT("IM_INVERT        "),
		TEXT("IM_NORMAL_COMPO  "),
		TEXT("IM_TRANSFORM     "),

		TEXT("ME_APPLYLAYOUT   "),
		TEXT("ME_DIFFERENCE    "),
		TEXT("ME_MORPH         "),
		TEXT("ME_MERGE         "),
		TEXT("ME_INTERPOLATE   "),
		TEXT("ME_MASKCLIPMESH  "),
		TEXT("ME_MASKCLIPUVMASK"),
		TEXT("ME_MASKDIFF      "),
        TEXT("ME_REMOVEMASK    "),
        TEXT("ME_FORMAT        "),
        TEXT("ME_EXTRACTLABLOCK"),
		TEXT("ME_TRANSFORM     "),
		TEXT("ME_CLIPMORPHPLANE"),
        TEXT("ME_CLIPWITHMESH  "),
        TEXT("ME_SETSKELETON   "),
        TEXT("ME_PROJECT       "),
        TEXT("ME_APPLYPOSE     "),
		TEXT("ME_GEOMETRYOP	   "),
		TEXT("ME_BINDSHAPE	   "),
		TEXT("ME_APPLYSHAPE	   "),
		TEXT("ME_CLIPDEFORM	   "),
		TEXT("ME_MORPHRESHAPE  "),
		TEXT("ME_OPTIMIZESKIN  "),
		TEXT("ME_ADDTAGS       "),

		TEXT("IN_ADDMESH       "),
		TEXT("IN_ADDIMAGE      "),
		TEXT("IN_ADDVECTOR     "),
		TEXT("IN_ADDSCALAR     "),
		TEXT("IN_ADDSTRING     "),
        TEXT("IN_ADDSURFACE    "),
        TEXT("IN_ADDCOMPONENT  "),
        TEXT("IN_ADDLOD        "),
		TEXT("IN_ADDEXTENSIDATA"),
		
		TEXT("LA_PACK          "),
		TEXT("LA_MERGE         "),
		TEXT("LA_REMOVEBLOCKS  "),
		TEXT("LA_FROMMESH	   "),
	};

	static_assert(sizeof(s_opNames) / sizeof(void*) == int32(OP_TYPE::COUNT));

    // clang-format on

    //---------------------------------------------------------------------------------------------
    const TCHAR* GetOpName( OP_TYPE type )
    {
        //check( type>=0 && type<(int)OP_TYPE::COUNT );
        return s_opNames[(int)type];
    }

}
