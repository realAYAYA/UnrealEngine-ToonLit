// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraBakerRenderer.h"
#include "NiagaraBakerSettings.h"
#include "NiagaraBakerOutputRegistry.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComputeExecutionContext.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraBatchedElements.h"

#include "NiagaraDataInterfaceGrid2DCollection.h"
#include "NiagaraDataInterfaceRenderTarget2D.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "AdvancedPreviewScene.h"
#include "BufferVisualizationData.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"
#include "EngineModule.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "ImageWrapperHelper.h"
#include "LegacyScreenPercentageDriver.h"
#include "VolumeCache.h"

//////////////////////////////////////////////////////////////////////////

const FString FNiagaraBakerOutputBindingHelper::STRING_SceneCaptureSource("SceneCaptureSource");
const FString FNiagaraBakerOutputBindingHelper::STRING_BufferVisualization("BufferVisualization");
const FString FNiagaraBakerOutputBindingHelper::STRING_EmitterDI("EmitterDI");
const FString FNiagaraBakerOutputBindingHelper::STRING_EmitterParticles("EmitterParticles");

FNiagaraBakerOutputBindingHelper::ERenderType FNiagaraBakerOutputBindingHelper::GetRenderType(FName BindingName, FName& OutName)
{
	OutName = FName();
	if (BindingName.IsNone())
	{
		return ERenderType::SceneCapture;
	}

	FString SourceBindingString = BindingName.ToString();
	TArray<FString> SplitNames;
	SourceBindingString.ParseIntoArray(SplitNames, TEXT("."));

	if (!ensure(SplitNames.Num() > 0))
	{
		return ERenderType::None;
	}

	// Scene Capture mode
	if (SplitNames[0] == STRING_SceneCaptureSource)
	{
		if (!ensure(SplitNames.Num() == 2))
		{
			return ERenderType::None;
		}

		OutName = FName(SplitNames[1]);
		return ERenderType::SceneCapture;
	}

	// Buffer Visualization Mode
	if (SplitNames[0] == STRING_BufferVisualization)
	{
		if (!ensure(SplitNames.Num() == 2))
		{
			return ERenderType::None;
		}
		OutName = FName(SplitNames[1]);
		return ERenderType::BufferVisualization;
	}

	// Emitter Data Interface
	if (SplitNames[0] == STRING_EmitterDI)
	{
		OutName = FName(*SourceBindingString.RightChop(SplitNames[0].Len() + 1));
		return ERenderType::DataInterface;
	}

	// Emitter Data Interface
	if (SplitNames[0] == STRING_EmitterParticles)
	{
		OutName = FName(*SourceBindingString.RightChop(SplitNames[0].Len() + 1));
		return ERenderType::Particle;
	}
	return ERenderType::None;
}

void FNiagaraBakerOutputBindingHelper::GetSceneCaptureBindings(TArray<FNiagaraBakerOutputBinding>& OutBindings)
{
	static UEnum* SceneCaptureOptions = StaticEnum<ESceneCaptureSource>();
	for (int i = 0; i < SceneCaptureOptions->GetMaxEnumValue(); ++i)
	{
		FNiagaraBakerOutputBinding& NewBinding = OutBindings.AddDefaulted_GetRef();
		NewBinding.BindingName = FName(STRING_SceneCaptureSource + TEXT(".") + SceneCaptureOptions->GetNameStringByIndex(i));
		NewBinding.MenuCategory = FText::FromString(STRING_SceneCaptureSource);
		NewBinding.MenuEntry = SceneCaptureOptions->GetDisplayNameTextByIndex(i);
	}
}

