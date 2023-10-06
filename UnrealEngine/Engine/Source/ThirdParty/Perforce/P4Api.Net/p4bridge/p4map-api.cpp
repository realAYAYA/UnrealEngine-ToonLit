/*******************************************************************************

Copyright (c) 2010, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: p4-map-api.cc
 *
 * Author	: dbb
 *
 * Description	: A "Flat C" interface for the MapApi object in the Perforce 
 *        API. Used to provide simple access for C#.NET using P/Invoke and 
 *		  dllimport.
 *
 ******************************************************************************/

#include "stdafx.h"

#include "P4BridgeServer.h"

#include <mapapi.h>

class P4MapApi : public p4base
{
public:
    MapApi * _mapApi;

    P4MapApi()
        : p4base(tP4MapApi) 
    {
        _mapApi = new MapApi();
    }

    P4MapApi(MapApi * _m)
        : p4base(tP4MapApi) 
    {
        _mapApi = _m;
    }

    virtual ~P4MapApi() 
    {
        delete _mapApi;
    }
    
    virtual int Type(void) 
    { 
        return tP4MapApi; 
    }
};

/******************************************************************************
 * 'Flat' C interface for the dll. This interface will be imported into C# 
 *    using P/Invoke 
******************************************************************************/

extern "C" 
{

    /**************************************************************************
     * Helper function to create a new MapApi object.
     *
     *  NOTE: Call DeletMapApi() on the returned pointer to free the object
     *
     **************************************************************************/
    P4BRIDGE_API void * CreateMapApi()
    {
        return (void *) new P4MapApi();
    }

    /**************************************************************************
     * Helper function to delete a MapApi object allocated by CreateMApApi().
     **************************************************************************/
    P4BRIDGE_API void DeleteMapApi( P4MapApi * pMap )
    {
        delete pMap;
    }

    /**************************************************************************
    *
    * P4MapApi functions
    *
    *    These are the functions that use a MapApi* to access an object 
    *      created in the dll.
    *
    **************************************************************************/

    /**************************************************************************
    *
    *  Clear: Clear all the data
    *
    *   pMap:	 Pointer to the P4MapApi instance 
    *
    *   Returns: void.
    *
    **************************************************************************/

    P4BRIDGE_API void Clear( P4MapApi *pMap )
    {
        VALIDATE_HANDLE_V(pMap, tP4MapApi)

        pMap->_mapApi->Clear();
    }

    /**************************************************************************
    *
    *  Count:Return the number of entries in the map
    *
    *   pMap:	 Pointer to the P4MapApi instance 
    *
    *   Returns: int, Number of map entries.
    *
    **************************************************************************/

    P4BRIDGE_API int Count( P4MapApi *pMap )
    {
        VALIDATE_HANDLE_I(pMap, tP4MapApi)

        return pMap->_mapApi->Count();
    }

    /**************************************************************************
    *
    *  GetLeft: Return the left side of the specified entry in the map
    *
    *   pMap:	 Pointer to the P4MapApi instance 
    *   i:		 Index of the desired entry
    *
    *   Returns: char *, a string representing the left side of the entry.
    *
    **************************************************************************/

    P4BRIDGE_API const char * GetLeft( P4MapApi *pMap, int i )
    {
        VALIDATE_HANDLE_P(pMap, tP4MapApi)

        return pMap->_mapApi->GetLeft( i )->Text();
    }

    /**************************************************************************
    *
    *  GetRight: Return the right side of the specified entry in the map
    *
    *   pMap:	 Pointer to the P4MapApi instance 
    *   i:		 Index of the desired entry
    *
    *   Returns: char *, a string representing the right side of the entry.
    *
    **************************************************************************/

    P4BRIDGE_API const char * GetRight( P4MapApi *pMap, int i )
    {
        VALIDATE_HANDLE_P(pMap, tP4MapApi)

		const StrPtr *r = pMap->_mapApi->GetRight( i );
		if (r != NULL)
			return r->Text();
		return NULL;
    }

    /**************************************************************************
    *
    *  GetType: Return the type of the specified entry in the map
    *
    *   pMap:	 Pointer to the P4MapApi instance 
    *   i:		 Index of the desired entry
    *
    *   Returns: int, The integer value of the MapType enumeration.
    *
    **************************************************************************/

    P4BRIDGE_API int GetType( P4MapApi *pMap, int i )
    {
        VALIDATE_HANDLE_I(pMap, tP4MapApi)

        return pMap->_mapApi->GetType( i );
    }

    /**************************************************************************
    *
    *  Insert: Adds a new entry in the map
    *
    *   pMap:	 Pointer to the P4MapApi instance 
    *   lr:		 String representing both the the left and right sides of the 
    *				new entry
    *   t:		 Type of the new entry
    *
    *   Returns: void
    *
    **************************************************************************/

    P4BRIDGE_API void Insert1( P4MapApi *pMap, const char * lr, int t )
    {
        VALIDATE_HANDLE_V(pMap, tP4MapApi)
        StrBuf lrs(lr);

        return pMap->_mapApi->Insert( lrs, (MapType) t );
    }

    /**************************************************************************
    *
    *  Insert: Adds a new entry in the map
    *
    *   pMap:	 Pointer to the P4MapApi instance 
    *   l:		 String representing the the left side of the new entry
    *   r:		 String representing the the right side of the new entry
    *   t:		 Type of the new entry
    *
    *   Returns: void
    *
    **************************************************************************/

    P4BRIDGE_API void Insert2( P4MapApi *pMap, const char * l, 
                                       const char * r, int t )
    {
        VALIDATE_HANDLE_V(pMap, tP4MapApi)
        StrBuf ls(l);
        StrBuf rs(r);

        return pMap->_mapApi->Insert( ls, rs, (MapType) t );
    }

    /**************************************************************************
    *
    *  Join: Combine two MapApis to create a new MapApi
    *
    *   pLeft:	 Pointer to the first map
    *   pRight:	 Pointer to the second map
    *
    *   Returns: MapApi * to the new map
    *
    **************************************************************************/

    P4BRIDGE_API void * Join1( P4MapApi *pLeft, P4MapApi *pRight )
    {
        VALIDATE_HANDLE_P(pLeft, tP4MapApi)
        VALIDATE_HANDLE_P(pRight, tP4MapApi)
        MapApi *j =  MapApi::Join( pLeft->_mapApi, pRight->_mapApi );
        if (j)
            return new P4MapApi( j );
        return NULL;
    }

    /**************************************************************************
    *
    *  Join: Combine two MapApis to create a new MapApi
    *
    *   pLeft:	  Pointer to the first map
    *   leftDir:  orientation of the first map
    *   pRight:	  Pointer to the second map
    *   rightDir: orientation of the second map
    *
    *   Returns: MapApi * to the new map
    *
    **************************************************************************/

    P4BRIDGE_API void * Join2( P4MapApi *pLeft, int ld,
                                       P4MapApi *pRight, int rd )
    {
        VALIDATE_HANDLE_P(pLeft, tP4MapApi)
        VALIDATE_HANDLE_P(pRight, tP4MapApi)
        MapApi * j = MapApi::Join( pLeft->_mapApi, (MapDir) ld, 
                                            pRight->_mapApi,(MapDir)  rd );
        if (j)
            return new P4MapApi( j );
        return NULL;
    }

    /**************************************************************************
    *
    *  Translate: Translate a file path from on side of the mapping to the 
    *				other.
    *
    *   pMap:	 Pointer to the P4MapApi instance 
    *   p:		 String representing the the path to translate
    *   d:		 The direction to perform the translation L->R or R->L
    *
    *   Returns: char * String translate path, NULL if translation failed
    *
    **************************************************************************/

    P4BRIDGE_API const char * Translate( P4MapApi *pMap, const char * p,
                                                    MapDir d)
    {
        VALIDATE_HANDLE_P(pMap, tP4MapApi)
        StrBuf ps(p);
        StrBuf rs;

        if ( pMap->_mapApi->Translate( ps, rs, d ))
        {
            return Utils::AllocString(rs.Text());
        }
        return NULL;
    }
}