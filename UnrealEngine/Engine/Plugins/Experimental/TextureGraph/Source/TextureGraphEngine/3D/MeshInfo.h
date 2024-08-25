// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GraphicsDefs.h"
#include "Helper/DataUtil.h"
#include <memory>
#include <array>
#include <unordered_map>
#include <limits>

struct CoreMesh;
typedef std::shared_ptr<CoreMesh>	CoreMeshPtr;

class MeshDetails;
typedef std::shared_ptr<MeshDetails> MeshDetailsPtr;

class MeshDetails_Tri;

class TEXTUREGRAPHENGINE_API MeshInfo
{
protected:
    typedef std::unordered_map<const char*, MeshDetailsPtr> MeshDetailsPtrLookup;

	CoreMeshPtr					_cmesh;				/// The core mesh data structure that should've been loaded by now
	CHashPtr					_hash;				/// The hash for the mesh
	mutable FCriticalSection	_detailsMutex;		/// Mutex for details
    MeshDetailsPtrLookup        _details;           /// Details lookup

public:
								MeshInfo(CoreMeshPtr cmesh);
								~MeshInfo();

	size_t						NumVertices() const;
	size_t						NumTriangles() const;
	std::array<Vector3, 3>		Vertices(int32 i0, int32 i1, int32 i2) const;
	int32		                GetMaterialIndex();
    void                        InitBounds(FVector min, FVector max);
    void                        UpdateBounds(const FVector& vert);

	//////////////////////////////////////////////////////////////////////////
    MeshDetails_Tri*            d_Tri();

    template <typename T_Details>
    T_Details* GetAttribute(const char* name, bool create = true) 
    {
		FScopeLock lock(&_detailsMutex);
        {
            auto iter = _details.find(name);

            /// If the details already exists
            if (iter != _details.end())
                return static_cast<T_Details*>(iter->second.get());

            if (!create)
                return nullptr;

            auto detail = std::make_shared<T_Details>(this);
            _details[name] = std::static_pointer_cast<MeshDetails>(detail);

            return detail.get();
        }
    }

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE CoreMeshPtr		CMesh() const { return _cmesh; } 
	FORCEINLINE CHashPtr		Hash() const { return _hash; }
};

typedef std::shared_ptr<MeshInfo> MeshInfoPtr;