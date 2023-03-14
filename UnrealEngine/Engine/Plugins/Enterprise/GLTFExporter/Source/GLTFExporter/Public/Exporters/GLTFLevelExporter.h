// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Exporters/GLTFExporter.h"
#include "GLTFLevelExporter.generated.h"

UCLASS()
class GLTFEXPORTER_API UGLTFLevelExporter : public UGLTFExporter
{
public:

	GENERATED_BODY()

	explicit UGLTFLevelExporter(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	virtual bool AddObject(FGLTFContainerBuilder& Builder, const UObject* Object) override;
};
