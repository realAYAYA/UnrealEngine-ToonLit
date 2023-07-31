// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ComponentSourceInterfaces.h"
#include "ConversionUtils/VolumeToDynamicMesh.h"
#include "MeshDescription.h"

class MODELINGCOMPONENTSEDITORONLY_API FVolumeComponentTargetFactory : public FComponentTargetFactory
{
public:
	bool CanBuild(UActorComponent* Candidate) override;
	TUniquePtr<FPrimitiveComponentTarget> Build(UPrimitiveComponent* PrimitiveComponent) override;
};

/** Deprecated. Use the tool target instead. */
class MODELINGCOMPONENTSEDITORONLY_API FVolumeComponentTarget : public FPrimitiveComponentTarget
{
public:

	FVolumeComponentTarget(UPrimitiveComponent* Component);

	FMeshDescription* GetMesh() override;
	void CommitMesh(const FCommitter&) override;

	virtual void GetMaterialSet(FComponentMaterialSet& MaterialSetOut, bool bAssetMaterials) const override;
	virtual void CommitMaterialSetUpdate(const FComponentMaterialSet& MaterialSet, bool bApplyToAsset) override {};

	virtual bool HasSameSourceData(const FPrimitiveComponentTarget& OtherTarget) const override;

	const UE::Conversion::FVolumeToMeshOptions& GetVolumeToMeshOptions() { return VolumeToMeshOptions; }

protected:
	TUniquePtr<FMeshDescription> ConvertedMeshDescription;

	UE::Conversion::FVolumeToMeshOptions VolumeToMeshOptions;
};
