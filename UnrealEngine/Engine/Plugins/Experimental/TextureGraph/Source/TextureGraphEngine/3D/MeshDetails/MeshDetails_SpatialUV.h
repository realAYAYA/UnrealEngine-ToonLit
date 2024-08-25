// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MeshDetails.h"
#include "GraphicsDefs.h"
#include <vector>
#include <list>

class TEXTUREGRAPHENGINE_API MeshDetails_SpatialUV : public MeshDetails
{
protected:
	virtual void				CalculateTri(size_t ti) override;

public:
								MeshDetails_SpatialUV(MeshInfo* mesh);
	virtual						~MeshDetails_SpatialUV();

	virtual MeshDetailsPAsync	Calculate() override;
	virtual void				Release() override;
};