void FNiagaraBakerOutputBindingHelper::GetBufferVisualizationBindings(TArray<FNiagaraBakerOutputBinding>& OutBindings)
{
	// Gather all buffer visualization options
	struct FIterator
	{
		TArray<FNiagaraBakerOutputBinding>& OutBindings;

		FIterator(TArray<FNiagaraBakerOutputBinding>& InOutBindings)
			: OutBindings(InOutBindings)
		{}

		void ProcessValue(const FString& MaterialName, UMaterialInterface* Material, const FText& DisplayName)
		{
			FNiagaraBakerOutputBinding& NewBinding = OutBindings.AddDefaulted_GetRef();
			NewBinding.BindingName = FName(STRING_BufferVisualization + TEXT(".") + MaterialName);
			NewBinding.MenuCategory = FText::FromString(STRING_BufferVisualization);
			NewBinding.MenuEntry = DisplayName;
		}
	} Iterator(OutBindings);
	GetBufferVisualizationData().IterateOverAvailableMaterials(Iterator);
}

void FNiagaraBakerOutputBindingHelper::ForEachEmitterDataInterface(UNiagaraSystem* NiagaraSystem, FEmitterDIFunction Function)
{
	check(NiagaraSystem);

	for (int32 EmitterIndex=0; EmitterIndex < NiagaraSystem->GetEmitterHandles().Num(); ++EmitterIndex)
	{
		const FNiagaraEmitterHandle& EmitterHandle = NiagaraSystem->GetEmitterHandle(EmitterIndex);
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetInstance().GetEmitterData();
		if (!EmitterHandle.IsValid() || !EmitterHandle.GetIsEnabled() || !EmitterData)
		{
			continue;
		}

		const FString EmitterName = EmitterHandle.GetName().ToString();
		const FString EmitterPrefix = EmitterName + TEXT(".");

		EmitterData->ForEachScript(
			[&](UNiagaraScript* NiagaraScript)
			{
				if (const FNiagaraScriptExecutionParameterStore* SrcStore = NiagaraScript->GetExecutionReadyParameterStore(EmitterData->SimTarget))
				{
					for (const FNiagaraVariableWithOffset& Variable : SrcStore->ReadParameterVariables())
					{
						if (Variable.IsDataInterface() == false)
						{
							continue;
						}

						const FString VariableName = Variable.GetName().ToString();
						if (!VariableName.StartsWith(EmitterPrefix))
						{
							continue;
						}

						UNiagaraDataInterface* DataInterface = SrcStore->GetDataInterface(Variable.Offset);
						check(DataInterface);

						Function(EmitterName, VariableName.Mid(EmitterPrefix.Len()), DataInterface);
					}
				}
			}
		);
	}
}

UNiagaraDataInterface* FNiagaraBakerOutputBindingHelper::GetDataInterface(UNiagaraComponent* NiagaraComponent, FName DataInterfaceName)
{
	// Find data interface
	FNiagaraSystemInstanceControllerPtr SystemInstanceController = NiagaraComponent->GetSystemInstanceController();
	if ( SystemInstanceController.IsValid() == false )
	{
		return nullptr;
	}

	FNiagaraSystemInstance* SystemInstance = SystemInstanceController->GetSoloSystemInstance();
	for (auto EmitterInstance : SystemInstance->GetEmitters())
	{
		if ( FNiagaraComputeExecutionContext* ComputeContext = EmitterInstance->GetGPUContext() )
		{
			for (const FNiagaraVariableWithOffset& Variable : ComputeContext->CombinedParamStore.ReadParameterVariables())
			{
				if (Variable.IsDataInterface() && (Variable.GetName() == DataInterfaceName))
				{
					return ComputeContext->CombinedParamStore.GetDataInterface(Variable.Offset);
				}
			}
		}
	}
	return nullptr;
}

