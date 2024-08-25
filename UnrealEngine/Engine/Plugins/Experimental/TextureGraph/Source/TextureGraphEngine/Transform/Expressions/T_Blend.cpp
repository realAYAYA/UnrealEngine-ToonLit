// Copyright Epic Games, Inc. All Rights Reserved.

#include "T_Blend.h"
#include "Job/JobArgs.h"
#include "TextureGraphEngine.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "2D/TargetTextureSet.h"
#include "Model/Mix/MixInterface.h"
#include "Model/Mix/MixSettings.h"

IMPLEMENT_GLOBAL_SHADER(FSH_BlendNormal		, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendNormal"	, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendAdd		, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendAdd"		, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendSubtract	, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendSubtract"	, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendMultiply	, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendMultiply"	, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendDivide		, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendDivide"	, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendDifference	, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendDifference", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendMax		, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendMax"		, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendMin		, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendMin"		, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendStep		, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendStep"		, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendOverlay	, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendOverlay"	, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_BlendDistort	, "/Plugin/TextureGraph/Expressions/Expression_Blend.usf", "FSH_BlendDistort"	, SF_Pixel);

T_Blend::T_Blend()
{
}

T_Blend::~T_Blend()
{
}

TiledBlobPtr T_Blend::Create(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture,
	TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId, EBlendModes InBlendMode)
{
	switch(InBlendMode)
	{
		case EBlendModes::Normal:
			return CreateNormal(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Add:
			return CreateAdd(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Subtract:
			return CreateSubtract(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Multiply:
			return CreateMultiply(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Divide:
			return CreateDivide(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Difference:
			return CreateDifference(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Max:
			return CreateMax(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Min:
			return CreateMin(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Step:
			return CreateStep(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		case EBlendModes::Overlay:
			return CreateOverlay(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);
		// case EBlendModes::Distort:
		// 	return CreateDistort(InCycle, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask, InOpacity, InTargetId);

	default:
		// Unhandled case
		checkNoEntry();
		return TextureHelper::GetBlack();
	}
}

template <typename FSH_Type>
TiledBlobPtr CreateGenericBlend(MixUpdateCyclePtr InCycle, int32 InTargetId, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture,
	TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, FString InTransformName)
{
	const RenderMaterial_FXPtr RenderMaterial = TextureGraphEngine::GetMaterialManager()->CreateMaterial_FX<VSH_Simple, FSH_Type>(InTransformName);
	check(RenderMaterial);

	if (!InBackgroundTexture)
	{
		InBackgroundTexture = TextureHelper::GetBlack();
	}
	
	if (!InForegroundTexture)
	{
		InForegroundTexture = TextureHelper::GetBlack();
	}
	
	if(!InMask)
	{
		InMask = TextureHelper::GetWhite();
	}  

	JobUPtr JobPtr = std::make_unique<Job>(InCycle->GetMix(), InTargetId, std::static_pointer_cast<BlobTransform>(RenderMaterial));
	
	JobPtr
		->AddArg(ARG_BLOB(InBackgroundTexture, "BackgroundTexture"))
		->AddArg(ARG_BLOB(InForegroundTexture, "ForegroundTexture"))
		->AddArg(ARG_BLOB(InMask, "MaskTexture"))
		->AddArg(ARG_FLOAT(InOpacity, "Opacity"))
		;

	const FString Name = FString::Printf(TEXT("[%llu] - Blend - %s"), InCycle->GetBatch()->GetBatchId(), *InTransformName);

	TiledBlobPtr Result = JobPtr->InitResult(Name, &DesiredDesc);

	InCycle->AddJob(InTargetId, std::move(JobPtr));

	return Result;
}

TiledBlobPtr T_Blend::CreateNormal(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendNormal>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendNormal");
}

TiledBlobPtr T_Blend::CreateAdd(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendAdd>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendAdd");
}

TiledBlobPtr T_Blend::CreateSubtract(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendSubtract>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendSubtract");
}

TiledBlobPtr T_Blend::CreateMultiply(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendMultiply>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForeGroundTexture, InMask,InOpacity, "T_BlendMultiply");
}

TiledBlobPtr T_Blend::CreateDivide(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendDivide>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendDivide");
}

TiledBlobPtr T_Blend::CreateDifference(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendDifference>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendDifference");
}

TiledBlobPtr T_Blend::CreateMax(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendMax>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendMax");
}

TiledBlobPtr T_Blend::CreateMin(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendMin>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendMin");
}

TiledBlobPtr T_Blend::CreateStep(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendStep>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendStep");
}

TiledBlobPtr T_Blend::CreateOverlay(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendOverlay>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendOverlay");
}

TiledBlobPtr T_Blend::CreateDistort(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForegroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId)
{
	return CreateGenericBlend<FSH_BlendDistort>(InCycle, InTargetId, DesiredDesc, InBackgroundTexture, InForegroundTexture, InMask,InOpacity, "T_BlendDistort");
}