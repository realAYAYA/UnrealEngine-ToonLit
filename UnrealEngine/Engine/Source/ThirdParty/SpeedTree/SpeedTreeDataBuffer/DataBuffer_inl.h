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
//	CArray::CArray

template<typename T>
ST_INLINE CArray<T>::CArray(void) :
	m_pData(NULL)
{
}


///////////////////////////////////////////////////////////////////////
//	CArray::IsEmpty

template<typename T>
ST_INLINE st_bool CArray<T>::IsEmpty(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return (*(reinterpret_cast<st_uint32*>(m_pData)) > 0);
}


///////////////////////////////////////////////////////////////////////
//	CArray::Count

template<typename T>
ST_INLINE st_uint32 CArray<T>::Count(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return *(reinterpret_cast<st_uint32*>(m_pData));
}


///////////////////////////////////////////////////////////////////////
//	CArray::Data

template<typename T>
ST_INLINE const T* CArray<T>::Data(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return reinterpret_cast<T*>(m_pData + sizeof(st_uint32));
}


///////////////////////////////////////////////////////////////////////
//	CArray::operator[]

template<typename T>
ST_INLINE const T& CArray<T>::operator[](st_uint32 uiIndex) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	st_assert(uiIndex < Count( ), "Index out of range");
	return (Data( )[uiIndex]);
}


///////////////////////////////////////////////////////////////////////
//	CUntypedArray::CUntypedArray

ST_INLINE CUntypedArray::CUntypedArray(void) :
	m_pData(NULL)
{
}


///////////////////////////////////////////////////////////////////////
//	CUntypedArray::IsEmpty

ST_INLINE st_bool CUntypedArray::IsEmpty(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return (reinterpret_cast<st_uint32*>(m_pData)[0] > 0);
}


///////////////////////////////////////////////////////////////////////
//	CUntypedArray::Count

ST_INLINE st_uint32 CUntypedArray::Count(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return reinterpret_cast<st_uint32*>(m_pData)[0];
}


///////////////////////////////////////////////////////////////////////
//	CUntypedArray::ElementSize

ST_INLINE st_uint32 CUntypedArray::ElementSize(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return reinterpret_cast<st_uint32*>(m_pData)[1];
}


///////////////////////////////////////////////////////////////////////
//	CUntypedArray::Data

ST_INLINE const st_byte* CUntypedArray::Data(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return (m_pData + 2 * sizeof(st_uint32));
}


///////////////////////////////////////////////////////////////////////
//	CUntypedArray::operator[]

ST_INLINE const st_byte* CUntypedArray::operator[](st_uint32 uiIndex) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	st_assert(uiIndex < Count( ), "Index out of range");
	return (Data( ) + uiIndex * ElementSize( ));
}


///////////////////////////////////////////////////////////////////////
//	CString::IsEmpty

ST_INLINE st_bool CString::IsEmpty(void) const
{
	return (Count( ) < 2);
}


///////////////////////////////////////////////////////////////////////
//	CString::Length

ST_INLINE st_uint32 CString::Length(void) const
{
	return (Count( ) - 1);
}


///////////////////////////////////////////////////////////////////////
//	CTable::CTable

ST_INLINE CTable::CTable(void) :
	m_pData(NULL)
{
}


///////////////////////////////////////////////////////////////////////
//	CTable::Count

ST_INLINE st_uint32 CTable::Count(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return *(reinterpret_cast<st_uint32*>(m_pData));
}


///////////////////////////////////////////////////////////////////////
//	CTable::GetValue

template <typename T>
ST_INLINE const T& CTable::GetValue(st_uint32 uiIndex) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	st_assert(uiIndex < Count( ), "Index out of range");
	st_uint32 uiDataIndex = *(reinterpret_cast<st_uint32*>(m_pData) + uiIndex + 1);
	return *(reinterpret_cast<T*>(m_pData + uiDataIndex));
}


///////////////////////////////////////////////////////////////////////
//	CTable::GetContainer

template <typename T>
ST_INLINE T CTable::GetContainer(st_uint32 uiIndex) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	st_assert(uiIndex < Count( ), "Index out of range");
	st_uint32 uiDataIndex = *(reinterpret_cast<st_uint32*>(m_pData) + uiIndex + 1);
	T tReturn;
	tReturn.m_pData = m_pData + uiDataIndex;
	return tReturn;
}


///////////////////////////////////////////////////////////////////////
//	CTableArray::operator[]

template<typename T>
ST_INLINE T CTableArray<T>::operator[](st_uint32 uiIndex) const
{
	return GetContainer<T>(uiIndex);
}


///////////////////////////////////////////////////////////////////////
//	CReader::CReader

ST_INLINE CReader::CReader(void) :
	m_pFileData(NULL),
	m_bOwnsData(false)
{
}


///////////////////////////////////////////////////////////////////////
//	CReader::~CReader

