// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorObject.h"
#include "LandscapeDataAccess.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorPrivate.h"
#include "LandscapeRender.h"
#include "LandscapeSettings.h"
#include "LandscapeImportHelper.h"
#include "LandscapeMaterialInstanceConstant.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineUtils.h"
#include "Misc/MessageDialog.h"

//#define LOCTEXT_NAMESPACE "LandscapeEditor"

ULandscapeEditorObject::ULandscapeEditorObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)

	// Tool Settings:
	, ToolStrength(0.3f)
    , PaintToolStrength(0.3f)
	, bUseWeightTargetValue(false)
	, WeightTargetValue(1.0f)
	, MaximumValueRadius(10000.0f)
	, bCombinedLayersOperation(true)

	, FlattenMode(ELandscapeToolFlattenMode::Both)
	, bUseSlopeFlatten(false)
	, bPickValuePerApply(false)
	, bUseFlattenTarget(false)
	, FlattenTarget(0)
	, bShowFlattenTargetPreview(true)
	
	, TerraceInterval(1.0f)
	, TerraceSmooth(0.0001f)

	, RampWidth(2000)
	, RampSideFalloff(0.4f)

	, SmoothFilterKernelSize(4)
	, bDetailSmooth(false)
	, DetailScale(0.3f)

	, ErodeThresh(64)
	, ErodeSurfaceThickness(256)
	, ErodeIterationNum(28)
	, ErosionNoiseMode(ELandscapeToolErosionMode::Lower)
	, ErosionNoiseScale(60.0f)

	, RainAmount(128)
	, SedimentCapacity(0.3f)
	, HErodeIterationNum(75)
	, RainDistMode(ELandscapeToolHydroErosionMode::Both)
	, RainDistScale(60.0f)
	, bHErosionDetailSmooth(true)
	, HErosionDetailScale(0.01f)

	, NoiseMode(ELandscapeToolNoiseMode::Both)
	, NoiseScale(128.0f)

	, bUseSelectedRegion(true)
	, bUseNegativeMask(true)

	, PasteMode(ELandscapeToolPasteMode::Both)
	, bApplyToAllTargets(true)
	, bSnapGizmo(false)
	, bSmoothGizmoBrush(true)

	, MirrorPoint(FVector::ZeroVector)
	, MirrorOp(ELandscapeMirrorOperation::MinusXToPlusX)

	, ResizeLandscape_QuadsPerSection(0)
	, ResizeLandscape_SectionsPerComponent(0)
	, ResizeLandscape_ComponentCount(0, 0)
	, ResizeLandscape_ConvertMode(ELandscapeConvertMode::Expand)

	, NewLandscape_Material(nullptr)
	, NewLandscape_QuadsPerSection(63)
	, NewLandscape_SectionsPerComponent(1)
	, NewLandscape_ComponentCount(8, 8)
	, NewLandscape_Location(0, 0, 100)
	, NewLandscape_Rotation(0, 0, 0)
	, NewLandscape_Scale(100, 100, 100)
	, ImportLandscape_Width(0)
	, ImportLandscape_Height(0)
	, ImportLandscape_AlphamapType(ELandscapeImportAlphamapType::Additive)

	// Brush Settings:
	, BrushRadius(2048.0f)
    , PaintBrushRadius(2048.0f) 
	, BrushFalloff(0.5f)
	, PaintBrushFalloff(0.5f)
	, bUseClayBrush(false)

	, AlphaBrushScale(0.5f)
	, bAlphaBrushAutoRotate(true)
	, AlphaBrushRotation(0.0f)
	, AlphaBrushPanU(0.5f)
	, AlphaBrushPanV(0.5f)
	, bUseWorldSpacePatternBrush(false)
	, WorldSpacePatternBrushSettings(FVector2D::ZeroVector, 0.0f, false, 3200)
	, AlphaTexture(nullptr)
	, AlphaTextureChannel(EColorChannel::Red)
	, AlphaTextureSizeX(1)
	, AlphaTextureSizeY(1)

	, BrushComponentSize(1)
	, TargetDisplayOrder(ELandscapeLayerDisplayMode::Default)
	, ShowUnusedLayers(true)
	, CurrentLayerIndex(INDEX_NONE)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> AlphaTexture;

		FConstructorStatics()
			: AlphaTexture(TEXT("/Engine/EditorLandscapeResources/DefaultAlphaTexture"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	if (!IsTemplate())
	{
		SetAlphaTexture(ConstructorStatics.AlphaTexture.Object, AlphaTextureChannel);
	}
}

void ULandscapeEditorObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SetbUseSelectedRegion(bUseSelectedRegion);
	SetbUseNegativeMask(bUseNegativeMask);
	SetPasteMode(PasteMode);
	SetbSnapGizmo(bSnapGizmo);

	if (PropertyChangedEvent.MemberProperty == nullptr ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, AlphaTexture) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, AlphaTextureChannel))
	{
		SetAlphaTexture(AlphaTexture, AlphaTextureChannel);
	}


	if (PropertyChangedEvent.MemberProperty == nullptr ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_QuadsPerSection) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_SectionsPerComponent) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_ComponentCount))
	{
		NewLandscape_ClampSize();
	}

	if (PropertyChangedEvent.MemberProperty == nullptr ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ResizeLandscape_QuadsPerSection) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ResizeLandscape_SectionsPerComponent) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ResizeLandscape_ConvertMode))
	{
		UpdateComponentCount();
	}

	if (PropertyChangedEvent.MemberProperty == nullptr ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Material) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapFilename) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_Layers))
	{
		// In Import/Export tool we need to refresh from the existing material
		const bool bRefreshFromTarget = ParentMode && ParentMode->CurrentTool && ParentMode->CurrentTool->GetToolName() == FName(TEXT("ImportExport"));
		RefreshImportLayersList(bRefreshFromTarget);
	}

	if (PropertyChangedEvent.MemberProperty == nullptr ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, PaintingRestriction))
	{
		UpdateComponentLayerAllowList();
	}

	if (PropertyChangedEvent.MemberProperty == nullptr ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, TargetDisplayOrder))
	{
		UpdateTargetLayerDisplayOrder();
	}

	if (PropertyChangedEvent.MemberProperty == nullptr ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ShowUnusedLayers))
	{
		UpdateShowUnusedLayers();
	}
}

