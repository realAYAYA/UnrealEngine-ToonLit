// Copyright Epic Games, Inc. All Rights Reserved.
#include "JobArgs.h"
#include "Job.h"
#include "TextureGraphEngine.h"
#include "Model/Mix/MixSettings.h"
#include "Data/Blob.h"
#include "JobBatch.h"
#include "2D/TextureHelper.h"
#include "Device/FX/Device_FX.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include "FxMat/RenderMaterial_FX.h"
#include "FxMat/RenderMaterial_FX_Combined.h"
#include "FxMat/RenderMaterial_BP.h"
#include "Engine/TextureRenderTarget2DArray.h"
#include "FxMat/RenderMaterial_FX_Combined.h"
#include "3D/RenderMesh.h"
#include "Data/Blobber.h"

DECLARE_CYCLE_STAT(TEXT("JobArg_Combined_Bind"), STAT_JobArg_Combined_Bind, STATGROUP_TextureGraphEngine);
DECLARE_CYCLE_STAT(TEXT("JobArg_Combined_UnBind"), STAT_JobArg_Combined_UnBind, STATGROUP_TextureGraphEngine);
DECLARE_CYCLE_STAT(TEXT("JobArg_Blob_Bind"), STAT_JobArg_Blob_Bind, STATGROUP_TextureGraphEngine);
DECLARE_CYCLE_STAT(TEXT("JobArg_Blob_UnBind"), STAT_JobArg_Blob_UnBind, STATGROUP_TextureGraphEngine);
DECLARE_CYCLE_STAT(TEXT("JobArg_Blob_Hash"), STAT_JobArg_Blob_Hash, STATGROUP_TextureGraphEngine);


JobArg_Blob::JobArg_Blob(TiledBlobPtr BlobObj, const ResourceBindInfo& BindInfo) 
	: JobArg_Resource(BindInfo)
	, BlobObjRef(BlobObj)
{
	check(BlobObjRef.get());

	/// Also check whether it's in the Blobber or not
	//auto CachedBlob = TextureGraphEngine::Blobber()->Find(BlobObj.GetHash()->Value());
	//check(CachedBlob || BlobObj.IsKeepStrong());

#if DEBUG_BLOB_REF_KEEPING == 1
	/// Check whether all the tiles are valid and available
	//for (size_t TileX = 0; TileX < BlobObj->Rows(); TileX++)
	//{
	//	for (size_t TileY = 0; TileY < BlobObj->Cols(); TileY++) 
	//	{
	//		BlobRef Tile = BlobObj->Tile(TileX, TileY);
	//		check(!Tile.expired());
	//	}
	//}

	TextureGraphEngine::Blobber()->AddReferencedBlob((Blob*)BlobObjRef.get().get(), this);
#endif 
}

JobArg_Blob::JobArg_Blob(TiledBlobPtr BlobObj, const char* TargetName) : JobArg_Blob(BlobObj, ResourceBindInfo({ FString(TargetName) })) 
{ 
}

JobArg_Blob::~JobArg_Blob()
{
#if DEBUG_BLOB_REF_KEEPING == 1
	TextureGraphEngine::Blobber()->RemoveReferencedBlob((Blob*)BlobObjRef.get().get(), this);
#endif 
}

bool JobArg::CanHandleTiles() const
{
	return true;
}

bool JobArg::ForceNonTiledTransform() const
{
	return false;
}

void JobArg_Blob::SetHandleTiles(bool bInCanHandleTiles)
{
	bCanHandleTiles = bInCanHandleTiles;
}

bool JobArg_Blob::CanHandleTiles() const
{
	return bCanHandleTiles;
}

void JobArg_Blob::SetForceNonTiledTransform(bool bInForceNonTiledTransform)
{
	bForceNonTiledTransform = bInForceNonTiledTransform;
}

bool JobArg_Blob::ForceNonTiledTransform() const
{
	return bForceNonTiledTransform;
}

JobArg_Blob& JobArg_Blob::WithDownsampled4To1()
{
	bBindDownsampled4To1 = true;
	return (*this);
}

bool JobArg_Blob::IsDownsampled4To1() const
{
	return bBindDownsampled4To1;
}

JobArg_Blob& JobArg_Blob::WithNeighborTiles()
{
	bBindNeighborTiles = true;
	return (*this);
}