void FNiagaraBakerOutputBindingHelper::GetDataInterfaceBindingsForCanvas(TArray<FNiagaraBakerOutputBinding>& OutBindings, UNiagaraSystem* NiagaraSystem)
{
	check(NiagaraSystem);

	ForEachEmitterDataInterface(
		NiagaraSystem,
		[&](const FString& EmitterName, const FString& VariableName, UNiagaraDataInterface* DataInterface)
		{
			if (DataInterface->CanRenderVariablesToCanvas())
			{
				TArray<FNiagaraVariableBase> RendererableVariables;
				DataInterface->GetCanvasVariables(RendererableVariables);
				for (const FNiagaraVariableBase& RendererableVariable : RendererableVariables)
				{
					const FString VariableString = VariableName + TEXT(".") + RendererableVariable.GetName().ToString();

					FNiagaraBakerOutputBinding& NewBinding = OutBindings.AddDefaulted_GetRef();
					NewBinding.BindingName = FName(STRING_EmitterDI + TEXT(".") + EmitterName + TEXT(".") + VariableString);
					NewBinding.MenuCategory = FText::FromString(TEXT("DataInterface") + EmitterName);
					NewBinding.MenuEntry = FText::FromString(VariableString);
				}
			}
		}
	);
}

void FNiagaraBakerOutputBindingHelper::GetParticleAttributeBindings(TArray<FNiagaraBakerOutputBinding>& OutBindings, UNiagaraSystem* NiagaraSystem)
{
	check(NiagaraSystem);

	const TArray<TSharedRef<const FNiagaraEmitterCompiledData>>& AllEmitterCompiledData = NiagaraSystem->GetEmitterCompiledData();

	for (int32 EmitterIndex = 0; EmitterIndex < NiagaraSystem->GetEmitterHandles().Num(); ++EmitterIndex)
	{
		const FNiagaraEmitterHandle& EmitterHandle = NiagaraSystem->GetEmitterHandle(EmitterIndex);
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetInstance().GetEmitterData();
		if (!EmitterHandle.IsValid() || !EmitterHandle.GetIsEnabled() || !EmitterData)
		{
			continue;
		}

		const FString EmitterName = EmitterHandle.GetName().ToString();

		if (ensure(AllEmitterCompiledData.IsValidIndex(EmitterIndex)))
		{
			const FNiagaraDataSetCompiledData& ParticleDataSet = AllEmitterCompiledData[EmitterIndex]->DataSetCompiledData;
			for (int32 iVariable = 0; iVariable < ParticleDataSet.VariableLayouts.Num(); ++iVariable)
			{
				const FNiagaraVariable& Variable = ParticleDataSet.Variables[iVariable];
				const FNiagaraVariableLayoutInfo& VariableLayout = ParticleDataSet.VariableLayouts[iVariable];
				if (VariableLayout.GetNumFloatComponents() > 0)
				{
					const FString VariableString = Variable.GetName().ToString();

					FNiagaraBakerOutputBinding& NewBinding = OutBindings.AddDefaulted_GetRef();
					NewBinding.BindingName = FName(STRING_EmitterParticles + TEXT(".") + EmitterName + TEXT(".") + VariableString);
					NewBinding.MenuCategory = FText::FromString(TEXT("ParticleAttribute ") + EmitterName);
					NewBinding.MenuEntry = FText::FromString(VariableString);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

FNiagaraBakerRenderer::FNiagaraBakerRenderer(UNiagaraSystem* InNiagaraSystem)
	: NiagaraSystem(InNiagaraSystem)
{
	SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	SceneCaptureComponent->bTickInEditor = false;
	SceneCaptureComponent->SetComponentTickEnabled(false);
	SceneCaptureComponent->SetVisibility(true);
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;

	CreatePreviewScene(NiagaraSystem, PreviewComponent, AdvancedPreviewScene);
}

FNiagaraBakerRenderer::~FNiagaraBakerRenderer()
{
	DestroyPreviewScene(PreviewComponent, AdvancedPreviewScene);
	DestroyPreviewScene(SimCachePreviewComponent, SimCacheAdvancedPreviewScene);
}

void FNiagaraBakerRenderer::SetAbsoluteTime(float AbsoluteTime, bool bShouldTickComponent)
{
	UNiagaraBakerSettings* BakerSettings = GetBakerSettings();
	if ( !ensure(BakerSettings) )
	{
		return;
	}

	PreviewComponent->SetSeekDelta(BakerSettings->GetSeekDelta());
	PreviewComponent->SeekToDesiredAge(AbsoluteTime);

	if (bShouldTickComponent)
	{
		PreviewComponent->TickComponent(BakerSettings->GetSeekDelta(), ELevelTick::LEVELTICK_All, nullptr);

		// World should be guaranteed but let's be safe
		UWorld* World = PreviewComponent->GetWorld();
		if ( ensure(World) )
		{
			// Send EOF updates before we flush our pending ticks to ensure everything is ready for Niagara
			World->SendAllEndOfFrameUpdates();

			// Since captures, etc, don't flush GPU updates so we need to force flush them
			FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = FNiagaraGpuComputeDispatchInterface::Get(World);
			if ( ensureMsgf(ComputeDispatchInterface, TEXT("The batcher was not valid on the world this may result in incorrect baking")) )
			{
				ComputeDispatchInterface->FlushPendingTicks_GameThread();
			}
		}
	}
}

void FNiagaraBakerRenderer::RenderSceneCapture(UTextureRenderTarget2D* RenderTarget, ESceneCaptureSource CaptureSource) const
{
	RenderSceneCapture(RenderTarget, PreviewComponent, CaptureSource);
}

void FNiagaraBakerRenderer::RenderSceneCapture(UTextureRenderTarget2D* RenderTarget, UNiagaraComponent* NiagaraComponent, ESceneCaptureSource CaptureSource) const
{
	UNiagaraBakerSettings* BakerSettings = GetBakerSettings();
	if (!NiagaraComponent || !RenderTarget || !BakerSettings)
	{
		return;
	}

	const float WorldTime = GetWorldTime();
	UWorld* World = NiagaraComponent->GetWorld();

	FCanvas Canvas(RenderTarget->GameThread_GetRenderTargetResource(), nullptr, FGameTime::CreateUndilated(WorldTime, FApp::GetDeltaTime()), GetFeatureLevel());
	Canvas.Clear(FLinearColor::Black);

	SceneCaptureComponent->RegisterComponentWithWorld(World);
	SceneCaptureComponent->TextureTarget = RenderTarget;
	SceneCaptureComponent->CaptureSource = CaptureSource;

	// Set view location
	const FNiagaraBakerCameraSettings& CurrentCamera = BakerSettings->GetCurrentCamera();
	if (CurrentCamera.IsOrthographic())
	{
		SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
		SceneCaptureComponent->OrthoWidth = CurrentCamera.OrthoWidth;
	}
	else
	{
		SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
		SceneCaptureComponent->FOVAngle = CurrentCamera.FOV;
	}

	const FMatrix SceneCaptureMatrix = FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	FMatrix ViewMatrix = SceneCaptureMatrix * BakerSettings->GetViewportMatrix().Inverse() * FRotationTranslationMatrix(BakerSettings->GetCameraRotation(), BakerSettings->GetCameraLocation());
	SceneCaptureComponent->SetWorldLocationAndRotation(ViewMatrix.GetOrigin(), ViewMatrix.Rotator());

	SceneCaptureComponent->bUseCustomProjectionMatrix = true;
	SceneCaptureComponent->CustomProjectionMatrix = BakerSettings->GetProjectionMatrix();

	if (BakerSettings->bRenderComponentOnly)
	{
		SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		SceneCaptureComponent->ShowOnlyComponents.Empty(1);
		SceneCaptureComponent->ShowOnlyComponents.Add(NiagaraComponent);
	}
	else
	{
		SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	}

	SceneCaptureComponent->CaptureScene();

	SceneCaptureComponent->TextureTarget = nullptr;
	SceneCaptureComponent->UnregisterComponent();

	// Alpha from a scene capture is 1- so we need to invert
	if (SceneCaptureComponent->CaptureSource == ESceneCaptureSource::SCS_SceneColorHDR)
	{
		FCanvasTileItem TileItem(FVector2D(0, 0), FVector2D(RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight()), FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Opaque;
		TileItem.BatchedElementParameters = new FBatchedElementNiagaraInvertColorChannel(0);
		Canvas.DrawItem(TileItem);
	}
	Canvas.Flush_GameThread();
}

void FNiagaraBakerRenderer::RenderBufferVisualization(UTextureRenderTarget2D* RenderTarget, FName BufferVisualizationMode) const
{
	UNiagaraBakerSettings* BakerSettings = GetBakerSettings();
	if (!RenderTarget || !BakerSettings)
	{
		return;
	}

	const FIntRect ViewRect = FIntRect(0, 0, RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight());
	const float GammaCorrection = 1.0f;
	const float WorldTime = GetWorldTime();
	UWorld* World = GetWorld();

	FCanvas Canvas(RenderTarget->GameThread_GetRenderTargetResource(), nullptr, FGameTime::CreateUndilated(WorldTime, FApp::GetDeltaTime()), GetFeatureLevel());
	Canvas.Clear(FLinearColor::Black);

	// Create View Family
	FSceneViewFamilyContext ViewFamily(
		FSceneViewFamily::ConstructionValues(RenderTarget->GameThread_GetRenderTargetResource(), World->Scene, FEngineShowFlags(ESFIM_Game))
		.SetTime(FGameTime::CreateUndilated(WorldTime, FApp::GetDeltaTime()))
		.SetGammaCorrection(GammaCorrection)
	);
	
	ViewFamily.EngineShowFlags.SetScreenPercentage(false);
	//ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	//ViewFamily.EngineShowFlags.MotionBlur = 0;
	//ViewFamily.EngineShowFlags.SetDistanceCulledPrimitives(true); // show distance culled objects
	//ViewFamily.EngineShowFlags.SetPostProcessing(false);
	
	if (BufferVisualizationMode.IsValid())
	{
		ViewFamily.EngineShowFlags.SetPostProcessing(true);
		ViewFamily.EngineShowFlags.SetVisualizeBuffer(true);
		ViewFamily.EngineShowFlags.SetTonemapper(false);
		ViewFamily.EngineShowFlags.SetScreenPercentage(false);
	}
	
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.ViewOrigin = BakerSettings->GetCameraLocation();
	ViewInitOptions.ViewRotationMatrix = BakerSettings->GetViewMatrix();
	ViewInitOptions.ProjectionMatrix = BakerSettings->GetProjectionMatrix();
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	if (BakerSettings->bRenderComponentOnly)
	{
		ViewInitOptions.ShowOnlyPrimitives.Emplace();
		ViewInitOptions.ShowOnlyPrimitives->Add(PreviewComponent->ComponentId);
	}
	
	FSceneView* NewView = new FSceneView(ViewInitOptions);
	NewView->CurrentBufferVisualizationMode = BufferVisualizationMode;
	ViewFamily.Views.Add(NewView);
	
	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));
	
	GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);

	Canvas.Flush_GameThread();
}

void FNiagaraBakerRenderer::RenderDataInterface(UTextureRenderTarget2D* RenderTarget, FName BindingName) const
{
	const float WorldTime = GetWorldTime();
	FCanvas Canvas(RenderTarget->GameThread_GetRenderTargetResource(), nullptr, FGameTime::CreateUndilated(WorldTime, FApp::GetDeltaTime()), GetFeatureLevel());
	Canvas.Clear(FLinearColor::Black);

	ON_SCOPE_EXIT{ Canvas.Flush_GameThread(); };

	// Gather data interface / attribute name
	FString SourceString = BindingName.ToString();
	int32 DotIndex;
	if (!SourceString.FindLastChar('.', DotIndex))
	{
		return;
	}

	const FName DataInterfaceName = FName(SourceString.LeftChop(SourceString.Len() - DotIndex));
	const FName VariableName = FName(SourceString.RightChop(DotIndex + 1));

	// Find data interface
	FNiagaraSystemInstanceControllerPtr SystemInstanceController = PreviewComponent->GetSystemInstanceController();
	if ( SystemInstanceController.IsValid() == false )
	{
		return;
	}

	FNiagaraSystemInstance* SystemInstance = SystemInstanceController->GetSoloSystemInstance();
	const FNiagaraSystemInstanceID SystemInstanceID = SystemInstance->GetId();
	for (auto EmitterInstance : SystemInstance->GetEmitters())
	{
		FNiagaraComputeExecutionContext* ExecContext = EmitterInstance->GetGPUContext();
		if ( ExecContext == nullptr )
		{
			continue;
		}

		for (const FNiagaraVariableWithOffset& Variable : ExecContext->CombinedParamStore.ReadParameterVariables())
		{
			if (Variable.IsDataInterface())
			{
				if (Variable.GetName() == DataInterfaceName)
				{
					if ( UNiagaraDataInterface* DataInterface = ExecContext->CombinedParamStore.GetDataInterface(Variable.Offset) )
					{
						const FIntRect ViewRect(0, 0, RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight());
						DataInterface->RenderVariableToCanvas(SystemInstanceID, VariableName, &Canvas, ViewRect);
						return;
					}
				}
			}
		}
	}
}

void FNiagaraBakerRenderer::RenderParticleAttribute(UTextureRenderTarget2D* RenderTarget, FName BindingName) const
{
	const float WorldTime = GetWorldTime();
	FCanvas Canvas(RenderTarget->GameThread_GetRenderTargetResource(), nullptr, FGameTime::CreateUndilated(WorldTime, FApp::GetDeltaTime()), GetFeatureLevel());
	Canvas.Clear(FLinearColor::Black);

	ON_SCOPE_EXIT { Canvas.Flush_GameThread(); };

	FString SourceString = BindingName.ToString();
	int32 DotIndex;
	if ( !SourceString.FindChar('.', DotIndex) )
	{
		return;
	}
	
	const FString EmitterName = SourceString.LeftChop(SourceString.Len() - DotIndex);
	const FName AttributeName = FName(SourceString.RightChop(DotIndex + 1));
	
	FNiagaraSystemInstanceControllerPtr SystemInstanceController = PreviewComponent->GetSystemInstanceController();
	if (!ensure(SystemInstanceController.IsValid()))
	{
		return;
	}
	
	FNiagaraSystemInstance* SystemInstance = SystemInstanceController->GetSoloSystemInstance();
	if ( !ensure(SystemInstance) )
	{
		return;
	}
	
	for ( const auto& EmitterInstance : SystemInstance->GetEmitters() )
	{
		UNiagaraEmitter* NiagaraEmitter = EmitterInstance->GetCachedEmitter().Emitter;
		if ( !NiagaraEmitter || (NiagaraEmitter->GetUniqueEmitterName() != EmitterName) )
		{
			continue;
		}
	
		if (EmitterInstance->GetGPUContext() != nullptr)
		{
			return;
		}
	
		const FNiagaraDataSet& ParticleDataSet = EmitterInstance->GetData();
		const FNiagaraDataBuffer* ParticleDataBuffer = ParticleDataSet.GetCurrentData();
		FNiagaraDataSetReaderInt32<int32> UniqueIDAccessor = FNiagaraDataSetAccessor<int32>::CreateReader(ParticleDataSet, FName("UniqueID"));
		if ( !ParticleDataBuffer || !UniqueIDAccessor.IsValid() )
		{
			return;
		}
	
		const int32 VariableIndex = ParticleDataSet.GetCompiledData().Variables.IndexOfByPredicate([&AttributeName](const FNiagaraVariable& Variable) { return Variable.GetName() == AttributeName; });
		if (VariableIndex == INDEX_NONE)
		{
			return;
		}
		const FNiagaraVariableLayoutInfo& VariableInfo = ParticleDataSet.GetCompiledData().VariableLayouts[VariableIndex];
	
		float* FloatChannels[4];
		FloatChannels[0] = (float*)ParticleDataBuffer->GetComponentPtrFloat(VariableInfo.FloatComponentStart);
		FloatChannels[1] = VariableInfo.GetNumFloatComponents() > 1 ? (float*)ParticleDataBuffer->GetComponentPtrFloat(VariableInfo.FloatComponentStart + 1) : nullptr;
		FloatChannels[2] = VariableInfo.GetNumFloatComponents() > 2 ? (float*)ParticleDataBuffer->GetComponentPtrFloat(VariableInfo.FloatComponentStart + 2) : nullptr;
		FloatChannels[3] = VariableInfo.GetNumFloatComponents() > 3 ? (float*)ParticleDataBuffer->GetComponentPtrFloat(VariableInfo.FloatComponentStart + 3) : nullptr;
	
		const FIntPoint RenderTargetSize(RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight());
		const int32 ParticleBufferStore = RenderTargetSize.X * RenderTargetSize.Y;
		for ( uint32 i=0; i < ParticleDataBuffer->GetNumInstances(); ++i )
		{
			const int32 UniqueID = UniqueIDAccessor[i];
			if (UniqueID >= ParticleBufferStore)
			{
				continue;
			}
	
			FLinearColor OutputColor;
			OutputColor.R = FloatChannels[0] ? FloatChannels[0][i] : 0.0f;
			OutputColor.G = FloatChannels[1] ? FloatChannels[1][i] : 0.0f;
			OutputColor.B = FloatChannels[2] ? FloatChannels[2][i] : 0.0f;
			OutputColor.A = FloatChannels[3] ? FloatChannels[3][i] : 0.0f;
	
			const int32 TexelX = UniqueID % RenderTargetSize.X;
			const int32 TexelY = UniqueID / RenderTargetSize.X;
			Canvas.DrawTile(TexelX, TexelY, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, OutputColor);
		}

		// We are done
		break;
	}
}

void FNiagaraBakerRenderer::RenderSimCache(UTextureRenderTarget2D* RenderTarget, UNiagaraSimCache* SimCache) const
{
	UNiagaraBakerSettings* BakerSettings = GetBakerSettings();
	if (!SimCache)
	{
		return;
	}

	if (SimCachePreviewComponent == nullptr)
	{
		CreatePreviewScene(NiagaraSystem, SimCachePreviewComponent, SimCacheAdvancedPreviewScene);
	}

	const float SeekDelta = BakerSettings->GetSeekDelta();

	SimCachePreviewComponent->SetSimCache(SimCache);
	SimCachePreviewComponent->SetSeekDelta(SeekDelta);
	SimCachePreviewComponent->SeekToDesiredAge(GetWorldTime());
	SimCachePreviewComponent->TickComponent(SeekDelta, ELevelTick::LEVELTICK_All, nullptr);

	SimCachePreviewComponent->MarkRenderDynamicDataDirty();
	UWorld* World = SimCachePreviewComponent->GetWorld();
	World->SendAllEndOfFrameUpdates();

	RenderSceneCapture(RenderTarget, SimCachePreviewComponent, ESceneCaptureSource::SCS_SceneColorHDR);

	SimCachePreviewComponent->SetSimCache(nullptr);
}

UWorld* FNiagaraBakerRenderer::GetWorld() const
{
	return PreviewComponent->GetWorld();
}

float FNiagaraBakerRenderer::GetWorldTime() const
{
	return PreviewComponent->GetDesiredAge();
}

ERHIFeatureLevel::Type FNiagaraBakerRenderer::GetFeatureLevel() const
{
	return PreviewComponent->GetWorld()->Scene->GetFeatureLevel();
}

UNiagaraSystem* FNiagaraBakerRenderer::GetNiagaraSystem() const
{
	return PreviewComponent->GetAsset();
}

void FNiagaraBakerRenderer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(NiagaraSystem);
	Collector.AddReferencedObject(PreviewComponent);
	Collector.AddReferencedObject(SceneCaptureComponent);
	Collector.AddReferencedObject(SimCachePreviewComponent);
}

FNiagaraBakerOutputRenderer* FNiagaraBakerRenderer::GetOutputRenderer(UClass* Class)
{
	return FNiagaraBakerOutputRegistry::Get().GetRendererForClass(Class);
}

bool FNiagaraBakerRenderer::ExportImage(FStringView FilePath, FIntPoint ImageSize, TArrayView<FFloat16Color> ImageData)
{
	const FString FileExtension = FPaths::GetExtension(FilePath.GetData(), true);
	const EImageFormat ImageFormat = ImageWrapperHelper::GetImageFormat(FileExtension);
	if (ImageFormat == EImageFormat::Invalid)
	{
		return false;
	}

	IImageWrapperModule & ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if ( ImageWrapper.IsValid() == false )
	{
		return false;
	}

	if ( ImageFormat == EImageFormat::EXR || ImageFormat == EImageFormat::HDR )
	{
		TArray<FLinearColor> TempImageData;
		TempImageData.Reserve(ImageData.Num());
		for (const FFloat16Color& HalfColor : ImageData)
		{
			TempImageData.Emplace(HalfColor.GetFloats());
		}

		if (ImageWrapper->SetRaw(TempImageData.GetData(), TempImageData.Num() * TempImageData.GetTypeSize(), ImageSize.X, ImageSize.Y, ERGBFormat::RGBAF, 32) == false)
		{
			return false;
		}
	}
	else
	{
		TArray<FColor> TempImageData;
		TempImageData.Reserve(ImageData.Num());
		for (const FFloat16Color& HalfColor : ImageData)
		{
			TempImageData.Add(HalfColor.GetFloats().ToFColor(true));
		}

		if (ImageWrapper->SetRaw(TempImageData.GetData(), TempImageData.Num() * TempImageData.GetTypeSize(), ImageSize.X, ImageSize.Y, ERGBFormat::BGRA, 8) == false)
		{
			return false;
		}
	}

	const TArray64<uint8> TempData = ImageWrapper->GetCompressed();
	return FFileHelper::SaveArrayToFile(TempData, FilePath.GetData());
}

void FNiagaraBakerRenderer::CreatePreviewScene(UNiagaraSystem* NiagaraSystem, UNiagaraComponent*& OutComponent, TSharedPtr<FAdvancedPreviewScene>& OutPreviewScene)
{
	check(NiagaraSystem);
	OutComponent = NewObject<UNiagaraComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	OutComponent->CastShadow = 1;
	OutComponent->bCastDynamicShadow = 1;
	OutComponent->SetAllowScalability(false);
	OutComponent->SetAsset(NiagaraSystem);
	OutComponent->SetForceSolo(true);
	OutComponent->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
	OutComponent->SetCanRenderWhileSeeking(true);
	OutComponent->SetMaxSimTime(0.0f);
	OutComponent->Activate(true);

	OutPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	OutPreviewScene->SetFloorVisibility(false);
	OutPreviewScene->AddComponent(OutComponent, OutComponent->GetRelativeTransform());
}

void FNiagaraBakerRenderer::DestroyPreviewScene(UNiagaraComponent*& InOutComponent, TSharedPtr<FAdvancedPreviewScene>& InOutPreviewScene)
{
	if ( InOutPreviewScene && InOutComponent )
	{
		InOutPreviewScene->RemoveComponent(InOutComponent);
		InOutPreviewScene = nullptr;
	}

	if ( InOutComponent )
	{
		InOutComponent->DestroyComponent();
		InOutComponent = nullptr;
	}
}
bool FNiagaraBakerRenderer::ExportVolume(FStringView FilePath, FIntVector ImageSize, TArrayView<FFloat16Color> ImageData)
{
	const FString FileExtension = FPaths::GetExtension(FilePath.GetData(), true);
	if (FileExtension == TEXT(".vdb"))
	{
#if PLATFORM_WINDOWS
		return OpenVDBTools::WriteImageDataToOpenVDBFile(FilePath, ImageSize, ImageData, false);
#else
		return false;
#endif
	}
	else
	{
		return ExportImage(FilePath, FIntPoint(ImageSize.X, ImageSize.Y * ImageSize.Z), ImageData);
	}
}