/** Load UI settings from ini file */
void ULandscapeEditorObject::Load()
{
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("ToolStrength"), ToolStrength, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("PaintToolStrength"), PaintToolStrength, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("WeightTargetValue"), WeightTargetValue, GEditorPerProjectIni);
	bool InbUseWeightTargetValue = bUseWeightTargetValue;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseWeightTargetValue"), InbUseWeightTargetValue, GEditorPerProjectIni);
	bUseWeightTargetValue = InbUseWeightTargetValue;

	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("BrushRadius"), BrushRadius, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("PaintBrushRadius"), PaintBrushRadius, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("BrushComponentSize"), BrushComponentSize, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("BrushFalloff"), BrushFalloff, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("PaintBrushFalloff"), PaintBrushFalloff, GEditorPerProjectIni);
	bool InbUseClayBrush = bUseClayBrush;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseClayBrush"), InbUseClayBrush, GEditorPerProjectIni);
	bUseClayBrush = InbUseClayBrush;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushScale"), AlphaBrushScale, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("AlphaBrushAutoRotate"), bAlphaBrushAutoRotate, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushRotation"), AlphaBrushRotation, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushPanU"), AlphaBrushPanU, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushPanV"), AlphaBrushPanV, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseWorldSpacePatternBrush"), bUseWorldSpacePatternBrush, GEditorPerProjectIni);
	GConfig->GetVector2D(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.Origin"), WorldSpacePatternBrushSettings.Origin, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.bCenterTextureOnOrigin"), WorldSpacePatternBrushSettings.bCenterTextureOnOrigin, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.RepeatSize"), WorldSpacePatternBrushSettings.RepeatSize, GEditorPerProjectIni);
	FString AlphaTextureName = (AlphaTexture != nullptr) ? AlphaTexture->GetPathName() : FString();
	int32 InAlphaTextureChannel = AlphaTextureChannel;
	GConfig->GetString(TEXT("LandscapeEdit"), TEXT("AlphaTextureName"), AlphaTextureName, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("AlphaTextureChannel"), InAlphaTextureChannel, GEditorPerProjectIni);
	AlphaTextureChannel = (EColorChannel::Type)InAlphaTextureChannel;
	SetAlphaTexture(LoadObject<UTexture2D>(nullptr, *AlphaTextureName, nullptr, LOAD_NoWarn), AlphaTextureChannel);

	int32 InFlattenMode = (int32)ELandscapeToolFlattenMode::Both;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("FlattenMode"), InFlattenMode, GEditorPerProjectIni);
	FlattenMode = (ELandscapeToolFlattenMode)InFlattenMode;

	bool InbUseSlopeFlatten = bUseSlopeFlatten;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseSlopeFlatten"), InbUseSlopeFlatten, GEditorPerProjectIni);
	bUseSlopeFlatten = InbUseSlopeFlatten;

	bool InbPickValuePerApply = bPickValuePerApply;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bPickValuePerApply"), InbPickValuePerApply, GEditorPerProjectIni);
	bPickValuePerApply = InbPickValuePerApply;

	bool InbUseFlattenTarget = bUseFlattenTarget;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseFlattenTarget"), InbUseFlattenTarget, GEditorPerProjectIni);
	bUseFlattenTarget = InbUseFlattenTarget;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("FlattenTarget"), FlattenTarget, GEditorPerProjectIni);

	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("TerraceSmooth"), TerraceSmooth, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("TerraceInterval"), TerraceInterval, GEditorPerProjectIni);

	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("RampWidth"), RampWidth, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("RampSideFalloff"), RampSideFalloff, GEditorPerProjectIni);

	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bCombinedLayersOperation"), bCombinedLayersOperation, GEditorPerProjectIni);

	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ErodeThresh"), ErodeThresh, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ErodeIterationNum"), ErodeIterationNum, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ErodeSurfaceThickness"), ErodeSurfaceThickness, GEditorPerProjectIni);
	int32 InErosionNoiseMode = (int32)ErosionNoiseMode;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ErosionNoiseMode"), InErosionNoiseMode, GEditorPerProjectIni);
	ErosionNoiseMode = (ELandscapeToolErosionMode)InErosionNoiseMode;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("ErosionNoiseScale"), ErosionNoiseScale, GEditorPerProjectIni);

	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("RainAmount"), RainAmount, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("SedimentCapacity"), SedimentCapacity, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("HErodeIterationNum"), HErodeIterationNum, GEditorPerProjectIni);
	int32 InRainDistMode = (int32)RainDistMode;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("RainDistNoiseMode"), InRainDistMode, GEditorPerProjectIni);
	RainDistMode = (ELandscapeToolHydroErosionMode)InRainDistMode;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("RainDistScale"), RainDistScale, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("HErosionDetailScale"), HErosionDetailScale, GEditorPerProjectIni);
	bool InbHErosionDetailSmooth = bHErosionDetailSmooth;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bHErosionDetailSmooth"), InbHErosionDetailSmooth, GEditorPerProjectIni);
	bHErosionDetailSmooth = InbHErosionDetailSmooth;

	int32 InNoiseMode = (int32)NoiseMode;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("NoiseMode"), InNoiseMode, GEditorPerProjectIni);
	NoiseMode = (ELandscapeToolNoiseMode)InNoiseMode;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("NoiseScale"), NoiseScale, GEditorPerProjectIni);

	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("SmoothFilterKernelSize"), SmoothFilterKernelSize, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("DetailScale"), DetailScale, GEditorPerProjectIni);
	bool InbDetailSmooth = bDetailSmooth;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bDetailSmooth"), InbDetailSmooth, GEditorPerProjectIni);
	bDetailSmooth = InbDetailSmooth;

	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("MaximumValueRadius"), MaximumValueRadius, GEditorPerProjectIni);

	bool InbSmoothGizmoBrush = bSmoothGizmoBrush;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bSmoothGizmoBrush"), InbSmoothGizmoBrush, GEditorPerProjectIni);
	bSmoothGizmoBrush = InbSmoothGizmoBrush;

	int32 InPasteMode = (int32)ELandscapeToolPasteMode::Both;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("PasteMode"), InPasteMode, GEditorPerProjectIni);
	//PasteMode = (ELandscapeToolPasteMode)InPasteMode;
	SetPasteMode((ELandscapeToolPasteMode)InPasteMode);

	int32 InMirrorOp = (int32)ELandscapeMirrorOperation::MinusXToPlusX;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("MirrorOp"), InMirrorOp, GEditorPerProjectIni);
	MirrorOp = (ELandscapeMirrorOperation)InMirrorOp;

	int32 InConvertMode = (int32)ResizeLandscape_ConvertMode;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ConvertMode"), InConvertMode, GEditorPerProjectIni);
	ResizeLandscape_ConvertMode = (ELandscapeConvertMode)InConvertMode;

	// Region
	//GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseSelectedRegion"), bUseSelectedRegion, GEditorPerProjectIni);
	//GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseNegativeMask"), bUseNegativeMask, GEditorPerProjectIni);
	bool InbApplyToAllTargets = bApplyToAllTargets;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bApplyToAllTargets"), InbApplyToAllTargets, GEditorPerProjectIni);
	bApplyToAllTargets = InbApplyToAllTargets;

	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("ShowUnusedLayers"), ShowUnusedLayers, GEditorPerProjectIni);

	// Set EditRenderMode
	SetbUseSelectedRegion(bUseSelectedRegion);
	SetbUseNegativeMask(bUseNegativeMask);

	// Gizmo History (not saved!)
	GizmoHistories.Empty();
	for (TActorIterator<ALandscapeGizmoActor> It(ParentMode->GetWorld()); It; ++It)
	{
		ALandscapeGizmoActor* Gizmo = *It;
		if (!Gizmo->IsEditable())
		{
			new(GizmoHistories) FGizmoHistory(Gizmo);
		}
	}

	FString NewLandscapeMaterialName;

	// If NewLandscape_Material is not null, we will try to use it
	if (!NewLandscape_Material.IsExplicitlyNull())
	{
		NewLandscapeMaterialName = NewLandscape_Material->GetPathName();
	}
	else
	{
		// If this project already has a saved NewLandscapeMaterialName, we use it
		GConfig->GetString(TEXT("LandscapeEdit"), TEXT("NewLandscapeMaterialName"), NewLandscapeMaterialName, GEditorPerProjectIni);

		if (NewLandscapeMaterialName.IsEmpty())
		{
			/* Project does not have a saved NewLandscapeMaterialNameand and NewLandscape_Material is not already assigned;
			 * we fallback to the DefaultLandscapeMaterial for the project, if set */
			const ULandscapeSettings* Settings = GetDefault<ULandscapeSettings>();
			TSoftObjectPtr<UMaterialInterface> DefaultMaterial = Settings->GetDefaultLandscapeMaterial();

			if (!DefaultMaterial.IsNull())
			{
				NewLandscapeMaterialName = DefaultMaterial.ToString();
			}
		}
	}
	
	if (!NewLandscapeMaterialName.IsEmpty())
	{
		NewLandscape_Material = LoadObject<UMaterialInterface>(nullptr, *NewLandscapeMaterialName, nullptr, LOAD_NoWarn);
	}
	
	int32 AlphamapType = (uint8)ImportLandscape_AlphamapType;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ImportLandscape_AlphamapType"), AlphamapType, GEditorPerProjectIni);
	ImportLandscape_AlphamapType = (ELandscapeImportAlphamapType)AlphamapType;

	RefreshImportLayersList();
}

