// Copyright Epic Games, Inc. All Rights Reserved.
#include "StaticImageResource.h"

#include "2D/Tex.h"
#include "Job/JobArgs.h"
#include "Mix/MixInterface.h"
#include "Transform/Utility/T_LoadStaticResource.h"
#include "Misc/PackageName.h"

UStaticImageResource::~UStaticImageResource()
{
}

AsyncTiledBlobRef UStaticImageResource::Load(MixUpdateCyclePtr Cycle)
{
	const FString& FileName = AssetUUID;

	// Asset path could be empty in case of a new asset channel source, we fallback to a default flat load
	if (FileName.IsEmpty() || FileName == TEXT("None"))
		return cti::make_ready_continuable(TiledBlobRef(TextureHelper::GBlack));
	
	// if the path is a valid package path (content browser asset)
	check(FPackageName::IsValidPath(FileName));
	Tex* TexObj = nullptr;

	// we load the asset -> UTexture2d -> Tex -> Blob and create a SourceAsset from that
	return PromiseUtil::OnGameThread().then([=](int32) mutable
	{
		FSoftObjectPath SoftPath(FileName);

		TexObj = new Tex();
		TexObj->LoadAsset(SoftPath);

		int32 NumXTiles = Cycle->GetMix()->GetNumXTiles();
		int32 NumYTiles = Cycle->GetMix()->GetNumYTiles();

		return TexObj->ToBlob(NumXTiles, NumYTiles, 0, 0, false);
	})
	.then([=](TiledBlobRef LoadedBlob) mutable
	{
		return (AsyncTiledBlobRef)PromiseUtil::OnGameThread().then([=]() { return LoadedBlob; });
	});
}


TiledBlobPtr UStaticImageResource::GetBlob(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredDesc, int32 TargetId)
{
	if (BlobObj)
		return BlobObj;

	auto JobObj = std::make_unique<Job_LoadStaticImageResource>(Cycle->GetMix(), this, TargetId);

	JobObj->AddArg(ARG_STRING(AssetUUID, "AssetUUID")); /// Assett UUID contains the Name of the asset megascan surface AND the Name of channel

	FString Name = "[StaticImageResource]-" + AssetUUID;
	DesiredDesc.Format = BufferFormat::LateBound;
	BlobObj = JobObj->InitResult(Name, &DesiredDesc);

	Cycle->AddJob(TargetId, std::move(JobObj));

	return BlobObj;
}
