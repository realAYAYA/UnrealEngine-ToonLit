// Copyright Epic Games, Inc. All Rights Reserved.
#include "T_CombineTiledBlob.h"
#include "3D/RenderMesh.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include "Device/FX/Device_FX.h"
#include "Model/Mix/MixInterface.h"

TiledBlobPtr T_CombineTiledBlob::Create(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr SourceTex)
{
	if (SourceTex && SourceTex->IsTiled())
	{
		FString Name = FString::Printf(TEXT("[%s].[%d].[%llu] CombineTiledBlob"), *SourceTex->Name(), TargetId, Cycle->GetBatch()->GetFrameId());

		std::shared_ptr<CombineTiledBlob_Transform> CombinedTiledBlobMat = std::make_shared< CombineTiledBlob_Transform>(Name, SourceTex);

		JobUPtr JobObj = std::make_unique<Job>(Cycle->GetMix(), TargetId, std::static_pointer_cast<BlobTransform>(CombinedTiledBlobMat));
		JobObj->AddArg(WithUnbounded(ARG_BLOB(SourceTex, "Source")));

		// Express the dependency of the new job on the job delivering the SourceTex
		auto PrevJob = SourceTex->Job();
		if (!PrevJob.expired())
			JobObj->AddPrev(std::static_pointer_cast<DeviceNativeTask>(PrevJob.lock()));

		TiledBlobPtr Result = JobObj->InitResult(Name, &DesiredOutputDesc, 1, 1);

		Cycle->AddJob(TargetId, std::move(JobObj));
		Result->MakeSingleBlob();

		return Result;
	}

	return SourceTex;
}


//////////////////////////////////////////////////////////////////////////
CombineTiledBlob_Transform::CombineTiledBlob_Transform(FString InName, TiledBlobPtr InSource)
	: BlobTransform(InName)
	, Source(InSource)
{
}

Device* CombineTiledBlob_Transform::TargetDevice(size_t DevIndex) const
{
	return Device_FX::Get();
}


bool CombineTiledBlob_Transform::GeneratesData() const 
{
	return true;
}
bool CombineTiledBlob_Transform::CanHandleTiles() const
{ 
	return false;
}

AsyncTransformResultPtr	CombineTiledBlob_Transform::Exec(const TransformArgs& Args)
{
	BlobPtr Target = Args.Target.lock();
	check(Target);

	DeviceBuffer_FX* DevBuffer = dynamic_cast<DeviceBuffer_FX*>(Target->GetBufferRef().get());
	check(DevBuffer);

	T_Tiles<DeviceBufferRef> Tiles(Source->Rows(), Source->Cols());

	for (int32 TileX = 0; TileX < Source->Rows(); TileX++)
	{
		for (int32 TileY = 0; TileY < Source->Cols(); TileY++)
		{
			check(Source->GetTiles()[TileX][TileY]);

			BlobPtr Tile = Source->GetTiles()[TileX][TileY];
			check(Tile->GetBufferRef());

			Tiles[TileX][TileY] = Tile->GetBufferRef();
		}
	}

	Device_FX* FXDevice = Device_FX::Get();

	return FXDevice->DrawTilesToBuffer_Deferred(Target->GetBufferRef(), Tiles).then([this, Target](DeviceBufferRef)
	{
			TransformResultPtr Result = std::make_shared<TransformResult>();
			Result->Target = Target;
			return Result;
	});
}

std::shared_ptr<BlobTransform> CombineTiledBlob_Transform::DuplicateInstance(FString NewName)
{
	return std::make_shared<CombineTiledBlob_Transform>(NewName, Source);
}

AsyncBufferResultPtr CombineTiledBlob_Transform::Bind(BlobPtr BlobObj, const ResourceBindInfo& BindInfo)
{
	return cti::make_ready_continuable<BufferResultPtr>(std::make_shared<BufferResult>());
}

AsyncBufferResultPtr CombineTiledBlob_Transform::Unbind(BlobPtr BlobObj, const ResourceBindInfo& BindInfo)
{
	return cti::make_ready_continuable<BufferResultPtr>(std::make_shared<BufferResult>());
}