/** Save UI settings to ini file */
void ULandscapeEditorObject::Save()
{
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("ToolStrength"), ToolStrength, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("PaintToolStrength"), PaintToolStrength, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("WeightTargetValue"), WeightTargetValue, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseWeightTargetValue"), bUseWeightTargetValue, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("BrushRadius"), BrushRadius, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("PaintBrushRadius"), PaintBrushRadius, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("BrushComponentSize"), BrushComponentSize, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("BrushFalloff"), BrushFalloff, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("PaintBrushFalloff"), PaintBrushFalloff, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseClayBrush"), bUseClayBrush, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushScale"), AlphaBrushScale, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("AlphaBrushAutoRotate"), bAlphaBrushAutoRotate, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushRotation"), AlphaBrushRotation, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushPanU"), AlphaBrushPanU, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushPanV"), AlphaBrushPanV, GEditorPerProjectIni);
	GConfig->SetVector2D(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.Origin"), WorldSpacePatternBrushSettings.Origin, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.bCenterTextureOnOrigin"), WorldSpacePatternBrushSettings.bCenterTextureOnOrigin, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.RepeatSize"), WorldSpacePatternBrushSettings.RepeatSize, GEditorPerProjectIni);
	const FString AlphaTextureName = (AlphaTexture != nullptr) ? AlphaTexture->GetPathName() : FString();
	GConfig->SetString(TEXT("LandscapeEdit"), TEXT("AlphaTextureName"), *AlphaTextureName, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("AlphaTextureChannel"), (int32)AlphaTextureChannel, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("FlattenMode"), (int32)FlattenMode, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseSlopeFlatten"), bUseSlopeFlatten, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bPickValuePerApply"), bPickValuePerApply, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseFlattenTarget"), bUseFlattenTarget, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("FlattenTarget"), FlattenTarget, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("TerraceSmooth"), TerraceSmooth, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("TerraceInterval"), TerraceInterval, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("RampWidth"), RampWidth, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("RampSideFalloff"), RampSideFalloff, GEditorPerProjectIni);

	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bCombinedLayersOperation"), bCombinedLayersOperation, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ErodeThresh"), ErodeThresh, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ErodeIterationNum"), ErodeIterationNum, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ErodeSurfaceThickness"), ErodeSurfaceThickness, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ErosionNoiseMode"), (int32)ErosionNoiseMode, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("ErosionNoiseScale"), ErosionNoiseScale, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("RainAmount"), RainAmount, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("SedimentCapacity"), SedimentCapacity, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("HErodeIterationNum"), ErodeIterationNum, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("RainDistMode"), (int32)RainDistMode, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("RainDistScale"), RainDistScale, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("HErosionDetailScale"), HErosionDetailScale, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bHErosionDetailSmooth"), bHErosionDetailSmooth, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("NoiseMode"), (int32)NoiseMode, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("NoiseScale"), NoiseScale, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("SmoothFilterKernelSize"), SmoothFilterKernelSize, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("DetailScale"), DetailScale, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bDetailSmooth"), bDetailSmooth, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("MaximumValueRadius"), MaximumValueRadius, GEditorPerProjectIni);

	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bSmoothGizmoBrush"), bSmoothGizmoBrush, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("PasteMode"), (int32)PasteMode, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("MirrorOp"), (int32)MirrorOp, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ConvertMode"), (int32)ResizeLandscape_ConvertMode, GEditorPerProjectIni);
	//GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseSelectedRegion"), bUseSelectedRegion, GEditorPerProjectIni);
	//GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseNegativeMask"), bUseNegativeMask, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bApplyToAllTargets"), bApplyToAllTargets, GEditorPerProjectIni);

	const FString NewLandscapeMaterialName = (NewLandscape_Material != nullptr) ? NewLandscape_Material->GetPathName() : FString();
	GConfig->SetString(TEXT("LandscapeEdit"), TEXT("NewLandscapeMaterialName"), *NewLandscapeMaterialName, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ImportLandscape_AlphamapType"), (uint8)ImportLandscape_AlphamapType, GEditorPerProjectIni);

	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("ShowUnusedLayers"), ShowUnusedLayers, GEditorPerProjectIni);
}