ST_INLINE CReader::~CReader(void)
{
	Clear( );
}


///////////////////////////////////////////////////////////////////////
//	CReader::CReader

ST_INLINE st_bool CReader::Valid(void)
{
	return (m_pData != NULL);
}


///////////////////////////////////////////////////////////////////////
//	CReader::Clear

ST_INLINE void CReader::Clear(void)
{
	if (m_bOwnsData && m_pFileData != NULL)
	{
		delete [] m_pFileData;
	}

	m_pFileData = NULL;
	m_bOwnsData = false;
	m_pData = NULL;
}


///////////////////////////////////////////////////////////////////////
//	CReader::LoadFile

ST_INLINE st_bool CReader::LoadFile(const st_char* pFilename)
{
	st_bool bReturn = false;
	Clear( );

	FILE* pFile = NULL;
	#if defined(MultiByteToWideChar)
		fopen_s(&pFile, pFilename, "rb");
		if (pFile == NULL)
		{
			// try utf-8
			int iLength = MultiByteToWideChar(CP_UTF8, 0, pFilename, -1, NULL , 0);
			wchar_t* pWideString = new wchar_t[iLength];
			MultiByteToWideChar(CP_UTF8, 0 , pFilename, -1, pWideString, iLength);
			pFile = _wfopen(pWideString, L"rb");
			delete[] pWideString;
		}
	#else
		pFile = fopen(pFilename, "rb");
	#endif

	if (pFile != NULL)
	{
		fseek(pFile, 0L, SEEK_END);
		st_int32 iNumBytes = ftell(pFile);
		st_int32 iErrorCode = fseek(pFile, 0L, SEEK_SET);
		if (iNumBytes > 0 && iErrorCode >= 0)
		{
			m_pFileData = new st_byte[iNumBytes];
			m_bOwnsData = true;
			st_int32 iNumBytesRead = st_int32(fread(m_pFileData, 1, iNumBytes, pFile));
			if (iNumBytesRead == iNumBytes)
			{
				const st_char* pToken = FileToken( );
				st_int32 iTokenLength = st_int32(strlen(pToken));
				if (iTokenLength < iNumBytesRead)
				{
					m_pData = m_pFileData + iTokenLength;
					bReturn = true;
					for (st_int32 i = 0; (i < iTokenLength) && bReturn; ++i)
					{
						if (pToken[i] != m_pFileData[i])
						{
							bReturn = false;
						}
					}
				}
			}
		}

		fclose(pFile);
	}

	if (!bReturn)
	{
		Clear( );
	}

	return bReturn;
}


///////////////////////////////////////////////////////////////////////
//	CReader::LoadFromData

ST_INLINE st_bool CReader::LoadFromData(const st_byte* pData, st_int32 iSize)
{
	bool bReturn = false;
	Clear( );

	m_pFileData = const_cast<st_byte*>(pData);
	m_bOwnsData = false;

	const st_char* pToken = FileToken( );
	st_int32 iTokenLength = st_int32(strlen(pToken));
	if (iTokenLength < iSize)
	{
		m_pData = m_pFileData + iTokenLength;
		bReturn = true;
		for (st_int32 i = 0; (i < iTokenLength) && bReturn; ++i)
		{
			if (pToken[i] != m_pFileData[i])
			{
				bReturn = false;
			}
		}
	}

	if (!bReturn)
	{
		Clear( );
	}

	return bReturn;
}


///////////////////////////////////////////////////////////////////////
//	CReader::ClearAfter

ST_INLINE st_bool CReader::ClearAfter(st_uint32 uiIndex)
{
	if (!m_bOwnsData)
	{
		return false;
	}

	// copy data
	const st_uint32 uiZeroPadding = 10;
	st_uint32 uiDataIndex = *(reinterpret_cast<st_uint32*>(m_pData) + uiIndex + 2);
	st_byte* pEnd = reinterpret_cast<st_byte*>(m_pData + uiDataIndex);
	st_uint32 uiDataSize = static_cast<st_uint32>(pEnd - m_pFileData) + uiZeroPadding;
	st_byte* pNewData = new st_byte[uiDataSize];
	memcpy(pNewData, m_pFileData, uiDataSize);
	memset(pNewData + uiDataSize - uiZeroPadding, 0, uiZeroPadding);

	// use new data
	delete [] m_pFileData;
	m_pFileData = pNewData;
	const st_char* pToken = FileToken( );
	st_int32 iTokenLength = st_int32(strlen(pToken));
	m_pData = m_pFileData + iTokenLength;

	// zero out the rest of the table
	st_uint32 uiCount = Count( );
	st_uint32 uiLastDataIndex = uiDataSize - uiZeroPadding;
	for (st_uint32 i = uiIndex + 1; i < uiCount; ++i)
	{
		*(reinterpret_cast<st_uint32*>(m_pData) + i + 1) = uiLastDataIndex;
	}

	return true;
}

