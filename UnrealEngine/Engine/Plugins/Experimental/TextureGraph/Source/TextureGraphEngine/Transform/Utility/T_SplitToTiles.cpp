// Copyright Epic Games, Inc. All Rights Reserved.
#include "T_SplitToTiles.h"
#include "TextureGraphEngine.h"
#include "Job/JobArgs.h"
#include "3D/RenderMesh.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "Model/Mix/MixInterface.h"

IMPLEMENT_GLOBAL_SHADER(CSH_SplitToTiles, "/Plugin/TextureGraph/Utils/SplitToTiles_comp.usf", "CSH_SplitToTiles", SF_Compute);

T_SplitToTiles::T_SplitToTiles()
{
}

T_SplitToTiles::~T_SplitToTiles()
{
}

TiledBlobPtr CreateSplitToTilesCompute(MixUpdateCyclePtr Cycle, TiledBlobPtr SourceTex, int32 TargetId)
{
	CSH_SplitToTiles::FPermutationDomain PermutationVector;

	FString Name = FString::Printf(TEXT("[%s].[%d].[%llu] SplitToTiles"), *SourceTex->Name(), TargetId, Cycle->GetBatch()->GetFrameId());

	int32 SrcWidth = SourceTex->GetWidth();
	int32 SrcHeight = SourceTex->GetHeight();

	int32 DstWidth = SrcWidth;
	int32 DstHeight = SrcHeight;

	int32 srcNumCols = (int32)SourceTex->GetTiles().Cols();
	int32 srcNumRows = (int32)SourceTex->GetTiles().Rows();	

	int32 DstNumCols = Cycle->GetMix()->GetNumXTiles();
	int32 DstNumRows = Cycle->GetMix()->GetNumYTiles();

	FIntVector4 SrcDimensions(DstWidth / DstNumCols, DstHeight / DstNumRows, srcNumCols, srcNumRows);
	FTileInfo TileInfo;

	RenderMaterial_FXPtr Transform = TextureGraphEngine::GetMaterialManager()->CreateMaterial_CmpFX<CSH_SplitToTiles>(
		Name, TEXT("Result"), PermutationVector, SrcDimensions.X, SrcDimensions.Y, 1);

	JobUPtr JobObj = std::make_unique<Job>(Cycle->GetMix(), TargetId, std::static_pointer_cast<BlobTransform>(Transform));
	JobObj
		->AddArg(ARG_BLOB(SourceTex, "SourceTexture"))
		->AddArg(ARG_TILEINFO(TileInfo, "TileInfo"));

	BufferDescriptor Desc = SourceTex->GetDescriptor();
	Desc.Name = Name;
	Desc.AllowUAV();

	TiledBlobPtr Result = JobObj->InitResult(Name, &Desc);

	Cycle->AddJob(TargetId, std::move(JobObj));

	check(Desc.Width == SourceTex->GetWidth() && Desc.Height == SourceTex->GetHeight())

	return Result;
}

TiledBlobPtr T_SplitToTiles::Create(MixUpdateCyclePtr Cycle, int32 TargetId, TiledBlobPtr SourceTex)
{
	/// Cannot generate. Just return the source texture
	check(TextureHelper::IsPowerOf2(SourceTex->GetWidth()) && TextureHelper::IsPowerOf2(SourceTex->GetHeight()));
	return CreateSplitToTilesCompute(Cycle, SourceTex, TargetId);
}