// Region
void ULandscapeEditorObject::SetbUseSelectedRegion(bool InbUseSelectedRegion)
{ 
	bUseSelectedRegion = InbUseSelectedRegion;
	if (bUseSelectedRegion)
	{
		GLandscapeEditRenderMode |= ELandscapeEditRenderMode::Mask;
	}
	else
	{
		GLandscapeEditRenderMode &= ~(ELandscapeEditRenderMode::Mask);
	}
}
void ULandscapeEditorObject::SetbUseNegativeMask(bool InbUseNegativeMask) 
{ 
	bUseNegativeMask = InbUseNegativeMask; 
	if (bUseNegativeMask)
	{
		GLandscapeEditRenderMode |= ELandscapeEditRenderMode::InvertedMask;
	}
	else
	{
		GLandscapeEditRenderMode &= ~(ELandscapeEditRenderMode::InvertedMask);
	}
}

void ULandscapeEditorObject::SetPasteMode(ELandscapeToolPasteMode InPasteMode)
{
	PasteMode = InPasteMode;
}

void ULandscapeEditorObject::SetbSnapGizmo(bool InbSnapGizmo)
{
	bSnapGizmo = InbSnapGizmo;

	if (ParentMode->CurrentGizmoActor.IsValid())
	{
		ParentMode->CurrentGizmoActor->bSnapToLandscapeGrid = bSnapGizmo;
	}

	if (bSnapGizmo)
	{
		if (ParentMode->CurrentGizmoActor.IsValid())
		{
			check(ParentMode->CurrentGizmoActor->TargetLandscapeInfo);

			const FVector WidgetLocation = ParentMode->CurrentGizmoActor->GetActorLocation();
			const FRotator WidgetRotation = ParentMode->CurrentGizmoActor->GetActorRotation();

			const FVector SnappedWidgetLocation = ParentMode->CurrentGizmoActor->SnapToLandscapeGrid(WidgetLocation);
			const FRotator SnappedWidgetRotation = ParentMode->CurrentGizmoActor->SnapToLandscapeGrid(WidgetRotation);

			ParentMode->CurrentGizmoActor->SetActorLocation(SnappedWidgetLocation, false);
			ParentMode->CurrentGizmoActor->SetActorRotation(SnappedWidgetRotation);
		}
	}
}

