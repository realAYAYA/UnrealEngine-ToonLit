///////////////////////////////////////////////////////////////////////
//
//  *** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with
//  the terms of that agreement.
//
//      Copyright (c) 2003-2017 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com

///////////////////////////////////////////////////////////////////////
//  Preprocessor

#pragma once
#include "Platform.h"


///////////////////////////////////////////////////////////////////////
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////
//	namespace SpeedTree8

namespace SpeedTreeDataBuffer
{
	///////////////////////////////////////////////////////////////////////
	//	class CArray

	template<typename T>
	class CArray
	{
	friend class CTable;
	public:
						CArray(void);

			st_bool		IsEmpty(void) const;
			st_uint32	Count(void) const;
			const T*	Data(void) const;
			const T&	operator [] (st_uint32 uiIndex) const;

	private:
			st_byte*	m_pData;
	};


	///////////////////////////////////////////////////////////////////////
	//	class CUntypedArray

	class CUntypedArray
	{
	friend class CTable;
	public:
							CUntypedArray(void);

			st_bool			IsEmpty(void) const;
			st_uint32		Count(void) const;
			st_uint32		ElementSize(void) const;
			const st_byte*	Data(void) const;
			const st_byte*	operator [] (st_uint32 uiIndex) const;

	private:
			st_byte*		m_pData;
	};


	///////////////////////////////////////////////////////////////////////
	//	CString

	class CString : public CArray<st_char>
	{
	public:
			st_bool		IsEmpty(void) const;
			st_uint32	Length(void) const;
	};


	///////////////////////////////////////////////////////////////////////
	//	class CTable

	class CTable
	{
	public:
						CTable(void);

			st_uint32	Count(void) const;

	protected:
			template <typename T>
			const T&	GetValue(st_uint32 uiIndex) const;

			template <typename T>
			T			GetContainer(st_uint32 uiIndex) const;

	public:
			st_byte*	m_pData;
	};


	///////////////////////////////////////////////////////////////////////
	//	class CTableArray

	template<typename T>
	class CTableArray : public CTable
	{
	public:
			T			operator [] (st_uint32 uiIndex) const;
	};


	///////////////////////////////////////////////////////////////////////
	//	class CReader

	class CReader : public CTable
	{
	public:
								CReader(void);
	virtual						~CReader(void);

			st_bool				Valid(void);
			void				Clear(void);

			st_bool				LoadFile(const st_char* pFilename);
		#ifdef WIN32
			st_bool				LoadFileU(const st_wchar* pFilename);
		#endif

			st_bool				LoadFromData(const st_byte* pData, st_int32 iSize);

			st_bool				ClearAfter(st_uint32 uiIndex);

	protected:

	virtual const st_char*		FileToken(void) const = 0;

	private:
			st_byte*			m_pFileData;
			st_bool				m_bOwnsData;
	};


	// include inline functions
	#include "DataBuffer_inl.h"

} // end namespace SpeedTreeDataBuffer


#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

