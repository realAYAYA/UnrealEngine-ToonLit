// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Converters/GLTFConverter.h"
#include "Converters/GLTFMeshSection.h"
#include "Converters/GLTFIndexArray.h"

template <typename MeshLODType>
class TGLTFMeshSectionConverter : public TGLTFConverter<const FGLTFMeshSection*, const MeshLODType*, FGLTFIndexArray>
{
public:

	TGLTFMeshSectionConverter() = default;
	TGLTFMeshSectionConverter(TGLTFMeshSectionConverter&&) = default;
	TGLTFMeshSectionConverter& operator=(TGLTFMeshSectionConverter&&) = default;

	TGLTFMeshSectionConverter(const TGLTFMeshSectionConverter&) = delete;
	TGLTFMeshSectionConverter& operator=(const TGLTFMeshSectionConverter&) = delete;

protected:

	virtual const FGLTFMeshSection* Convert(const MeshLODType* MeshLOD, FGLTFIndexArray SectionIndices) override
	{
		return Outputs.Add_GetRef(MakeUnique<FGLTFMeshSection>(MeshLOD, SectionIndices)).Get();
	}

private:

	TArray<TUniquePtr<FGLTFMeshSection>> Outputs;
};

typedef TGLTFMeshSectionConverter<FStaticMeshLODResources> FGLTFStaticMeshSectionConverter;
typedef TGLTFMeshSectionConverter<FSkeletalMeshLODRenderData> FGLTFSkeletalMeshSectionConverter;