bool JobArg_Blob::IsNeighborTiles() const 
{
	return bBindNeighborTiles;
}

//////////////////////////////////////////////////////////////////////////
TiledBlobPtr JobArg_Blob::GetRootBlob(JobArgBindInfo JobBindInfo) const
{
	TiledBlobPtr RootBlob = BlobObjRef.get();

	if (JobBindInfo.LODLevel != 0 &&					/// LOD is enabled in this cycle
		!RootBlob->IsLODLevel() &&						/// Don't wanna LOD an existing LOD BlobObj
		RootBlob->HasLODLevels() &&						/// Check whether it has LOD levels
		RootBlob->HasLODLevel(JobBindInfo.LODLevel))	/// Ensure that it has a valid object in the LOD that we need
	{
		RootBlob = std::static_pointer_cast<TiledBlob>(RootBlob->GetLODLevel(JobBindInfo.LODLevel).lock());
		check(RootBlob);
	}

	return RootBlob;
}

AsyncJobArgResultPtr JobArg_Blob::Bind(JobArgBindInfo JobBindInfo)
{
	check(!bUnbound);

	SCOPE_CYCLE_COUNTER(STAT_JobArg_Blob_Bind)
	const Job* CurrentJob = JobBindInfo.JobObj;
	check(CurrentJob);

	BlobTransformPtr transform = JobBindInfo.Transform;

	ResourceBindInfo BindInfo = ArgBindInfo;
	BindInfo.Dev = JobBindInfo.Dev;
	check(BindInfo.Dev);

	TiledBlobPtr RootBlob = GetRootBlob(JobBindInfo);

	if (IsDownsampled4To1())
	{
		return RootBlob->TransferTo(BindInfo.Dev)
			.then([RootBlob, BindInfo, JobBindInfo](auto)
			{
				// Collect the individual tile textures from the root tiled BlobObj in an array
				std::vector<TexPtr> TileTexObjs;
				for (int TileX = 0; TileX < 2; TileX++)
				{
					for (int TileY = 0; TileY < 2; TileY++)
					{
						if (RootBlob->IsValidTileIndex(TileX + 2 * JobBindInfo.TileX, TileY + 2 * JobBindInfo.TileY))
						{
							auto Tile = RootBlob->GetTile(TileX + 2 * JobBindInfo.TileX, TileY + 2 * JobBindInfo.TileY);
							auto TileBuffer = std::static_pointer_cast<DeviceBuffer_FX>(Tile->GetBufferRef().GetPtr());
							TileTexObjs.push_back(TileBuffer->GetTexture());
						}
						else
						{
							auto Black = TextureHelper::GetBlack()->GetTile(0, 0);
							auto BlackBuffer = std::static_pointer_cast<DeviceBuffer_FX> (Black->GetBufferRef().GetPtr());
							TileTexObjs.push_back(BlackBuffer->GetTexture());
						}
					}
				}

				// True binding of the array of textures to the Material
				const RenderMaterial* Material = static_cast<const RenderMaterial*>(JobBindInfo.Transform.get());
				check(Material);
				Material->SetArrayTexture(*BindInfo.Target, TileTexObjs);

				return cti::make_ready_continuable(std::make_shared<JobArgResult>());
			});
	}
	else if (IsNeighborTiles())
	{
		return RootBlob->TransferTo(BindInfo.Dev)
			.then([RootBlob, BindInfo, JobBindInfo](auto)
			{
				// Collect the individual tile textures from the root tiled BlobObj in an array
				std::vector<TexPtr> Textures;
				auto MainX = JobBindInfo.TileX;
				auto MainY = JobBindInfo.TileY;
				auto GridX = RootBlob->Cols();
				auto GridY = RootBlob->Rows();

				for (int TileX = 0; TileX < 3; TileX++)
				{
					for (int TileY = 0; TileY < 3; TileY++)
					{
						// Compute the true tile index
						auto InnerTileX = (MainX - 1 + TileX);
						auto InnerTileY = (MainY - 1 + TileY);
						if (InnerTileX < 0)
							InnerTileX = GridX - 1;
						if (InnerTileX >= GridX)
							InnerTileX = 0;
						if (InnerTileY < 0)
							InnerTileY = GridY - 1;
						if (InnerTileY >= GridY)
							InnerTileY = 0;

						if (RootBlob->IsValidTileIndex(InnerTileX, InnerTileY))
						{
							auto Tile = RootBlob->GetTile(InnerTileX, InnerTileY);
							auto TileBuffer = std::static_pointer_cast<DeviceBuffer_FX>(Tile->GetBufferRef().GetPtr());
							Textures.push_back(TileBuffer->GetTexture());
						}
						else
						{
							auto Black = TextureHelper::GetBlack()->GetTile(0, 0);
							auto BlackBuffer = std::static_pointer_cast<DeviceBuffer_FX> (Black->GetBufferRef().GetPtr());
							Textures.push_back(BlackBuffer->GetTexture());
						}
					}
				}

				// True binding of the array of textures to the Material
				const RenderMaterial* Material = static_cast<const RenderMaterial*>(JobBindInfo.Transform.get());
				check(Material);
				Material->SetArrayTexture(*BindInfo.Target, Textures);

				return cti::make_ready_continuable(std::make_shared<JobArgResult>());
			});
	}
	else
	{
		BlobPtr BlobToBind = RootBlob;
		check(BlobToBind);

		bool bArgCanHandleTiles = CanHandleTiles();
		if (bArgCanHandleTiles && transform->CanHandleTiles() && JobBindInfo.TileX >= 0 && JobBindInfo.TileY >= 0)
			BlobToBind = BlobObjRef.get()->GetTile(JobBindInfo.TileX, JobBindInfo.TileY).lock();

		FString TransformName = CurrentJob->GetTransform()->GetName();

		//UE_LOG(LogJob, Log, TEXT("[%s] Binding BlobObj arg: %s => %s [Tile: %s]"), *transformName, *BlobObj->Name(), *BindInfo.TargetName, *blobToBind->Name());
	
		BindInfo.bIsCombined = !bArgCanHandleTiles;
		return BlobToBind->Bind(transform.get(), BindInfo)
			.then([this]() mutable
			{
				return std::make_shared<JobArgResult>();
			});
	}
}

