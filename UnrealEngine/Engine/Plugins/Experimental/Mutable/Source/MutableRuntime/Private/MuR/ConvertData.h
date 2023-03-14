// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Mesh.h"
#include "MuR/Platform.h"
#include "MuR/MutableMath.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//! Convert one channel element
	//---------------------------------------------------------------------------------------------
	inline void ConvertData
		(
			int channel,
			void* pResult, MESH_BUFFER_FORMAT resultFormat,
			const void* pSource, MESH_BUFFER_FORMAT sourceFormat
		)
	{
		switch ( resultFormat )
		{
		case MBF_FLOAT64:
		{
			double* pTypedResult = reinterpret_cast<double*>(pResult);
			uint8* pByteResult = reinterpret_cast<uint8*>(pResult);
			const uint8* pByteSource = reinterpret_cast<const uint8*>(pSource);

			switch (sourceFormat)
			{
			case MBF_FLOAT64:
			{
				// Just dereferencing is not safe in all architectures. some like ARM require the
				// floats to be 4-byte aligned and it may not be the case. We need memcpy.
				memcpy(pByteResult + 8 * channel, pByteSource + 8* channel, 8);
				break;
			}

			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>(pSource);
				pTypedResult[channel] = pTypedSource[channel];
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>(pSource);
				pTypedResult[channel] = halfToFloat(pTypedSource[channel]);
				break;
			}

			case MBF_INT32:
			{
				const int32* pTypedSource = reinterpret_cast<const int32*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				break;
			}

			case MBF_UINT32:
			{
				const uint32* pTypedSource = reinterpret_cast<const uint32*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				break;
			}

			case MBF_INT16:
			{
				const int16* pTypedSource = reinterpret_cast<const int16*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				break;
			}

			case MBF_UINT16:
			{
				const uint16* pTypedSource = reinterpret_cast<const uint16*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				break;
			}

			case MBF_INT8:
			{
				const int8* pTypedSource = reinterpret_cast<const int8*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				break;
			}

			case MBF_UINT8:
			{
				const uint8* pTypedSource = reinterpret_cast<const uint8*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				break;
			}

			case MBF_NINT32:
			{
				const int32* pTypedSource = reinterpret_cast<const int32*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				pTypedResult[channel] /= 65536.0f * 65536.0f / 2.0f;
				break;
			}

			case MBF_NUINT32:
			{
				const uint32* pTypedSource = reinterpret_cast<const uint32*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				pTypedResult[channel] /= 65536.0f * 65536.0f - 1.0f;
				break;
			}

			case MBF_NINT16:
			{
				const int16* pTypedSource = reinterpret_cast<const int16*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				pTypedResult[channel] /= 32768.0f;
				break;
			}

			case MBF_NUINT16:
			{
				const uint16* pTypedSource = reinterpret_cast<const uint16*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				pTypedResult[channel] /= 65535.0f;
				break;
			}

			case MBF_NINT8:
			{
				const int8* pTypedSource = reinterpret_cast<const int8*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				pTypedResult[channel] /= 128.0f;
				break;
			}

			case MBF_NUINT8:
			{
				const uint8* pTypedSource = reinterpret_cast<const uint8*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				pTypedResult[channel] /= 255.0f;
				break;
			}

			case MBF_PACKEDDIR8:
			case MBF_PACKEDDIR8_W_TANGENTSIGN:
			{
				const uint8* pTypedSource = reinterpret_cast<const uint8*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				pTypedResult[channel] /= 127.5f;
				pTypedResult[channel] -= 1.0f;
				break;
			}

			case MBF_PACKEDDIRS8:
			case MBF_PACKEDDIRS8_W_TANGENTSIGN:
			{
				const int8* pTypedSource = reinterpret_cast<const int8*>(pSource);
				pTypedResult[channel] = (double)(pTypedSource[channel]);
				pTypedResult[channel] /= 127.5f;
				break;
			}

			default:
				checkf(false, TEXT("Conversion not implemented."));
				break;
			}
			break;
		}

		case MBF_FLOAT32:
		{
			float* pTypedResult = reinterpret_cast<float*>( pResult );
            uint8* pByteResult = reinterpret_cast<uint8*>( pResult );
            const uint8* pByteSource = reinterpret_cast<const uint8*>( pSource );

			switch ( sourceFormat )
			{
			case MBF_FLOAT64:
			{
				const double* pTypedSource = reinterpret_cast<const double*>(pSource);
				pTypedResult[channel] = float(pTypedSource[channel]);
				break;
			}

			case MBF_FLOAT32:
			{
				// Just dereferencing is not safe in all architectures. some like ARM require the
				// floats to be 4-byte aligned and it may not be the case. We need memcpy.
				memcpy(pByteResult + 4 * channel, pByteSource + 4 * channel, 4);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
				pTypedResult[channel] = halfToFloat(pTypedSource[channel]);
				break;
			}

			case MBF_INT32:
			{
                const int32* pTypedSource = reinterpret_cast<const int32*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				break;
			}

			case MBF_UINT32:
			{
                const uint32* pTypedSource = reinterpret_cast<const uint32*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				break;
			}

			case MBF_INT16:
			{
                const int16* pTypedSource = reinterpret_cast<const int16*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				break;
			}

			case MBF_UINT16:
			{
                const uint16* pTypedSource = reinterpret_cast<const uint16*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				break;
			}

			case MBF_INT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				break;
			}

			case MBF_UINT8:
			{
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				break;
			}

			case MBF_NINT32:
			{
                const int32* pTypedSource = reinterpret_cast<const int32*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				pTypedResult[channel] /= 65536.0f*65536.0f/2.0f;
				break;
			}

			case MBF_NUINT32:
			{
                const uint32* pTypedSource = reinterpret_cast<const uint32*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				pTypedResult[channel] /= 65536.0f*65536.0f-1.0f;
				break;
			}

			case MBF_NINT16:
			{
                const int16* pTypedSource = reinterpret_cast<const int16*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				pTypedResult[channel] /= 32768.0f;
				break;
			}

			case MBF_NUINT16:
			{
                const uint16* pTypedSource = reinterpret_cast<const uint16*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				pTypedResult[channel] /= 65535.0f;
				break;
			}

			case MBF_NINT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				pTypedResult[channel] /= 128.0f;
				break;
			}

			case MBF_NUINT8:
			{
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
				pTypedResult[channel] = (float)(pTypedSource[channel]);
				pTypedResult[channel] /= 255.0f;
				break;
			}

            case MBF_PACKEDDIR8:
            case MBF_PACKEDDIR8_W_TANGENTSIGN:
            {
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
                pTypedResult[channel] = (float)(pTypedSource[channel]);
                pTypedResult[channel] /= 127.5f;
                pTypedResult[channel] -= 1.0f;
                break;
            }

            case MBF_PACKEDDIRS8:
            case MBF_PACKEDDIRS8_W_TANGENTSIGN:
            {
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
                pTypedResult[channel] = (float)(pTypedSource[channel]);
                pTypedResult[channel] /= 127.5f;
                break;
            }

			default:
				checkf( false, TEXT("Conversion not implemented.") );
				break;
			}
			break;
		}

		//-----------------------------------------------------------------------------------------
		case MBF_FLOAT16:
		{
			float16* pTypedResult = reinterpret_cast<float16*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
				pTypedResult[channel] = floatToHalf(pTypedSource[channel]);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
				pTypedResult[channel] = pTypedSource[channel];
				break;
			}

			case MBF_UINT32:
			{
                const uint32* pTypedSource = reinterpret_cast<const uint32*>( pSource );
				pTypedResult[channel] = floatToHalf((float)(pTypedSource[channel]));
				break;
			}

			case MBF_INT32:
			{
                const int32* pTypedSource = reinterpret_cast<const int32*>( pSource );
				pTypedResult[channel] = floatToHalf((float)(pTypedSource[channel]));
				break;
			}

			case MBF_UINT16:
			{
                const uint16* pTypedSource = reinterpret_cast<const uint16*>( pSource );
				pTypedResult[channel] = floatToHalf((float)(pTypedSource[channel]));
				break;
			}

			case MBF_INT16:
			{
                const int16* pTypedSource = reinterpret_cast<const int16*>( pSource );
				pTypedResult[channel] = floatToHalf((float)(pTypedSource[channel]));
				break;
			}

			case MBF_UINT8:
			{
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
				pTypedResult[channel] = floatToHalf((float)(pTypedSource[channel]));
				break;
			}

			case MBF_INT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
				pTypedResult[channel] = floatToHalf((float)(pTypedSource[channel]));
				break;
			}

			default:
				checkf( false, TEXT("Conversion not implemented.") );
				break;
			}
			break;
		}

		//-----------------------------------------------------------------------------------------
		case MBF_UINT8:
		{
            uint8* pTypedResult = reinterpret_cast<uint8*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (uint8)
                        FMath::Min<uint32>(
							0xFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (uint8)
                        FMath::Min<uint32>(
							0xFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)halfToFloat(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_INT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
                pTypedResult[channel] =  (uint8)FMath::Max<int8>( 0, pTypedSource[channel] );
				break;
			}

			case MBF_UINT8:
			{
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
				pTypedResult[channel] = pTypedSource[channel];
				break;
			}

			case MBF_INT16:
			{
                const int16* pTypedSource = reinterpret_cast<const int16*>( pSource );
                pTypedResult[channel] =  (uint8)
                        FMath::Min<int16>(
							0xFF,
                            FMath::Max<int16>(
								0,
								pTypedSource[channel]
								)
							);
				break;
			}

			case MBF_UINT16:
			{
				// Clamp
                const uint16* pTypedSource = reinterpret_cast<const uint16*>( pSource );
                pTypedResult[channel] = (uint8)
                        FMath::Min<uint16>(
							0xFF,
                            FMath::Max<uint16>(
								0,
								pTypedSource[channel]
								)
							);
				break;
			}

			case MBF_INT32:
			{
                const int32* pTypedSource = reinterpret_cast<const int32*>( pSource );
                pTypedResult[channel] =  (uint8)
                        FMath::Min<int32>(
							0xFF,
                            FMath::Max<int32>(
								0,
								pTypedSource[channel]
								)
							);
				break;
			}

			case MBF_UINT32:
			{
				// Clamp
                const uint32* pTypedSource = reinterpret_cast<const uint32*>( pSource );
                pTypedResult[channel] = (uint8)
                        FMath::Min<uint32>(
							0xFF,
                            FMath::Max<uint32>(
								0,
								pTypedSource[channel]
								)
							);
				break;
			}

			case MBF_NUINT8:
			case MBF_NUINT16:
			case MBF_NUINT32:
			case MBF_NINT8:
			case MBF_NINT16:
			case MBF_NINT32:
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_UINT16:
		{
            uint16* pTypedResult = reinterpret_cast<uint16*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (uint16)
                        FMath::Min<uint32>(
							0xFFFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (uint16)
                        FMath::Min<uint32>(
							0xFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)halfToFloat(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_UINT8:
			{
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
				pTypedResult[channel] =  pTypedSource[channel];
				break;
			}

			case MBF_INT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
                pTypedResult[channel] =  (uint16)FMath::Max<int8>( 0, pTypedSource[channel] );
				break;
			}

			case MBF_UINT16:
			{
                const uint16* pTypedSource = reinterpret_cast<const uint16*>( pSource );
				pTypedResult[channel] =  pTypedSource[channel];
				break;
			}

			case MBF_INT16:
			{
                const int16* pTypedSource = reinterpret_cast<const int16*>( pSource );
                pTypedResult[channel] =  (uint16)FMath::Max<int16>( 0, pTypedSource[channel] );
				break;
			}

			case MBF_UINT32:
			{
				// Clamp
                const uint32* pTypedSource = reinterpret_cast<const uint32*>( pSource );
                pTypedResult[channel] = (uint16)
                        FMath::Min<uint32>(
							0xFFFF,
                            FMath::Max<uint32>(
								0,
								pTypedSource[channel]
								)
							);
				break;
			}

			case MBF_INT32:
			{
				// Clamp
                const int32* pTypedSource = reinterpret_cast<const int32*>( pSource );
                pTypedResult[channel] = (uint16)
                        FMath::Min<int32>(
							0xFFFF,
                            FMath::Max<int32>(
								0,
								pTypedSource[channel]
								)
							);
				break;
			}

			case MBF_NUINT8:
			case MBF_NUINT16:
			case MBF_NUINT32:
			case MBF_NINT8:
			case MBF_NINT16:
			case MBF_NINT32:
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_UINT32:
		{
            uint32* pTypedResult = reinterpret_cast<uint32*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
				pTypedResult[channel] =
                        FMath::Min<uint32>(
							0xFFFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
				pTypedResult[channel] =
                        FMath::Min<uint32>(
							0xFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)halfToFloat(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_UINT8:
			{
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
				pTypedResult[channel] =  pTypedSource[channel];
				break;
			}

			case MBF_INT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
                pTypedResult[channel] =  (uint16)FMath::Max<int8>( 0, pTypedSource[channel] );
				break;
			}

			case MBF_UINT16:
			{
                const uint16* pTypedSource = reinterpret_cast<const uint16*>( pSource );
				pTypedResult[channel] =  pTypedSource[channel];
				break;
			}

			case MBF_INT16:
			{
                const int16* pTypedSource = reinterpret_cast<const int16*>( pSource );
                pTypedResult[channel] =  (uint16)FMath::Max<int16>( 0, pTypedSource[channel] );
				break;
			}

			case MBF_UINT32:
			{
                const uint32* pTypedSource = reinterpret_cast<const uint32*>( pSource );
				pTypedResult[channel] = pTypedSource[channel];
				break;
			}

			case MBF_INT32:
			{
				// Clamp
                const int32* pTypedSource = reinterpret_cast<const int32*>( pSource );
                pTypedResult[channel] = (uint32)FMath::Max<int32>( 0, pTypedSource[channel] );
				break;
			}

			case MBF_NUINT8:
			case MBF_NUINT16:
			case MBF_NUINT32:
			case MBF_NINT8:
			case MBF_NINT16:
			case MBF_NINT32:
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_INT8:
		{
            int8* pTypedResult = reinterpret_cast<int8*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (int8)
                        FMath::Min<int32>(
							127,
                            FMath::Max<int32>(
								-128,
                                (int32)(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (int8)
                        FMath::Min<int32>(
							127,
                            FMath::Max<int32>(
								-128,
                                (int32)halfToFloat(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_INT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
				pTypedResult[channel] = pTypedSource[channel];
				break;
			}

			case MBF_NUINT8:
			case MBF_NUINT16:
			case MBF_NUINT32:
			case MBF_NINT8:
			case MBF_NINT16:
			case MBF_NINT32:
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_INT16:
		{
            int16* pTypedResult = reinterpret_cast<int16*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (int16)
                        FMath::Min<int32>(
							32767,
                            FMath::Max<int32>(
								-32768,
                                (int32)(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (int16)
                        FMath::Min<int32>(
							32767,
                            FMath::Max<int32>(
								-32768,
                                (int32)halfToFloat(pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_INT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
                pTypedResult[channel] = (int16)pTypedSource[channel];
				break;
			}

			case MBF_UINT8:
			{
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
                pTypedResult[channel] = (int16)pTypedSource[channel];
				break;
			}

			case MBF_UINT16:
			{
                const uint16* pTypedSource = reinterpret_cast<const uint16*>( pSource );
                pTypedResult[channel] = (int16)
                        FMath::Min<int32>(
                            32767, (int32)pTypedSource[channel]
							);
				break;
			}

			case MBF_INT32:
			{
                const int32* pTypedSource = reinterpret_cast<const int32*>( pSource );
                pTypedResult[channel] = (int16)
                        FMath::Min<int32>(
							32767,
                            FMath::Max<int32>(
								-32768,
								pTypedSource[channel]
								)
							);
				break;
			}

			case MBF_UINT32:
			{
                const uint32* pTypedSource = reinterpret_cast<const uint32*>( pSource );
                pTypedResult[channel] = (int16)
                        FMath::Min<int32>(
							32767, pTypedSource[channel]
							);
				break;
			}

			case MBF_NUINT8:
			case MBF_NUINT16:
			case MBF_NUINT32:
			case MBF_NINT8:
			case MBF_NINT16:
			case MBF_NINT32:
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_INT32:
		{
            int32* pTypedResult = reinterpret_cast<int32*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (int32)(pTypedSource[channel]);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (int32)halfToFloat(pTypedSource[channel]);
				break;
			}

			case MBF_INT8:
			{
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
                pTypedResult[channel] = (int32)pTypedSource[channel];
				break;
			}

			case MBF_UINT8:
			{
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
                pTypedResult[channel] = (int32)pTypedSource[channel];
				break;
			}

			case MBF_INT16:
			{
                const int16* pTypedSource = reinterpret_cast<const int16*>( pSource );
                pTypedResult[channel] = (int32)pTypedSource[channel];
				break;
			}

			case MBF_UINT16:
			{
                const uint16* pTypedSource = reinterpret_cast<const uint16*>( pSource );
                pTypedResult[channel] = (int32)pTypedSource[channel];
				break;
			}

			case MBF_UINT32:
			{
                const uint32* pTypedSource = reinterpret_cast<const uint32*>( pSource );
                pTypedResult[channel] = (int32)pTypedSource[channel];
				break;
			}

			case MBF_INT32:
			{
                const int32* pTypedSource = reinterpret_cast<const int32*>( pSource );
                pTypedResult[channel] = pTypedSource[channel];
				break;
			}

			case MBF_NUINT8:
			case MBF_NUINT16:
			case MBF_NUINT32:
			case MBF_NINT8:
			case MBF_NINT16:
			case MBF_NINT32:
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		//-----------------------------------------------------------------------------------------
		case MBF_NUINT8:
		{
            uint8* pTypedResult = reinterpret_cast<uint8*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_NUINT8:
			{
				auto pTypedSource = reinterpret_cast<const uint8*>(pSource);
				pTypedResult[channel] = pTypedSource[channel];
				break;
			}

			case MBF_NUINT16:
			{
				const uint16* pTypedSource = reinterpret_cast<const uint16*>(pSource);
				pTypedResult[channel] = pTypedSource[channel] / (65535 / 255);
				break;
			}

			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>(pSource);
				pTypedResult[channel] = (uint8)
					FMath::Min<uint32>(
						0xFF,
						FMath::Max<uint32>(
							0,
							(uint32)(((float)0xFF)*pTypedSource[channel] + 0.5f)
							)
						);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (uint8)
                        FMath::Min<uint32>(
							0xFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)(((float)0xFF)*halfToFloat(pTypedSource[channel])+0.5f)
								)
							);
				break;
			}

			case MBF_UINT8:
			case MBF_UINT16:
			case MBF_UINT32:
			case MBF_INT8:
			case MBF_INT16:
			case MBF_INT32:
				// TODO: 1 or 0
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_NUINT16:
		{
            uint16* pTypedResult = reinterpret_cast<uint16*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_NUINT16:
			{
				auto pTypedSource = reinterpret_cast<const uint16*>(pSource);
				pTypedResult[channel] = pTypedSource[channel];
				break;
			}

			case MBF_NUINT8:
			{
				const uint8* pTypedSource = reinterpret_cast<const uint8*>(pSource);
				pTypedResult[channel] = pTypedSource[channel] * (65535 / 255);
				break;
			}

			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (uint16)
                        FMath::Min<uint32>(
							0xFFFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)(((float)0xFFFF)*pTypedSource[channel]+0.5f)
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (uint16)
                        FMath::Min<uint32>(
							0xFFFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)(((float)0xFFFF)*halfToFloat(pTypedSource[channel])+0.5f)
								)
							);
				break;
			}

			case MBF_UINT8:
			case MBF_UINT16:
			case MBF_UINT32:
			case MBF_INT8:
			case MBF_INT16:
			case MBF_INT32:
				// TODO: 1 or 0
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_NUINT32:
		{
            uint32* pTypedResult = reinterpret_cast<uint32*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (uint32)
                        FMath::Min<uint32>(
							0xFFFFFFFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)(((float)0xFFFFFFFF)*pTypedSource[channel])
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (uint32)
                        FMath::Min<uint32>(
							0xFFFFFFFF,
                            FMath::Max<uint32>(
								0,
                                (uint32)(((float)0xFFFFFFFF)*halfToFloat(pTypedSource[channel])+0.5f)
								)
							);
				break;
			}

			case MBF_UINT8:
			case MBF_UINT16:
			case MBF_UINT32:
			case MBF_INT8:
			case MBF_INT16:
			case MBF_INT32:
				// TODO: 1 or 0
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_NINT8:
		{
            int8* pTypedResult = reinterpret_cast<int8*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (int8)
                        FMath::Min<int32>(
							127,
                            FMath::Max<int32>(
								-128,
                                (int32)(128.0f*pTypedSource[channel]+0.5f)
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (int8)
                        FMath::Min<int32>(
							127,
                            FMath::Max<int32>(
								-128,
                                (int32)(128.0f*halfToFloat(pTypedSource[channel])+0.5f)
								)
							);
				break;
			}

			case MBF_UINT8:
			case MBF_UINT16:
			case MBF_UINT32:
			case MBF_INT8:
			case MBF_INT16:
			case MBF_INT32:
				// TODO: -1, 1 or 0
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

		case MBF_NINT16:
		{
            int16* pTypedResult = reinterpret_cast<int16*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (int16)
                        FMath::Min<int32>(
							32767,
                            FMath::Max<int32>(
								-32768,
                                (int32)(32768.0f*pTypedSource[channel]+0.5f)
								)
							);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (int16)
                        FMath::Min<int32>(
							32767,
                            FMath::Max<int32>(
								-32768,
                                (int32)(32768.0f*halfToFloat(pTypedSource[channel])+0.5f)
								)
							);
				break;
			}

			case MBF_UINT8:
			case MBF_UINT16:
			case MBF_UINT32:
			case MBF_INT8:
			case MBF_INT16:
			case MBF_INT32:
				// TODO: -1, 1 or 0
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}


		case MBF_NINT32:
		{
            int32* pTypedResult = reinterpret_cast<int32*>( pResult );

			switch ( sourceFormat )
			{
			case MBF_FLOAT32:
			{
				const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                pTypedResult[channel] = (int32)(2147483648.0f*pTypedSource[channel]+0.5f);
				break;
			}

			case MBF_FLOAT16:
			{
				const float16* pTypedSource = reinterpret_cast<const float16*>( pSource );
                pTypedResult[channel] = (int32)(2147483648.0f*halfToFloat(pTypedSource[channel])+0.5f);
				break;
			}

			case MBF_UINT8:
			case MBF_UINT16:
			case MBF_UINT32:
			case MBF_INT8:
			case MBF_INT16:
			case MBF_INT32:
				// TODO: -1, 1 or 0
				pTypedResult = 0;
				break;

			default:
				checkf( false, TEXT("Conversion not implemented." ) );
				break;
			}
			break;
		}

        case MBF_PACKEDDIR8:
        case MBF_PACKEDDIR8_W_TANGENTSIGN:
        {
            uint8* pTypedResult = reinterpret_cast<uint8*>( pResult );

            switch ( sourceFormat )
            {
            case MBF_PACKEDDIR8:
            case MBF_PACKEDDIR8_W_TANGENTSIGN:
            {
                const uint8* pTypedSource = reinterpret_cast<const uint8*>( pSource );
                uint8 source = pTypedSource[channel];
                pTypedResult[channel] = source;
                break;
            }

            case MBF_FLOAT32:
            {
                const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                float source = pTypedSource[channel];
                source = (source*0.5f+0.5f)*255.0f;
                pTypedResult[channel] =
                        (uint8)FMath::Min<float>( 255.0f, FMath::Max<float>( 0.0f, source ) );
                break;
            }

            default:
                checkf( false, TEXT("Conversion not implemented." ) );
                break;
            }
            break;
        }

        case MBF_PACKEDDIRS8:
        case MBF_PACKEDDIRS8_W_TANGENTSIGN:
        {
            int8* pTypedResult = reinterpret_cast<int8*>( pResult );

            switch ( sourceFormat )
            {
            case MBF_PACKEDDIRS8:
            case MBF_PACKEDDIRS8_W_TANGENTSIGN:
            {
                const int8* pTypedSource = reinterpret_cast<const int8*>( pSource );
                int8 source = pTypedSource[channel];
                pTypedResult[channel] = source;
                break;
            }

            case MBF_FLOAT32:
            {
                const float* pTypedSource = reinterpret_cast<const float*>( pSource );
                float source = pTypedSource[channel];
                source = source*0.5f*255.0f;
                pTypedResult[channel] =
                        (int8)FMath::Min<float>( 127.0f, FMath::Max<float>( -128.0f, source ) );
                break;
            }

            default:
                checkf( false, TEXT("Conversion not implemented." ) );
                break;
            }
            break;
        }

		default:
			checkf( false, TEXT("Conversion not implemented." ) );
			break;
		}

	}

}
