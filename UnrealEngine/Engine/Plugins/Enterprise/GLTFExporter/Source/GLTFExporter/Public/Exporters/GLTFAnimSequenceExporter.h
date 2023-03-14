// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Exporters/GLTFExporter.h"
#include "GLTFAnimSequenceExporter.generated.h"

UCLASS()
class GLTFEXPORTER_API UGLTFAnimSequenceExporter : public UGLTFExporter
{
public:

	GENERATED_BODY()

	explicit UGLTFAnimSequenceExporter(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	virtual bool AddObject(FGLTFContainerBuilder& Builder, const UObject* Object) override;
};
