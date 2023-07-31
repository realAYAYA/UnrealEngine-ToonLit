// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Exporters/GLTFExporter.h"
#include "GLTFMaterialExporter.generated.h"

class UStaticMesh;

UCLASS()
class GLTFEXPORTER_API UGLTFMaterialExporter : public UGLTFExporter
{
public:

	GENERATED_BODY()

	explicit UGLTFMaterialExporter(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	virtual bool AddObject(FGLTFContainerBuilder& Builder, const UObject* Object) override;

private:

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UStaticMesh> DefaultPreviewMesh;
};
