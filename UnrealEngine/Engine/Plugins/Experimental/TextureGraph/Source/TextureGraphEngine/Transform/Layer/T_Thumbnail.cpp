// Copyright Epic Games, Inc. All Rights Reserved.
#include "T_Thumbnail.h"
#include "Job/JobArgs.h"
#include "Model/Mix/MixInterface.h"
#include "TextureGraphEngine.h"
#include "Device/FX/Device_FX.h"
#include "FxMat/MaterialManager.h"
#include "Job/ThumbnailsService.h"
#include "Job/Scheduler.h"
#include <TextureResource.h>
#include <Engine/Texture2D.h>

////////////////////////////////////////////////////////////////////////// Transform //////////////////////////////////////////////////////////////////////////
JobBatchPtr AddThumbJobToCycle(TiledBlobPtr InBlobToBind, JobUPtr JobPtr, UObject* Model, int32 InTargetId)
{
	ThumbnailsServicePtr SvcThumbnail = TextureGraphEngine::GetScheduler()->GetThumbnailsService().lock();
	if (!SvcThumbnail)
		return nullptr;
	
	UMixInterface* Mix = JobPtr->GetMix();
	check(Mix);
	SvcThumbnail->AddUniqueJobToCycle(Model, Mix, std::move(JobPtr), InTargetId);
	return SvcThumbnail->GetCurrentBatch();
}

TiledBlobRef T_Thumbnail::Bind(UMixInterface* Mix, UObject* Model, TiledBlobPtr InBlobToBind, int32 InTargetId)
{
	if (InBlobToBind->IsTransient())
		return TextureHelper::GetBlack();

	check(InBlobToBind);

	UE_LOG(LogJob, VeryVerbose, TEXT("T_Thumbnail::Bind [%s]"), *InBlobToBind->Name());
	//RenderMaterial_ThumbPtr MatThumb = TextureGraphEngine::GetMaterialManager()->CreateMaterial_Thumbnail(TEXT("T_Thumbnail"), TEXT("Util/CopyUnlit"));
	FString ThmJobName = FString::Printf(TEXT("Thm [%s]"), *InBlobToBind->GetDescriptor().Name);
	RenderMaterial_FXPtr RenderMaterial = TextureGraphEngine::GetMaterialManager()->CreateMaterialOfType_FX<Fx_FullScreenCopy>(ThmJobName);

	JobUPtr JobPtr = std::make_unique<Job>(Mix, InTargetId, std::static_pointer_cast<BlobTransform>(RenderMaterial));
	JobPtr
		->AddArg(ARG_BLOB(InBlobToBind, "SourceTexture"))
		->SetTiled(false)
		;

	BufferDescriptor Desc = InBlobToBind->GetDescriptor();
	Desc.Width = RenderMaterial_Thumbnail::GThumbWidth;
	Desc.Height = RenderMaterial_Thumbnail::GThumbHeight;

	FString Name = FString::Printf(TEXT("T_Thumbnail [%s]"), *InBlobToBind->GetDescriptor().Name);
	
	TiledBlobPtr Result = JobPtr->InitResult(Name, &Desc);
	Result->MakeSingleBlob();

	AddThumbJobToCycle(InBlobToBind, std::move(JobPtr), Model, InTargetId);

	return Result;
}
