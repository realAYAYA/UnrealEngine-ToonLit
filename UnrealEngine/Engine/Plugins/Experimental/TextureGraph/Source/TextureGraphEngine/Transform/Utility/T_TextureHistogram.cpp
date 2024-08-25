// Copyright Epic Games, Inc. All Rights Reserved.
#include "T_TextureHistogram.h"
#include "TextureGraphEngine.h"
#include "Job/JobArgs.h"
#include "TextureGraphEngine.h"
#include "Job/HistogramService.h"
#include "Job/JobBatch.h"
#include "Job/Scheduler.h"
#include "Helper/MathUtils.h"
#include "Helper/GraphicsUtil.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Device/DeviceManager.h"
#include "Device/Mem/Device_Mem.h"
#include "Data/Blobber.h"
#include "2D/Tex.h"
#include "FxMat/MaterialManager.h"
#include "T_MinMax.h"

IMPLEMENT_GLOBAL_SHADER(CSH_Histogram, "/Plugin/TextureGraph/Utils/Histogram_comp.usf", "CSH_Histogram", SF_Compute);

static constexpr int NumBins = 256;

T_TextureHistogram::T_TextureHistogram()
{
}

T_TextureHistogram::~T_TextureHistogram()
{
}

RenderMaterial_FXPtr T_TextureHistogram::CreateMaterial_Histogram(FString Name, FString OutputId, const CSH_Histogram::FPermutationDomain& cmpshPermutationDomain,
	int NumThreadsX, int NumThreadsY, int NumThreadsZ)
{
	std::shared_ptr<FxMaterial_Histogram<CSH_Histogram>> Mat = std::make_shared<FxMaterial_Histogram<CSH_Histogram>>(OutputId, &cmpshPermutationDomain, NumThreadsX, NumThreadsY, NumThreadsZ);
	return std::make_shared<RenderMaterial_FX>(Name, std::static_pointer_cast<FxMaterial>(Mat));
}


TiledBlobPtr T_TextureHistogram::CreateJobAndResult(JobUPtr& OutJob, MixUpdateCyclePtr Cycle, TiledBlobPtr SourceTex, int32 TargetId)
{
	check(!SourceTex->HasHistogram())

	CSH_Histogram::FPermutationDomain PermutationVector;

	FIntVector4 SrcDimensions(SourceTex->GetWidth(), SourceTex->GetHeight(), 1, 1);

	FString Name = FString::Printf(TEXT("[%s].[%d] Histogram"), *SourceTex->DisplayName(), TargetId);
	RenderMaterial_FXPtr Transform = T_TextureHistogram::CreateMaterial_Histogram(TEXT("T_Histogram"), TEXT("Result"), PermutationVector, SrcDimensions.X, SrcDimensions.Y, 1);

	OutJob = std::make_unique<Job>(Cycle->GetMix(), TargetId, std::static_pointer_cast<BlobTransform>(Transform));
	OutJob->AddArg(ARG_BLOB(SourceTex, "SourceTiles"));

	BufferDescriptor Desc;
	Desc.Width = NumBins;
	Desc.Height = 2;
	Desc.Format = BufferFormat::Float;// BufferFormat::Int;
	Desc.ItemsPerPoint = 4;
	Desc.Name = FString::Printf(TEXT("Histogram - %s"), *SourceTex->Name());
	Desc.AllowUAV();

	OutJob->SetTiled(false);

	TiledBlobPtr Result = OutJob->InitResult(Name, &Desc, 1, 1);

	Result->MakeSingleBlob();

	if (!SourceTex->HasHistogram())
	{
		//setting it as the histogram of source so it is retained untill the life cycle of source blob.
		SourceTex->SetHistogram(Result);
	}
	return Result;
}

TiledBlobPtr T_TextureHistogram::Create(MixUpdateCyclePtr Cycle, TiledBlobPtr SourceTex, int32 TargetId)
{
	if (SourceTex->HasHistogram())
	{
		TiledBlobPtr SourceHistogram = std::static_pointer_cast<TiledBlob>(SourceTex->GetHistogram());
		return SourceHistogram;
	}

	JobUPtr JobObj;
	TiledBlobPtr Result = CreateJobAndResult(JobObj, Cycle, SourceTex, TargetId);

	Cycle->AddJob(TargetId, std::move(JobObj));

	return Result;
}

TiledBlobPtr T_TextureHistogram::CreateOnService(UMixInterface* InMix, TiledBlobPtr SourceTex, int32 TargetId)
{
	if (SourceTex->HasHistogram())
	{
		TiledBlobPtr SourceHistogram = std::static_pointer_cast<TiledBlob>(SourceTex->GetHistogram());
		return SourceHistogram;
	}

	check(InMix);
	check(SourceTex);

	/// If the source texture turns out to be transient at this point, we just return a black histogram. 
	/// We don't want to calculate anything for transient buffers
	if (SourceTex->IsTransient())
		return TextureHelper::GetBlack();

	HistogramServicePtr Service = TextureGraphEngine::GetScheduler()->GetHistogramService().lock();
	check(Service);

	JobBatchPtr Batch = Service->GetOrCreateNewBatch(InMix);

	JobUPtr JobObj;
	TiledBlobPtr Result = CreateJobAndResult(JobObj, Batch->GetCycle(), SourceTex, TargetId);

	//Add the job using histogram idle service
	//TODO: get rid of mix here and use Null Mix instead
	Service->AddHistogramJob(Batch->GetCycle(), std::move(JobObj), TargetId, InMix);

	return Result;
}
