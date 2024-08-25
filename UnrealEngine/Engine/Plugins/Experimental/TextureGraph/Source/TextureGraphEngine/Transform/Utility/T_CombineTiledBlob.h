// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ShaderCompilerCore.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>


/**
 * A dedicated BlobTransform creating a Combined version of a TiledBlob
 */
class TEXTUREGRAPHENGINE_API CombineTiledBlob_Transform : public BlobTransform
{
private:
	TiledBlobPtr					Source;

public:
	CombineTiledBlob_Transform(FString InName, TiledBlobPtr InSource);
	virtual AsyncBufferResultPtr	Bind(BlobPtr Value, const ResourceBindInfo& BindInfo) override;
	virtual AsyncBufferResultPtr	Unbind(BlobPtr BlobObj, const ResourceBindInfo& BindInfo) override;
	virtual Device*					TargetDevice(size_t DevIndex) const override;
	virtual std::shared_ptr<BlobTransform> DuplicateInstance(FString NewName) override;
	virtual bool					GeneratesData() const override;
	virtual bool					CanHandleTiles() const override;

	virtual AsyncTransformResultPtr	Exec(const TransformArgs& Args) override;
};

/**
 * Tiles To Combined Transform
 */
class TEXTUREGRAPHENGINE_API T_CombineTiledBlob
{
public:

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	
	static TiledBlobPtr				Create(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr SourceTex);
};
