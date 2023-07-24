// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Exporters/GLTFExporter.h"
#include "GLTFSkeletalMeshExporter.generated.h"

UCLASS()
class GLTFEXPORTER_API UGLTFSkeletalMeshExporter : public UGLTFExporter
{
public:

	GENERATED_BODY()

	explicit UGLTFSkeletalMeshExporter(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	virtual bool AddObject(FGLTFContainerBuilder& Builder, const UObject* Object) override;
};