void ULandscapeEditorObject::SetAlphaTexture(UTexture2D* InTexture, EColorChannel::Type InTextureChannel)
{
	TArray64<uint8> NewTextureData;
	UTexture2D* NewAlphaTexture = nullptr;
	int32 NumChannels = 0;

	// Validate that the input texture is valid, if not, we'll display an error message and use the default brush alpha texture : 
	if (InTexture != nullptr)
	{
		if (!InTexture->Source.IsValid())
		{
			UE_LOG(LogLandscapeTools, Error, TEXT("Invalid source data detected for texture (%s), the default AlphaTexture (%s) will be used."), *InTexture->GetPathName(), *GetClass()->GetDefaultObject<ULandscapeEditorObject>()->AlphaTexture->GetPathName());
		}
		else
		{
			// Try to read the new texture data now : 
			const bool bSourceDataIsG8 = (InTexture->Source.GetFormat() == TSF_G8);
			NumChannels = bSourceDataIsG8 ? 1 : 4;
			InTexture->Source.GetMipData(NewTextureData, 0);
			if (NewTextureData.Num() == (NumChannels * InTexture->Source.GetSizeX() * InTexture->Source.GetSizeY()))
			{
				// Valid new texture
				NewAlphaTexture = InTexture;
			}
			else
			{
				UE_LOG(LogLandscapeTools, Error, TEXT("Invalid data size detected for texture (%s), the default AlphaTexture (%s) will be used."), *InTexture->GetPathName(), *GetClass()->GetDefaultObject<ULandscapeEditorObject>()->AlphaTexture->GetPathName());
			}
		}
	}

	// Load fallback if there's no texture or valid data
	if (NewAlphaTexture == nullptr)
	{
		UTexture2D* DefaultAlphaTexture = GetClass()->GetDefaultObject<ULandscapeEditorObject>()->AlphaTexture;
		check((DefaultAlphaTexture != nullptr) && DefaultAlphaTexture->Source.IsValid()); // The default texture should always be valid
		DefaultAlphaTexture->Source.GetMipData(NewTextureData, 0);
		NewAlphaTexture = DefaultAlphaTexture;
		const bool bSourceDataIsG8 = (DefaultAlphaTexture->Source.GetFormat() == TSF_G8);
		NumChannels = bSourceDataIsG8 ? 1 : 4;
	}

	check((NewAlphaTexture != nullptr) && !NewTextureData.IsEmpty() && (NumChannels > 0));

	AlphaTexture = NewAlphaTexture;
	AlphaTextureSizeX = NewAlphaTexture->Source.GetSizeX();
	AlphaTextureSizeY = NewAlphaTexture->Source.GetSizeY();
	AlphaTextureChannel = (NumChannels == 1) ? EColorChannel::Red : InTextureChannel;
	AlphaTextureData.Empty(AlphaTextureSizeX * AlphaTextureSizeY);

	uint8* SrcPtr;
	switch (AlphaTextureChannel)
	{
	case 1:
		SrcPtr = &((FColor*)NewTextureData.GetData())->G;
		break;
	case 2:
		SrcPtr = &((FColor*)NewTextureData.GetData())->B;
		break;
	case 3:
		SrcPtr = &((FColor*)NewTextureData.GetData())->A;
		break;
	default:
		SrcPtr = &((FColor*)NewTextureData.GetData())->R;
		break;
	}

	for (int32 i = 0; i < AlphaTextureSizeX * AlphaTextureSizeY; i++)
	{
		AlphaTextureData.Add(*SrcPtr);
		SrcPtr += NumChannels;
	}
}

void ULandscapeEditorObject::ChooseBestComponentSizeForImport()
{
	FLandscapeImportHelper::ChooseBestComponentSizeForImport(ImportLandscape_Width, ImportLandscape_Height, NewLandscape_QuadsPerSection, NewLandscape_SectionsPerComponent, NewLandscape_ComponentCount);
}

bool ULandscapeEditorObject::UseSingleFileImport() const
{
	if (ParentMode)
	{
		return ParentMode->UseSingleFileImport();
	}

	return true;
}

void ULandscapeEditorObject::RefreshImports()
{
	ClearImportLandscapeData();
	HeightmapImportDescriptorIndex = 0;
	HeightmapImportDescriptor.Reset();
	ImportLandscape_Width = 0;
	ImportLandscape_Height = 0;

	ImportLandscape_HeightmapImportResult = ELandscapeImportResult::Success;
	ImportLandscape_HeightmapErrorMessage = FText();

	if (!ImportLandscape_HeightmapFilename.IsEmpty())
	{
		ImportLandscape_HeightmapImportResult =
			FLandscapeImportHelper::GetHeightmapImportDescriptor(ImportLandscape_HeightmapFilename, UseSingleFileImport(), bFlipYAxis, HeightmapImportDescriptor, ImportLandscape_HeightmapErrorMessage);
		if (ImportLandscape_HeightmapImportResult != ELandscapeImportResult::Error)
		{
			ImportLandscape_Width = HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex].Width;
			ImportLandscape_Height = HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex].Height;
			ChooseBestComponentSizeForImport();
			ImportLandscapeData();
		}
	}

	RefreshLayerImports();
}

