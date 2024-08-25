// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h" 
#include <memory>
#include "Helper/Promise.h"
#include "Helper/DataUtil.h"
#include <array>

class MeshInfo;
class MeshDetails;
typedef cti::continuable<MeshDetails*>	MeshDetailsPAsync;

class RawBuffer;
typedef std::shared_ptr<RawBuffer>		RawBufferPtr;

//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API MeshDetails
{
protected:
	MeshInfo*					Mesh;					/// The mesh that is containing this detail
	bool						bIsFinalised = false;		/// Whether this detail has been finalised or not
	CHashPtr					HashValue;					/// What is the hash of this detail

	virtual void				CalculateTri(size_t ti);
	virtual void				CalculateVertex(size_t vi);

public:
								MeshDetails(MeshInfo* mesh);
	virtual						~MeshDetails();

	virtual MeshDetailsPAsync	Calculate();
	virtual MeshDetailsPAsync	Finalise();
	virtual void				RenderDebug();
	virtual void				Release();
};

typedef std::shared_ptr<MeshDetails> MeshDetailsPtr;
