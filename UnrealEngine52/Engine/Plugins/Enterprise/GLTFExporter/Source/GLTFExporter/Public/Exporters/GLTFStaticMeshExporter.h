// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Exporters/GLTFExporter.h"
#include "GLTFStaticMeshExporter.generated.h"

UCLASS()
class GLTFEXPORTER_API UGLTFStaticMeshExporter : public UGLTFExporter
{
public:

	GENERATED_BODY()

	explicit UGLTFStaticMeshExporter(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	virtual bool AddObject(FGLTFContainerBuilder& Builder, const UObject* Object) override;
};