void ULandscapeEditorObject::RefreshLayerImports()
{
	// Make sure to reset import width and height if we don't have a Heightmap to import
	if (ImportLandscape_HeightmapFilename.IsEmpty())
	{
		HeightmapImportDescriptorIndex = 0;
		ImportLandscape_Width = 0;
		ImportLandscape_Height = 0;
	}

	for (FLandscapeImportLayer& UIImportLayer : ImportLandscape_Layers)
	{
		RefreshLayerImport(UIImportLayer);
	}
}

void ULandscapeEditorObject::RefreshLayerImport(FLandscapeImportLayer& ImportLayer)
{
	ImportLayer.ErrorMessage = FText();
	ImportLayer.ImportResult = ELandscapeImportResult::Success;

	if (ImportLayer.LayerName == ALandscapeProxy::VisibilityLayer->LayerName)
	{
		ImportLayer.LayerInfo = ALandscapeProxy::VisibilityLayer;
	}

	if (!ImportLayer.SourceFilePath.IsEmpty())
	{
		if (!ImportLayer.LayerInfo)
		{
			ImportLayer.ImportResult = ELandscapeImportResult::Error;
			ImportLayer.ErrorMessage = NSLOCTEXT("LandscapeEditor.NewLandscape", "Import_LayerInfoNotSet", "Can't import a layer file without a layer info");
		}
		else
		{
			ImportLayer.ImportResult = FLandscapeImportHelper::GetWeightmapImportDescriptor(ImportLayer.SourceFilePath, UseSingleFileImport(), bFlipYAxis, ImportLayer.LayerName, ImportLayer.ImportDescriptor, ImportLayer.ErrorMessage);
			if (ImportLayer.ImportResult != ELandscapeImportResult::Error)
			{
				if (ImportLandscape_Height != 0 || ImportLandscape_Width != 0)
				{
					// Use same import index as Heightmap
					int32 FoundIndex = INDEX_NONE;
					for (int32 Index = 0; Index < ImportLayer.ImportDescriptor.FileResolutions.Num(); ++Index)
					{
						if (ImportLayer.ImportDescriptor.ImportResolutions[Index] == HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex])
						{
							FoundIndex = Index;
							break;
						}
					}

					if (FoundIndex == INDEX_NONE)
					{
						ImportLayer.ImportResult = ELandscapeImportResult::Error;
						ImportLayer.ErrorMessage = NSLOCTEXT("LandscapeEditor.ImportLandscape", "Import_WeightHeightResolutionMismatch", "Weightmap import resolution isn't same as Heightmap resolution.");
					}
				}
			}
		}
	}
}

void ULandscapeEditorObject::OnChangeImportLandscapeResolution(int32 DescriptorIndex)
{
	check(DescriptorIndex >= 0 && DescriptorIndex < HeightmapImportDescriptor.ImportResolutions.Num());
	HeightmapImportDescriptorIndex = DescriptorIndex;
	ImportLandscape_Width = HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex].Width;
	ImportLandscape_Height = HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex].Height;
	ClearImportLandscapeData();
	ImportLandscapeData();
	ChooseBestComponentSizeForImport();
}

void ULandscapeEditorObject::ImportLandscapeData()
{
	ImportLandscape_HeightmapImportResult = FLandscapeImportHelper::GetHeightmapImportData(HeightmapImportDescriptor, HeightmapImportDescriptorIndex, ImportLandscape_Data, ImportLandscape_HeightmapErrorMessage);
	if (ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error)
	{
		ImportLandscape_Data.Empty();
	}
}

ELandscapeImportResult ULandscapeEditorObject::CreateImportLayersInfo(TArray<FLandscapeImportLayerInfo>& OutImportLayerInfos)
{
	const uint32 ImportSizeX = ImportLandscape_Width;
	const uint32 ImportSizeY = ImportLandscape_Height;

	if (ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error)
	{
		// Cancel import
		return ELandscapeImportResult::Error;
	}

	OutImportLayerInfos.Reserve(ImportLandscape_Layers.Num());

	// Fill in LayerInfos array and allocate data
	for (FLandscapeImportLayer& UIImportLayer : ImportLandscape_Layers)
	{
		OutImportLayerInfos.Add((const FLandscapeImportLayer&)UIImportLayer); //slicing is fine here
		FLandscapeImportLayerInfo& ImportLayer = OutImportLayerInfos.Last();

		if (ImportLayer.LayerInfo != nullptr && !ImportLayer.SourceFilePath.IsEmpty())
		{
			UIImportLayer.ImportResult = FLandscapeImportHelper::GetWeightmapImportDescriptor(ImportLayer.SourceFilePath, UseSingleFileImport(), bFlipYAxis, ImportLayer.LayerName, UIImportLayer.ImportDescriptor, UIImportLayer.ErrorMessage);
			if (UIImportLayer.ImportResult == ELandscapeImportResult::Error)
			{
				FMessageDialog::Open(EAppMsgType::Ok, UIImportLayer.ErrorMessage);
				return ELandscapeImportResult::Error;
			}

			// Use same import index as Heightmap
			int32 FoundIndex = INDEX_NONE;
			for (int32 Index = 0; Index < UIImportLayer.ImportDescriptor.FileResolutions.Num(); ++Index)
			{
				if (UIImportLayer.ImportDescriptor.ImportResolutions[Index] == HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex])
				{
					FoundIndex = Index;
					break;
				}
			}

			if (FoundIndex == INDEX_NONE)
			{
				UIImportLayer.ImportResult = ELandscapeImportResult::Error;
				UIImportLayer.ErrorMessage = NSLOCTEXT("LandscapeEditor.NewLandscape", "Import_WeightHeightResolutionMismatch", "Weightmap import resolution isn't same as Heightmap resolution.");
				FMessageDialog::Open(EAppMsgType::Ok, UIImportLayer.ErrorMessage);
				return ELandscapeImportResult::Error;
			}

			UIImportLayer.ImportResult = FLandscapeImportHelper::GetWeightmapImportData(UIImportLayer.ImportDescriptor, FoundIndex, ImportLayer.LayerName, ImportLayer.LayerData, UIImportLayer.ErrorMessage);
			if (UIImportLayer.ImportResult == ELandscapeImportResult::Error)
			{
				FMessageDialog::Open(EAppMsgType::Ok, UIImportLayer.ErrorMessage);
				return ELandscapeImportResult::Error;
			}
		}
	}

	return ELandscapeImportResult::Success;
}