AsyncJobArgResultPtr JobArg_Blob::Unbind(JobArgBindInfo JobBindInfo)
{
	check(!bUnbound);

	SCOPE_CYCLE_COUNTER(STAT_JobArg_Blob_UnBind)
	if (BlobObjRef.expired()) // No references remain , this cause may arise if BlobObj is cleared through sharedPtrs before unbind
		return cti::make_ready_continuable(std::make_shared<JobArgResult>());

	if (IsDownsampled4To1())
	{
		// IN this case the arg is only used to read from, nothing to do to unbind
		return cti::make_ready_continuable(std::make_shared<JobArgResult>());
	}
	else if (IsNeighborTiles())
	{
		// IN this case the arg is only used to read from, nothing to do to unbind
		return cti::make_ready_continuable(std::make_shared<JobArgResult>());
	}
	else
	{
		check(JobBindInfo.JobObj);

		BlobTransformPtr transform = JobBindInfo.Transform;

		BlobPtr blobToBind = GetRootBlob(JobBindInfo);

		bool bCanArgHandleTiles = CanHandleTiles();
		if (bCanArgHandleTiles && transform->CanHandleTiles() && JobBindInfo.TileX >= 0 && JobBindInfo.TileY >= 0)
		{
			blobToBind = BlobObjRef.get()->GetTile(JobBindInfo.TileX, JobBindInfo.TileY).lock();
		}

		ArgBindInfo.BatchId = JobBindInfo.Batch->GetBatchId();

		ArgBindInfo.bIsCombined = !bCanArgHandleTiles;
		return blobToBind->Unbind(transform.get(), ArgBindInfo)
			.then([=]() mutable
			{
				return std::make_shared<JobArgResult>();
			});
	}
}

bool JobArg_Blob::IsLateBound(uint32 TileX, uint32 TileY) const
{
	TiledBlobPtr BlobObj = BlobObjRef.get();
	BlobRef TileXY = BlobObj->GetTile(TileX, TileY);

	/// If the tile isn't even there then it's definitely late bound
	if (TileXY.IsNull())
		return true;

	BlobPtr TileXYPtr = TileXY.lock();
	return BlobObj->IsLateBound() || !TileXYPtr || TileXYPtr->IsNull() || TileXYPtr->IsLateBound();
}

