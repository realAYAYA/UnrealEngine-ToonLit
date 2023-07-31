// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/GLTFDelayedTask.h"
#include "Builders/GLTFConvertBuilder.h"
#include "Converters/GLTFMeshSectionConverters.h"
#include "Converters/GLTFMaterialArray.h"
#include "Converters/GLTFNameUtility.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

class SkeletalMeshComponent;
class StaticMeshComponent;

class FGLTFDelayedStaticMeshTask : public FGLTFDelayedTask
{
public:

	FGLTFDelayedStaticMeshTask(FGLTFConvertBuilder& Builder, FGLTFStaticMeshSectionConverter& MeshSectionConverter, const UStaticMesh* StaticMesh, const UStaticMeshComponent* StaticMeshComponent, FGLTFMaterialArray Materials, int32 LODIndex, FGLTFJsonMesh* JsonMesh)
		: FGLTFDelayedTask(EGLTFTaskPriority::Mesh)
		, Builder(Builder)
		, MeshSectionConverter(MeshSectionConverter)
		, StaticMesh(StaticMesh)
		, StaticMeshComponent(StaticMeshComponent)
		, Materials(Materials)
		, LODIndex(LODIndex)
		, JsonMesh(JsonMesh)
	{
	}

	virtual FString GetName() override
	{
		return StaticMeshComponent != nullptr ? FGLTFNameUtility::GetName(StaticMeshComponent) : StaticMesh->GetName();
	}

	virtual void Process() override;

private:

	FGLTFConvertBuilder& Builder;
	FGLTFStaticMeshSectionConverter& MeshSectionConverter;
	const UStaticMesh* StaticMesh;
	const UStaticMeshComponent* StaticMeshComponent;
	const FGLTFMaterialArray Materials;
	const int32 LODIndex;
	FGLTFJsonMesh* JsonMesh;
};

class FGLTFDelayedSkeletalMeshTask : public FGLTFDelayedTask
{
public:

	FGLTFDelayedSkeletalMeshTask(FGLTFConvertBuilder& Builder, FGLTFSkeletalMeshSectionConverter& MeshSectionConverter, const USkeletalMesh* SkeletalMesh, const USkeletalMeshComponent* SkeletalMeshComponent, FGLTFMaterialArray Materials, int32 LODIndex, FGLTFJsonMesh* JsonMesh)
		: FGLTFDelayedTask(EGLTFTaskPriority::Mesh)
		, Builder(Builder)
		, MeshSectionConverter(MeshSectionConverter)
		, SkeletalMesh(SkeletalMesh)
		, SkeletalMeshComponent(SkeletalMeshComponent)
		, Materials(Materials)
		, LODIndex(LODIndex)
		, JsonMesh(JsonMesh)
	{
	}

	virtual FString GetName() override
	{
		return SkeletalMeshComponent != nullptr ? FGLTFNameUtility::GetName(SkeletalMeshComponent) : SkeletalMesh->GetName();
	}

	virtual void Process() override;

private:

	FGLTFConvertBuilder& Builder;
	FGLTFSkeletalMeshSectionConverter& MeshSectionConverter;
	const USkeletalMesh* SkeletalMesh;
	const USkeletalMeshComponent* SkeletalMeshComponent;
	const FGLTFMaterialArray Materials;
	const int32 LODIndex;
	FGLTFJsonMesh* JsonMesh;
};
