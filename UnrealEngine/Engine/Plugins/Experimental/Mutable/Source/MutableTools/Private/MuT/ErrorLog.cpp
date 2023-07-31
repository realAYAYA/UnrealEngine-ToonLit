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
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Skeleton.h"
#include "MuR/System.h"
#include "MuT/ErrorLogPrivate.h"
#include "Templates/Tuple.h"
#include "Trace/Detail/Channel.h"

#include <memory>
#include <utility>

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
    ErrorLogMessageAttachedDataView ErrorLog::GetMessageAttachedData( int index ) const
	{
        ErrorLogMessageAttachedDataView result;

		if ( index >=0 && index<(int)m_pD->m_messages.Num() )
		{
			const Private::MSG& message = m_pD->m_messages[index];
            
            if ( message.m_data ) 
            {
                result.m_unassignedUVs = message.m_data->m_unassignedUVs.GetData();
			    result.m_unassignedUVsSize = message.m_data->m_unassignedUVs.Num();
            }
		}

		return result;
	}

	//---------------------------------------------------------------------------------------------
	void ErrorLog::Private::Add(const char* strMessage, ErrorLogMessageType type,
		const void* context)
	{
		m_messages.Add(MSG());
		m_messages.Last().m_type = type;
		m_messages.Last().m_text = strMessage;
		m_messages.Last().m_context = context;
	}

	//---------------------------------------------------------------------------------------------
	void ErrorLog::Private::Add(const FString& InMessage, ErrorLogMessageType InType, const void* InContext)
	{
		m_messages.Add(MSG());
		m_messages.Last().m_type = InType;
		m_messages.Last().m_text = InMessage;
		m_messages.Last().m_context = InContext;
	}

	//---------------------------------------------------------------------------------------------
	void ErrorLog::Private::Add( const char* strMessage, 
                                 const ErrorLogMessageAttachedDataView& dataView,
                                 ErrorLogMessageType type, const void* context )
	{
		m_messages.Add( MSG() );
		m_messages.Last().m_type = type;
		m_messages.Last().m_text = strMessage;
		m_messages.Last().m_context = context;
		m_messages.Last().m_data = std::make_shared< DATA >();

        if ( dataView.m_unassignedUVs && dataView.m_unassignedUVsSize > 0 )
        {
			// \TODO: Review
			m_messages.Last().m_data->m_unassignedUVs.Append(dataView.m_unassignedUVs, dataView.m_unassignedUVsSize);
        }
	}
	
	//---------------------------------------------------------------------------------------------
	void ErrorLog::Log() const
	{
		UE_LOG(LogMutableCore, Log, TEXT(" Error Log :\n"));

		for ( const Private::MSG& msg : m_pD->m_messages )
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

    const TCHAR* s_opNames[ size_t(OP_TYPE::COUNT) ] =
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

		TEXT("BO_PARAMETER     "),
		TEXT("NU_PARAMETER     "),
		TEXT("SC_PARAMETER     "),
		TEXT("CO_PARAMETER     "),
		TEXT("PR_PARAMETER     "),
		TEXT("IM_PARAMETER     "),
		TEXT("ST_PARAMETER     "),
		
		TEXT("NU_CONDITIONAL   "),
		TEXT("SC_CONDITIONAL   "),
		TEXT("CO_CONDITIONAL   "),
		TEXT("IM_CONDITIONAL   "),
		TEXT("ME_CONDITIONAL   "),
		TEXT("LA_CONDITIONAL   "),
		TEXT("IN_CONDITIONAL   "),
		
		TEXT("NU_SWITCH        "),
		TEXT("SC_SWITCH        "),
		TEXT("CO_SWITCH        "),
		TEXT("IM_SWITCH        "),
		TEXT("ME_SWITCH        "),
		TEXT("LA_SWITCH        "),
		TEXT("IN_SWITCH        "),

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
		TEXT("CO_IMAGESIZE     "),
		TEXT("CO_LAYOUTBLOCKTR "),
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
		TEXT("IM_DIFFERENCE    "),
		TEXT("IM_INTERPOLATE   "),
		TEXT("IM_INTERPOLATE3  "),
		TEXT("IM_SATURATE      "),
		TEXT("IM_LUMINANCE     "),
		TEXT("IM_SWIZZLE       "),
		TEXT("IM_SELECTCOLOUR  "),
		TEXT("IM_COLOURMAP     "),
		TEXT("IM_GRADIENT      "),
		TEXT("IM_BINARISE      "),
		TEXT("IM_PLAINCOLOUR   "),
		TEXT("IM_GPU           "),
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
		TEXT("ME_MORPH2        "),
		TEXT("ME_MERGE         "),
		TEXT("ME_INTERPOLATE   "),
		TEXT("ME_MASKCLIPMESH  "),
		TEXT("ME_MASKDIFF      "),
        TEXT("ME_SUBTRACT      "),
        TEXT("ME_REMOVEMASK    "),
        TEXT("ME_FORMAT        "),
        TEXT("ME_EXTRACTLABLOCK"),
        TEXT("ME_EXTRACTFACEGRP"),
		TEXT("ME_TRANSFORM     "),
		TEXT("ME_CLIPMORPHPLANE"),
        TEXT("ME_CLIPWITHMESH  "),
        TEXT("ME_SETSKELETON   "),
        TEXT("ME_PROJECT       "),
        TEXT("ME_APPLYPOSE     "),
        TEXT("ME_REMAPINDICES  "),
		TEXT("ME_GEOMETRYOP	   "),
		TEXT("ME_BINDSHAPE	   "),
		TEXT("ME_APPLYSHAPE	   "),
		TEXT("ME_CLIPDEFORM	   "),
		TEXT("ME_MORPHRESHAPE  "),

		TEXT("IN_ADDMESH       "),
		TEXT("IN_ADDIMAGE      "),
		TEXT("IN_ADDVECTOR     "),
		TEXT("IN_ADDSCALAR     "),
		TEXT("IN_ADDSTRING     "),
        TEXT("IN_ADDSURFACE    "),
        TEXT("IN_ADDCOMPONENT  "),
        TEXT("IN_ADDLOD        "),

		TEXT("LA_PACK          "),
		TEXT("LA_MERGE         "),
        TEXT("LA_REMOVEBLOCKS  ")
	};

    // clang-format on

    //---------------------------------------------------------------------------------------------
    const TCHAR* GetOpName( OP_TYPE type )
    {
        //check( type>=0 && type<(int)OP_TYPE::COUNT );
        return s_opNames[(int)type];
    }


	//---------------------------------------------------------------------------------------------
    extern FString GetOpDesc( const PROGRAM& program, OP::ADDRESS at )
	{
		char temp[1024];
		FMemory::Memzero( temp, 1024 );

		size_t done = 0;

        OP_TYPE type = program.GetOpType(at);
        switch (type)
		{
		case OP_TYPE::BO_CONSTANT:
        {
            auto args = program.GetOpArgs<OP::BoolConstantArgs>(at);
			mutable_snprintf( temp, 1024,
                              "bool : %s\t", args.value?"true":"false" );
			break;
        }
        case OP_TYPE::NU_CONSTANT:
        {
            auto args = program.GetOpArgs<OP::IntConstantArgs>(at);
            mutable_snprintf( temp, 1024,
                              "int : %d\t", args.value );
            break;
        }
        case OP_TYPE::SC_CONSTANT:
        {
            auto args = program.GetOpArgs<OP::ScalarConstantArgs>(at);
            mutable_snprintf( temp, 1024,
                              "float : %f\t", args.value );
            break;
        }
        case OP_TYPE::CO_CONSTANT:
        {
            auto args = program.GetOpArgs<OP::ColourConstantArgs>(at);
            mutable_snprintf( temp, 1024,
							  "colour : %f %f %f\t",
                              args.value[0],
                              args.value[1],
                              args.value[2] );
			break;
        }
        case OP_TYPE::IM_CONSTANT:
        case OP_TYPE::LA_CONSTANT:
        {
            auto args = program.GetOpArgs<OP::ResourceConstantArgs>(at);
            mutable_snprintf( temp, 1024,
                              "res  : %d\t", (int)args.value );
            break;
        }
        case OP_TYPE::ME_CONSTANT:
        {
            auto args = program.GetOpArgs<OP::MeshConstantArgs>(at);
            mutable_snprintf( temp, 1024,
                              "res  : %d\t", (int)args.value );
            break;
        }
        case OP_TYPE::BO_PARAMETER:
        case OP_TYPE::NU_PARAMETER:
        case OP_TYPE::SC_PARAMETER:
        case OP_TYPE::CO_PARAMETER:
        case OP_TYPE::PR_PARAMETER:
        case OP_TYPE::IM_PARAMETER:
        {
            auto args = program.GetOpArgs<OP::ParameterArgs>(at);
            mutable_snprintf( temp, 1024,
                              "variable : %2d", (int)args.variable );
			break;
        }
        case OP_TYPE::NU_CONDITIONAL:
        case OP_TYPE::SC_CONDITIONAL:
        case OP_TYPE::CO_CONDITIONAL:
		case OP_TYPE::IM_CONDITIONAL:
		case OP_TYPE::ME_CONDITIONAL:
		case OP_TYPE::LA_CONDITIONAL:
		case OP_TYPE::IN_CONDITIONAL:
        {
            auto args = program.GetOpArgs<OP::ConditionalArgs>(at);
            mutable_snprintf( temp, 1024,
							  "yes : %3d\tno : %3d\t",
                              (int)args.yes,
                              (int)args.no );
			break;
        }

        case OP_TYPE::NU_SWITCH:
        case OP_TYPE::SC_SWITCH:
        case OP_TYPE::CO_SWITCH:
        case OP_TYPE::IM_SWITCH:
		case OP_TYPE::ME_SWITCH:
        case OP_TYPE::LA_SWITCH:
        case OP_TYPE::IN_SWITCH:
        {	
			const uint8_t* data = program.GetOpArgsPointer(at);
		
			OP::ADDRESS VarAddress;
			FMemory::Memcpy( &VarAddress, data, sizeof(OP::ADDRESS) );
			data += sizeof(OP::ADDRESS);
			
			OP::ADDRESS DefAddress;
			FMemory::Memcpy( &DefAddress, data, sizeof(OP::ADDRESS));
			data += sizeof(OP::ADDRESS);
	
			uint32_t CaseCount;
			FMemory::Memcpy( &CaseCount, data,  sizeof(uint32_t));
			data += sizeof(uint32_t);

            done = mutable_snprintf( temp, 1024, "variable : %3d", (int)VarAddress );

            for ( uint32_t C = 0; C < CaseCount; ++C )
            {
				int32_t Condition;
				FMemory::Memcpy( &Condition, data, sizeof(int32_t));
				data += sizeof(int32_t);
				
				OP::ADDRESS At;
				FMemory::Memcpy( &At, data, sizeof(OP::ADDRESS));
				data += sizeof(OP::ADDRESS);

                done += mutable_snprintf( temp+done, 1024-done, "\t[ %3d : %3d]", (int)Condition, (int)At);
			}

			done += mutable_snprintf( temp+done, 1024-done, "\tdefault : %3d", (int)DefAddress );
			break;
        }

		//-----------------------------------------------------------------------------------------
		case OP_TYPE::IM_RESIZE:
        {
            auto args = program.GetOpArgs<OP::ImageResizeArgs>(at);
            mutable_snprintf( temp, 1024,
							  "base : %3d\tsize  : %4d %4d",
                              (int)args.source,
                              (int)args.size[0],
                              (int)args.size[1] );
			break;
        }
		case OP_TYPE::IM_RESIZELIKE:
        {
            auto args = program.GetOpArgs<OP::ImageResizeLikeArgs>(at);
            mutable_snprintf( temp, 1024,
							  "base : %3d\tsize  : %3d",
                              (int)args.source,
                              (int)args.sizeSource );
			break;
        }
        case OP_TYPE::IM_PIXELFORMAT:
        {
            auto args = program.GetOpArgs<OP::ImagePixelFormatArgs>(at);
            mutable_snprintf( temp, 1024,
                              "base : %3d format : %2d  format-if-alpha : %2d",
                              (int)args.source,
                              (int)args.format,
                              (int)args.formatIfAlpha );
            break;
        }
        case OP_TYPE::IM_MIPMAP:
        {
            auto args = program.GetOpArgs<OP::ImageMipmapArgs>(at);
            mutable_snprintf( temp, 1024,
                              "base : %3d\tlevels : %2d\tblockLevels : %2d",
                              (int)args.source,
                              (int)args.levels,
                              (int)args.blockLevels );
            break;
        }
		case OP_TYPE::IM_BLANKLAYOUT:
        {
            auto args = program.GetOpArgs<OP::ImageBlankLayoutArgs>(at);
            mutable_snprintf( temp, 1024,
							  "block : %4d %4d  fmt  : %2d",
                              (int)args.blockSize[0],
                              (int)args.blockSize[1],
                              (int)args.format );
			break;
        }
		case OP_TYPE::IM_COMPOSE:
        {
            auto args = program.GetOpArgs<OP::ImageComposeArgs>(at);
            mutable_snprintf( temp, 1024,
							  "base : %3d\tblock image : %3d\tblock index : %3d\tlayout: %3d\tmask: %3d",
                              (int)args.base,
                              (int)args.blockImage,
                              (int)args.blockIndex,
                              (int)args.layout,
                              (int)args.mask );
			break;
        }
		case OP_TYPE::IM_INTERPOLATE:
        {
            auto args = program.GetOpArgs<OP::ImageInterpolateArgs>(at);
            done += mutable_snprintf( temp, 1024,
                                      "factor : %3d", args.factor);
			for (int t=0; t<MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++t)
			{
				done += mutable_snprintf( temp+done, 1024-done,
                                          "\tt%d : %3d", t, args.targets[t] );
			}
			break;
        }
		case OP_TYPE::IM_SWIZZLE:
        {
            auto args = program.GetOpArgs<OP::ImageSwizzleArgs>(at);
            done += mutable_snprintf( temp, 1024,
                              "format : %3d", args.format);
			for (int t=0; t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++t)
			{
				done += mutable_snprintf( temp+done, 1024-done,
							  "\tsource %d, channel %3d",
                          args.sources[t],
                          args.sourceChannels[t] );
			}
			break;
        }
		case OP_TYPE::IM_SELECTCOLOUR:
        {
            auto args = program.GetOpArgs<OP::ImageSelectColourArgs>(at);
            mutable_snprintf( temp, 1024,
							  "base : %3d\tcolour  : %3d",
                    (int)args.base,
                    (int)args.colour );
			break;
        }
		case OP_TYPE::IM_COLOURMAP:
        {
            auto args = program.GetOpArgs<OP::ImageColourMapArgs>(at);
            mutable_snprintf( temp, 1024,
							  "base : %3d\tmask  : %3d\tmap  : %3d",
                      (int)args.base,
                      (int)args.mask,
                      (int)args.map );
			break;
        }
		case OP_TYPE::IM_GRADIENT:
        {
            auto args = program.GetOpArgs<OP::ImageGradientArgs>(at);
            mutable_snprintf( temp, 1024,
							  "colour0 : %3d\tcolour1  : %3d\tsize  : %3d x %3d",
                      (int)args.colour0,
                      (int)args.colour1,
                      (int)args.size[0],
                      (int)args.size[1]
					  );
			break;
        }
		case OP_TYPE::IM_GPU:
		{
            auto args = program.GetOpArgs<OP::ImageGPUArgs>(at);
            mutable_snprintf( temp, 1024,
							  "program : %3d",
                              (int)args.program
							  );
			break;
		}

        case OP_TYPE::ME_MERGE:
        {
            auto args = program.GetOpArgs<OP::MeshMergeArgs>(at);
            mutable_snprintf( temp, 1024,
                              "base : %3d    added : %3d",
                    (int)args.base,
                    (int)args.added );
			break;
        }
		case OP_TYPE::ME_INTERPOLATE:
        {
            auto args = program.GetOpArgs<OP::MeshInterpolateArgs>(at);
            done += mutable_snprintf( temp, 1024,
							  "factor : %3d\tbase : %3d",
                              args.factor,
                              args.base);
			for (int t=0; t<MUTABLE_OP_MAX_INTERPOLATE_COUNT-1; ++t)
			{
				done += mutable_snprintf( temp+done, 1024-done,
                              "\tt%d : %3d", t, args.targets[t] );
			}
			break;
        }

		//-----------------------------------------------------------------------------------------
		default:
			//check( false );
			break;
		}

		temp[1023] = 0;
		return temp;
	}

}