const BufferDescriptor* JobArg_Blob::GetDescriptor() const 
{
	return BlobObjRef ? &BlobObjRef->GetDescriptor() : nullptr;
}

CHashPtr JobArg_Blob::TileHash(uint32 TileX, uint32 TileY) const
{
	TiledBlobPtr BlobObj = BlobObjRef.get();
	return BlobObj->GetTile(TileX, TileY)->Hash();
}

CHashPtr JobArg_Blob::Hash() const
{
	SCOPE_CYCLE_COUNTER(STAT_JobArg_Blob_Hash);
	check(BlobObjRef.get());
	return BlobObjRef.GetHash();
}

JobPtrW JobArg_Blob::GeneratingJob() const
{
	return BlobObjRef.get() ? BlobObjRef.get()->Job() : JobPtrW();
}

//////////////////////////////////////////////////////////////////////////

AsyncJobArgResultPtr JobArg_Mesh::Bind(JobArgBindInfo JobBindInfo) 
{
	JobBindInfo.JobObj->SetMesh(Mesh.get());
	return cti::make_ready_continuable(std::make_shared<JobArgResult>());
}

AsyncJobArgResultPtr JobArg_Mesh::Unbind(JobArgBindInfo JobBindInfo) 
{
	return cti::make_ready_continuable(std::make_shared<JobArgResult>());
}

CHashPtr JobArg_Mesh::Hash() const 
{ 
	return Mesh->Hash(); 
}

CHashPtr JobArg_Mesh::TileHash(uint32 TileX, uint32 TileY) const
{
	return Mesh->Hash();
}

//////////////////////////////////////////////////////////////////////////
AsyncJobArgResultPtr JobArg_TileInfo::Bind(JobArgBindInfo JobBindInfo)
{
	const Job* JobObj = JobBindInfo.JobObj;
	check(JobObj);
	BlobTransformPtr Transform = JobBindInfo.Transform;

	TiledBlobPtr Result = JobObj->GetResult();
	float TileWidth = Result->GetWidth() / Result->Cols();
	float tileHeight = Result->GetHeight() / Result->Rows();

	Value.TileX = JobBindInfo.TileX;
	Value.TileCountX = Result->Cols();
	Value.TileWidth = TileWidth;
	
	Value.TileY = JobBindInfo.TileY;
	Value.TileCountY = Result->Rows();
	Value.TileHeight = tileHeight;

	Transform->BindStruct<FTileInfo>(Value, ArgBindInfo);
	
	return cti::make_ready_continuable(std::make_shared<JobArgResult>());
}

AsyncJobArgResultPtr JobArg_TileInfo::Unbind(JobArgBindInfo JobBindInfo)
{
	return cti::make_ready_continuable(std::make_shared<JobArgResult>());
}

CHashPtr JobArg_TileInfo::Hash() const
{
	if (!HashValue)
		HashValue = TileHash(-1, -1);
	return HashValue;
}

CHashPtr JobArg_TileInfo::TileHash(uint32 TileX, uint32 TileY) const
{
	HashTypeVec StructHash =
	{
		MX_HASH_VAL_DEF(TileX),
		MX_HASH_VAL_DEF(Value.TileCountX),
		MX_HASH_VAL_DEF(Value.TileWidth),
		MX_HASH_VAL_DEF(TileY),
		
		MX_HASH_VAL_DEF(Value.TileCountY),
		MX_HASH_VAL_DEF(Value.TileHeight),
		MX_HASH_VAL_DEF(sizeof(Value)),
	};

	return std::make_shared<CHash>(DataUtil::Hash(StructHash), true);
}

//////////////////////////////////////////////////////////////////////////

CHashPtr JobArg_ForceTiling::Hash() const
{
	return TileHash(-1, -1);
}

CHashPtr JobArg_ForceTiling::TileHash(uint32 TileX, uint32 TileY) const
{
	HashTypeVec StructHash =
	{
		MX_HASH_VAL_DEF(TileX),
		MX_HASH_VAL_DEF(TileY),
	};
	
	return std::make_shared<CHash>(DataUtil::Hash(StructHash), true);
}