ELandscapeImportResult ULandscapeEditorObject::CreateNewLayersInfo(TArray<FLandscapeImportLayerInfo>& OutNewLayerInfos)
{
	const int32 QuadsPerComponent = NewLandscape_SectionsPerComponent * NewLandscape_QuadsPerSection;
	const int32 SizeX = NewLandscape_ComponentCount.X * QuadsPerComponent + 1;
	const int32 SizeY = NewLandscape_ComponentCount.Y * QuadsPerComponent + 1;

	OutNewLayerInfos.Reset(ImportLandscape_Layers.Num());

	// Fill in LayerInfos array and allocate data
	for (const FLandscapeImportLayer& UIImportLayer : ImportLandscape_Layers)
	{
		FLandscapeImportLayerInfo ImportLayer = FLandscapeImportLayerInfo(UIImportLayer.LayerName);
		ImportLayer.LayerInfo = UIImportLayer.LayerInfo;
		ImportLayer.SourceFilePath = "";
		ImportLayer.LayerData = TArray<uint8>();
		OutNewLayerInfos.Add(MoveTemp(ImportLayer));
	}

	// Fill the first weight-blended layer to 100%
	if (FLandscapeImportLayerInfo* FirstBlendedLayer = OutNewLayerInfos.FindByPredicate([](const FLandscapeImportLayerInfo& ImportLayer) { return ImportLayer.LayerInfo && !ImportLayer.LayerInfo->bNoWeightBlend; }))
	{
		const int32 DataSize = SizeX * SizeY;
		FirstBlendedLayer->LayerData.AddUninitialized(DataSize);

		uint8* ByteData = FirstBlendedLayer->LayerData.GetData();
		FMemory::Memset(ByteData, 255, DataSize);
	}

	return ELandscapeImportResult::Success;
}

void ULandscapeEditorObject::InitializeDefaultHeightData(TArray<uint16>& OutData)
{
	const int32 QuadsPerComponent = NewLandscape_SectionsPerComponent * NewLandscape_QuadsPerSection;
	const int32 SizeX = NewLandscape_ComponentCount.X * QuadsPerComponent + 1;
	const int32 SizeY = NewLandscape_ComponentCount.Y * QuadsPerComponent + 1;
	const int32 TotalSize = SizeX * SizeY;
	// Initialize heightmap data
	OutData.Reset();
	OutData.AddUninitialized(TotalSize);
	
	TArray<uint16> StrideData;
	StrideData.AddUninitialized(SizeX);
	// Initialize blank heightmap data
	for (int32 X = 0; X < SizeX; ++X)
	{
		StrideData[X] = LandscapeDataAccess::MidValue;
	}
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		FMemory::Memcpy(&OutData[Y * SizeX], StrideData.GetData(), sizeof(uint16) * SizeX);
	}
}

void ULandscapeEditorObject::ExpandImportData(TArray<uint16>& OutHeightData, TArray<FLandscapeImportLayerInfo>& OutImportLayerInfos)
{
	const TArray<uint16>& ImportData = GetImportLandscapeData();
	if (ImportData.Num())
	{
		const int32 QuadsPerComponent = NewLandscape_SectionsPerComponent * NewLandscape_QuadsPerSection;
		FLandscapeImportResolution RequiredResolution(NewLandscape_ComponentCount.X * QuadsPerComponent + 1, NewLandscape_ComponentCount.Y * QuadsPerComponent + 1);
		FLandscapeImportResolution ImportResolution(ImportLandscape_Width, ImportLandscape_Height);

		FLandscapeImportHelper::TransformHeightmapImportData(ImportData, OutHeightData, ImportResolution, RequiredResolution, ELandscapeImportTransformType::ExpandCentered);

		for (int32 LayerIdx = 0; LayerIdx < OutImportLayerInfos.Num(); ++LayerIdx)
		{
			TArray<uint8>& OutImportLayerData = OutImportLayerInfos[LayerIdx].LayerData;
			TArray<uint8> OutLayerData;
			if (OutImportLayerData.Num())
			{
				FLandscapeImportHelper::TransformWeightmapImportData(OutImportLayerData, OutLayerData, ImportResolution, RequiredResolution, ELandscapeImportTransformType::ExpandCentered);
				OutImportLayerData = MoveTemp(OutLayerData);
			}
		}
	}
}

