// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Exporters/GLTFExporter.h"
#include "GLTFLevelSequenceExporter.generated.h"

UCLASS()
class GLTFEXPORTER_API UGLTFLevelSequenceExporter : public UGLTFExporter
{
public:

	GENERATED_BODY()

	explicit UGLTFLevelSequenceExporter(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	virtual bool AddObject(FGLTFContainerBuilder& Builder, const UObject* Object) override;
};