void ULandscapeEditorObject::RefreshImportLayersList(bool bRefreshFromTarget)
{
	UTexture2D* ThumbnailWeightmap = nullptr;
	UTexture2D* ThumbnailHeightmap = nullptr;
		
	TArray<FName> LayerNames;
	TArray<ULandscapeLayerInfoObject*> LayerInfoObjs;
	UMaterialInterface* Material = nullptr;
	if (bRefreshFromTarget)
	{
		LayerNames.Reset(ImportLandscape_Layers.Num());
		LayerInfoObjs.Reset(ImportLandscape_Layers.Num());
		Material = ParentMode->GetTargetLandscapeMaterial();
		for (const TSharedRef<FLandscapeTargetListInfo>& TargetListInfo : ParentMode->GetTargetList())
		{
			if ((TargetListInfo->TargetType != ELandscapeToolTargetType::Weightmap) && (TargetListInfo->TargetType != ELandscapeToolTargetType::Visibility))
			{
				continue;
			}

			LayerNames.Add(TargetListInfo->LayerName);
			LayerInfoObjs.Add(TargetListInfo->LayerInfoObj.Get());
		}
	}
	else
	{
		Material = NewLandscape_Material.Get();
		LayerNames = ALandscapeProxy::GetLayersFromMaterial(Material);
	}

	const TArray<FLandscapeImportLayer> OldLayersList = MoveTemp(ImportLandscape_Layers);
	ImportLandscape_Layers.Reset(LayerNames.Num());

	for (int32 i = 0; i < LayerNames.Num(); i++)
	{
		const FName& LayerName = LayerNames[i];

		if (!LayerName.IsNone())
		{
			bool bFound = false;
			FLandscapeImportLayer NewImportLayer;
			NewImportLayer.ImportResult = ELandscapeImportResult::Success;
			NewImportLayer.ErrorMessage = FText();

			for (int32 j = 0; j < OldLayersList.Num(); j++)
			{
				if (OldLayersList[j].LayerName == LayerName)
				{
					NewImportLayer = OldLayersList[j];
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				if (NewImportLayer.ThumbnailMIC->Parent != Material)
				{
					FMaterialUpdateContext Context;
					NewImportLayer.ThumbnailMIC->SetParentEditorOnly(Material);
					Context.AddMaterialInterface(NewImportLayer.ThumbnailMIC);
				}
			}
			else
			{
				if (!ThumbnailWeightmap)
				{
					ThumbnailWeightmap = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorLandscapeResources/LandscapeThumbnailWeightmap.LandscapeThumbnailWeightmap"), nullptr, LOAD_None, nullptr);
				}

				if (!ThumbnailHeightmap)
				{
					ThumbnailHeightmap = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorLandscapeResources/LandscapeThumbnailHeightmap.LandscapeThumbnailHeightmap"), nullptr, LOAD_None, nullptr);
				}

				NewImportLayer.LayerName = LayerName;
				NewImportLayer.ThumbnailMIC = ALandscapeProxy::GetLayerThumbnailMIC(Material, LayerName, ThumbnailWeightmap, ThumbnailHeightmap, nullptr);
			}

			if (bRefreshFromTarget)
			{
				NewImportLayer.LayerInfo = LayerInfoObjs[i];
			}

			RefreshLayerImport(NewImportLayer);

			ImportLandscape_Layers.Add(MoveTemp(NewImportLayer));
		}
	}
}

void ULandscapeEditorObject::UpdateComponentLayerAllowList()
{
	if (ParentMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		ParentMode->CurrentToolTarget.LandscapeInfo->UpdateComponentLayerAllowList();
	}
}

void ULandscapeEditorObject::UpdateTargetLayerDisplayOrder()
{
	if (ParentMode != nullptr)
	{
		ParentMode->UpdateTargetLayerDisplayOrder(TargetDisplayOrder);
	}
}

void ULandscapeEditorObject::UpdateShowUnusedLayers()
{
	if (ParentMode != nullptr)
	{
		ParentMode->UpdateShownLayerList();
	}
}

float ULandscapeEditorObject::GetCurrentToolStrength() const
{
	if (IsWeightmapTarget())
	{
		return PaintToolStrength;
	}
	return ToolStrength;
}

void ULandscapeEditorObject::SetCurrentToolStrength(float NewToolStrength)
{
	if (IsWeightmapTarget())
	{
		PaintToolStrength = NewToolStrength;		
	}
	else
	{
		ToolStrength = NewToolStrength;
	}
}

float ULandscapeEditorObject::GetCurrentToolBrushRadius() const
{
	if (IsWeightmapTarget())
	{
		return PaintBrushRadius;
	}
	return BrushRadius;
	
	
}

void ULandscapeEditorObject::SetCurrentToolBrushRadius(float NewBrushStrength)
{
	if (IsWeightmapTarget())
	{
		PaintBrushRadius = NewBrushStrength;
	}
	else
	{
		BrushRadius = NewBrushStrength;
	}
}

float ULandscapeEditorObject::GetCurrentToolBrushFalloff() const
{
	if (IsWeightmapTarget())
	{
		return PaintBrushFalloff;
		
	}
	return BrushFalloff;
	
}

void ULandscapeEditorObject::SetCurrentToolBrushFalloff(float NewBrushFalloff)
{
	if (IsWeightmapTarget())
	{
		PaintBrushFalloff = NewBrushFalloff;
	}
	else
	{
		BrushFalloff = NewBrushFalloff;
	}
}
