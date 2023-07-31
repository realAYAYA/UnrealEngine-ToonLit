// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceGeometryCollection.h"
#include "NiagaraRenderer.h"
#include "NiagaraSimStageData.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "ShaderParameterUtils.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceGeometryCollection)

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceGeometryCollection"
DEFINE_LOG_CATEGORY_STATIC(LogGeometryCollection, Log, All);

//------------------------------------------------------------------------------------------------------------

namespace NDIGeometryCollectionLocal
{
	static const FName GetClosestPointNoNormalName(TEXT("GetClosestPointNoNormal"));
	static const TCHAR* TemplateShaderFilePath = TEXT("/Plugin/Experimental/ChaosNiagara/NiagaraDataInterfaceGeometryCollection.ush");

	template<typename BufferType, EPixelFormat PixelFormat>
	void CreateInternalBuffer(FReadBuffer& OutputBuffer, uint32 ElementCount)
	{
		if (ElementCount > 0)
		{
			OutputBuffer.Initialize(TEXT("FNDIGeometryCollectionBuffer"), sizeof(BufferType), ElementCount, PixelFormat, BUF_Static);
		}
	}

	template<typename BufferType, EPixelFormat PixelFormat>
	void UpdateInternalBuffer(const TArray<BufferType>& InputData, FReadBuffer& OutputBuffer)
	{
		uint32 ElementCount = InputData.Num();
		if (ElementCount > 0 && OutputBuffer.Buffer.IsValid())
		{
			const uint32 BufferBytes = sizeof(BufferType) * ElementCount;

			void* OutputData = RHILockBuffer(OutputBuffer.Buffer, 0, BufferBytes, RLM_WriteOnly);

			FMemory::Memcpy(OutputData, InputData.GetData(), BufferBytes);
			RHIUnlockBuffer(OutputBuffer.Buffer);
		}
	}
}

//------------------------------------------------------------------------------------------------------------

void FNDIGeometryCollectionBuffer::InitRHI()
{
	NDIGeometryCollectionLocal::CreateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(WorldTransformBuffer, 3 * NumPieces);
	NDIGeometryCollectionLocal::CreateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(PrevWorldTransformBuffer, 3 * NumPieces);

	NDIGeometryCollectionLocal::CreateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(WorldInverseTransformBuffer, 3 * NumPieces);
	NDIGeometryCollectionLocal::CreateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(PrevWorldInverseTransformBuffer, 3 * NumPieces);

	NDIGeometryCollectionLocal::CreateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(BoundsBuffer, NumPieces);
}

void FNDIGeometryCollectionBuffer::ReleaseRHI()
{
	WorldTransformBuffer.Release();
	PrevWorldTransformBuffer.Release();
	WorldInverseTransformBuffer.Release();
	PrevWorldInverseTransformBuffer.Release();
	BoundsBuffer.Release();
}

//------------------------------------------------------------------------------------------------------------



void FNDIGeometryCollectionData::Release()
{
	if (AssetBuffer)
	{
		BeginReleaseResource(AssetBuffer);
		ENQUEUE_RENDER_COMMAND(DeleteResource)(
			[ParamPointerToRelease = AssetBuffer](FRHICommandListImmediate& RHICmdList)
			{
				delete ParamPointerToRelease;
			});
		AssetBuffer = nullptr;
	}
}

void FNDIGeometryCollectionData::Init(UNiagaraDataInterfaceGeometryCollection* Interface, FNiagaraSystemInstance* SystemInstance)
{
	AssetBuffer = nullptr;

	if (Interface != nullptr && SystemInstance != nullptr)
	{		
		if (Interface->GeometryCollectionActor != nullptr && Interface->GeometryCollectionActor->GetGeometryCollectionComponent() != nullptr &&
			Interface->GeometryCollectionActor->GetGeometryCollectionComponent()->RestCollection != nullptr)
		{			
			const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe>
				Collection = Interface->GeometryCollectionActor->GetGeometryCollectionComponent()->RestCollection->GetGeometryCollection();
			const TManagedArray<FBox>& BoundingBoxes = Collection->BoundingBox;
			const TManagedArray<int32>& TransformIndex = Collection->TransformIndex;
			const TManagedArray<FTransform>& Transforms = Interface->GeometryCollectionActor->GetGeometryCollectionComponent()->GetTransformArray();
			const TManagedArray<TSet<int32>>& Children = Collection->Children;
			const TManagedArray<int32>& TransformIndexArray = Collection->TransformIndex;

			int NumPieces = 0;
			
			for (int i = 0; i < BoundingBoxes.Num(); ++i)
			{
				int32 CurrTransformIndex = TransformIndexArray[i];

				// only consider leaf geometry
				if (Collection->Children[CurrTransformIndex].Num() == 0)
				{
					NumPieces++;
				}
			}

			AssetArrays = new FNDIGeometryCollectionArrays();
			AssetArrays->Resize(NumPieces);

			AssetBuffer = new FNDIGeometryCollectionBuffer();
			AssetBuffer->SetNumPieces(NumPieces);
			BeginInitResource(AssetBuffer);

			FVector Origin;
			FVector Extents;
			Interface->GeometryCollectionActor->GetActorBounds(false, Origin, Extents, true);

			BoundsOrigin = (FVector3f)Origin;	// LWC_TODO: Precision Loss
			BoundsExtent = (FVector3f)Extents;	// LWC_TODO: Precision Loss

			int PieceIndex = 0;
			for (int i = 0; i < BoundingBoxes.Num(); ++i)
			{
				int32 CurrTransformIndex = TransformIndexArray[i];

				// only consider leaf geometry
				if (Collection->Children[CurrTransformIndex].Num() == 0)
				{
					FBox CurrBox = BoundingBoxes[i];
					FVector3f BoxSize = FVector3f(CurrBox.Max - CurrBox.Min);
					AssetArrays->BoundsBuffer[PieceIndex] = FVector4f(BoxSize.X, BoxSize.Y, BoxSize.Z, 0);

					PieceIndex++;
				}
			}
		}
		else
		{
			AssetArrays = new FNDIGeometryCollectionArrays();
			AssetArrays->Resize(1);

			AssetBuffer = new FNDIGeometryCollectionBuffer();
			AssetBuffer->SetNumPieces(1);
			BeginInitResource(AssetBuffer);
		}
	}

}

void FNDIGeometryCollectionData::Update(UNiagaraDataInterfaceGeometryCollection* Interface, FNiagaraSystemInstance* SystemInstance)
{
	if (Interface != nullptr && SystemInstance != nullptr)
	{		
		TickingGroup = ComputeTickingGroup();
		
		if (Interface->GeometryCollectionActor != nullptr && Interface->GeometryCollectionActor->GetGeometryCollectionComponent() != nullptr &&
			Interface->GeometryCollectionActor->GetGeometryCollectionComponent()->RestCollection != nullptr)
		{			
			const FTransform ActorTransform = Interface->GeometryCollectionActor->GetTransform();

			const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe>
				Collection = Interface->GeometryCollectionActor->GetGeometryCollectionComponent()->RestCollection->GetGeometryCollection();
			const TManagedArray<FBox>& BoundingBoxes = Collection->BoundingBox;
			const TManagedArray<int32>& TransformIndexArray = Collection->TransformIndex;
			const TManagedArray<FTransform>& Transforms = Interface->GeometryCollectionActor->GetGeometryCollectionComponent()->GetTransformArray();
			const TArray<FMatrix>& GlobalMatrices = Interface->GeometryCollectionActor->GetGeometryCollectionComponent()->GetGlobalMatrices();
			const TManagedArray<TSet<int32>>& Children = Collection->Children;
			
			int NumPieces = 0;

			for (int i = 0; i < BoundingBoxes.Num(); ++i)
			{
				int32 CurrTransformIndex = TransformIndexArray[i];

				// only consider leaf geometry
				if (Collection->Children[CurrTransformIndex].Num() == 0)
				{
					NumPieces++;
				}
			}

			if (GlobalMatrices.Num() != Transforms.Num())
			{
				return;
			}

			if (NumPieces != AssetArrays->BoundsBuffer.Num())
			{
				Init(Interface, SystemInstance);
			}
			
			FVector Origin;
			FVector Extents;
			Interface->GeometryCollectionActor->GetActorBounds(false, Origin, Extents, true);

			BoundsOrigin = (FVector3f)Origin;
			BoundsExtent = (FVector3f)Extents;

			int PieceIndex = 0;
			for (int i = 0; i < BoundingBoxes.Num(); ++i)
			{
				int32 CurrTransformIndex = TransformIndexArray[i];
				if (Collection->Children[CurrTransformIndex].Num() == 0)
				{
					int32 TransformIndex = 3 * PieceIndex;
					AssetArrays->PrevWorldInverseTransformBuffer[TransformIndex] = AssetArrays->WorldInverseTransformBuffer[TransformIndex];
					AssetArrays->PrevWorldInverseTransformBuffer[TransformIndex + 1] = AssetArrays->WorldInverseTransformBuffer[TransformIndex + 1];
					AssetArrays->PrevWorldInverseTransformBuffer[TransformIndex + 2] = AssetArrays->WorldInverseTransformBuffer[TransformIndex + 2];

					AssetArrays->PrevWorldTransformBuffer[TransformIndex] = AssetArrays->WorldTransformBuffer[TransformIndex];
					AssetArrays->PrevWorldTransformBuffer[TransformIndex + 1] = AssetArrays->WorldTransformBuffer[TransformIndex + 1];
					AssetArrays->PrevWorldTransformBuffer[TransformIndex + 2] = AssetArrays->WorldTransformBuffer[TransformIndex + 2];

					FBox CurrBox = BoundingBoxes[i];

					// #todo(dmp): save this somewhere in an array?
					FVector LocalTranslation = (CurrBox.Max + CurrBox.Min) * .5;
					FTransform LocalOffset(LocalTranslation);



					FMatrix44f CurrTransform = FMatrix44f(LocalOffset.ToMatrixWithScale() * GlobalMatrices[CurrTransformIndex] * ActorTransform.ToMatrixWithScale());
					CurrTransform.To3x4MatrixTranspose(&AssetArrays->WorldTransformBuffer[TransformIndex].X);

					FMatrix44f CurrInverse = CurrTransform.Inverse();
					CurrInverse.To3x4MatrixTranspose(&AssetArrays->WorldInverseTransformBuffer[TransformIndex].X);

					PieceIndex++;
				}
			}
		}		
	}
}

ETickingGroup FNDIGeometryCollectionData::ComputeTickingGroup()
{
	TickingGroup = NiagaraFirstTickGroup;
	return TickingGroup;
}

//------------------------------------------------------------------------------------------------------------

void FNDIGeometryCollectionProxy::ConsumePerInstanceDataFromGameThread(void* DataFromGameThread, const FNiagaraSystemInstanceID& Instance)
{
	check(IsInRenderingThread());

	FNDIGeometryCollectionData* SourceData = static_cast<FNDIGeometryCollectionData*>(DataFromGameThread);
	FNDIGeometryCollectionData* TargetData = &(SystemInstancesToProxyData.FindOrAdd(Instance));

	ensure(TargetData);
	if (TargetData)
	{
		TargetData->AssetBuffer = SourceData->AssetBuffer;		
		TargetData->AssetArrays = SourceData->AssetArrays;
		TargetData->TickingGroup = SourceData->TickingGroup;
		TargetData->BoundsOrigin = SourceData->BoundsOrigin;
		TargetData->BoundsExtent = SourceData->BoundsExtent;
	}
	else
	{
		UE_LOG(LogGeometryCollection, Log, TEXT("ConsumePerInstanceDataFromGameThread() ... could not find %d"), Instance);
	}
	SourceData->~FNDIGeometryCollectionData();
}

void FNDIGeometryCollectionProxy::InitializePerInstanceData(const FNiagaraSystemInstanceID& SystemInstance)
{
	check(IsInRenderingThread());

	check(!SystemInstancesToProxyData.Contains(SystemInstance));
	FNDIGeometryCollectionData* TargetData = &SystemInstancesToProxyData.Add(SystemInstance);
}

void FNDIGeometryCollectionProxy::DestroyPerInstanceData(const FNiagaraSystemInstanceID& SystemInstance)
{
	check(IsInRenderingThread());	

	SystemInstancesToProxyData.Remove(SystemInstance);
}

void FNDIGeometryCollectionProxy::PreStage(const FNDIGpuComputePreStageContext& Context)
{
	check(SystemInstancesToProxyData.Contains(Context.GetSystemInstanceID()));

	FNDIGeometryCollectionData* ProxyData = SystemInstancesToProxyData.Find(Context.GetSystemInstanceID());
	if (ProxyData != nullptr && ProxyData->AssetBuffer)
	{
		if (Context.GetSimStageData().bFirstStage)
		{
			FNDIGeometryCollectionBuffer* AssetBuffer = ProxyData->AssetBuffer;

			// #todo(dmp): bounds buffer doesn't need to be updated each frame
			NDIGeometryCollectionLocal::UpdateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ProxyData->AssetArrays->WorldTransformBuffer, ProxyData->AssetBuffer->WorldTransformBuffer);
			NDIGeometryCollectionLocal::UpdateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ProxyData->AssetArrays->PrevWorldTransformBuffer, ProxyData->AssetBuffer->PrevWorldTransformBuffer);
			NDIGeometryCollectionLocal::UpdateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ProxyData->AssetArrays->WorldInverseTransformBuffer, ProxyData->AssetBuffer->WorldInverseTransformBuffer);
			NDIGeometryCollectionLocal::UpdateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ProxyData->AssetArrays->PrevWorldInverseTransformBuffer, ProxyData->AssetBuffer->PrevWorldInverseTransformBuffer);
			NDIGeometryCollectionLocal::UpdateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ProxyData->AssetArrays->BoundsBuffer, ProxyData->AssetBuffer->BoundsBuffer);
		}
	}
}

//------------------------------------------------------------------------------------------------------------

UNiagaraDataInterfaceGeometryCollection::UNiagaraDataInterfaceGeometryCollection(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)	
{
	Proxy.Reset(new FNDIGeometryCollectionProxy());
}

bool UNiagaraDataInterfaceGeometryCollection::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGeometryCollectionData* InstanceData = new (PerInstanceData) FNDIGeometryCollectionData();

	check(InstanceData);
	InstanceData->Init(this, SystemInstance);
	
	return true;
}

ETickingGroup UNiagaraDataInterfaceGeometryCollection::CalculateTickGroup(const void* PerInstanceData) const
{
	const FNDIGeometryCollectionData* InstanceData = static_cast<const FNDIGeometryCollectionData*>(PerInstanceData);

	if (InstanceData)
	{
		return InstanceData->TickingGroup;
	}
	return NiagaraFirstTickGroup;
}

void UNiagaraDataInterfaceGeometryCollection::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGeometryCollectionData* InstanceData = static_cast<FNDIGeometryCollectionData*>(PerInstanceData);

	InstanceData->Release();	
	InstanceData->~FNDIGeometryCollectionData();

	FNDIGeometryCollectionProxy* ThisProxy = GetProxyAs<FNDIGeometryCollectionProxy>();
	ENQUEUE_RENDER_COMMAND(FNiagaraDIDestroyInstanceData) (
		[ThisProxy, InstanceID = SystemInstance->GetId()](FRHICommandListImmediate& CmdList)
		{
			FNDIGeometryCollectionData* ProxyData =
				ThisProxy->SystemInstancesToProxyData.Find(InstanceID);

			if (ProxyData != nullptr && ProxyData->AssetArrays)
			{			
				ThisProxy->SystemInstancesToProxyData.Remove(InstanceID);
				delete ProxyData->AssetArrays;
			}		
		}
	);
}

bool UNiagaraDataInterfaceGeometryCollection::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float InDeltaSeconds)
{
	FNDIGeometryCollectionData* InstanceData = static_cast<FNDIGeometryCollectionData*>(PerInstanceData);
	if (InstanceData && InstanceData->AssetBuffer && SystemInstance)
	{
		InstanceData->Update(this, SystemInstance);
	}
	return false;
}

bool UNiagaraDataInterfaceGeometryCollection::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceGeometryCollection* OtherTyped = CastChecked<UNiagaraDataInterfaceGeometryCollection>(Destination);		

	OtherTyped->GeometryCollectionActor = GeometryCollectionActor;

	return true;
}

bool UNiagaraDataInterfaceGeometryCollection::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceGeometryCollection* OtherTyped = CastChecked<const UNiagaraDataInterfaceGeometryCollection>(Other);

	return  OtherTyped->GeometryCollectionActor == GeometryCollectionActor;
}

void UNiagaraDataInterfaceGeometryCollection::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceGeometryCollection::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NDIGeometryCollectionLocal::GetClosestPointNoNormalName;
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Delta Time")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Closest Distance")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Position")));		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Velocity")));

		OutFunctions.Add(Sig);
	}
}

void UNiagaraDataInterfaceGeometryCollection::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceGeometryCollection::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	if (FunctionInfo.DefinitionName == NDIGeometryCollectionLocal::GetClosestPointNoNormalName)
	{
		return true;
	}

	return false;
}

bool UNiagaraDataInterfaceGeometryCollection::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);
	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceGeometryCollectionSource"), GetShaderFileHash(NDIGeometryCollectionLocal::TemplateShaderFilePath, EShaderPlatform::SP_PCD3D_SM5).ToString());
	bSuccess &= InVisitor->UpdateShaderParameters<FShaderParameters>();
	return bSuccess;
}

void UNiagaraDataInterfaceGeometryCollection::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	const TMap<FString, FStringFormatArg> TemplateArgs = { {TEXT("ParameterName"),	ParamInfo.DataInterfaceHLSLSymbol}, };

	FString TemplateFile;
	LoadShaderSourceFile(NDIGeometryCollectionLocal::TemplateShaderFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}
#endif

void UNiagaraDataInterfaceGeometryCollection::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void UNiagaraDataInterfaceGeometryCollection::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNDIGeometryCollectionProxy& InterfaceProxy = Context.GetProxy<FNDIGeometryCollectionProxy>();
	FNDIGeometryCollectionData* ProxyData = InterfaceProxy.SystemInstancesToProxyData.Find(Context.GetSystemInstanceID());

	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();
	if (ProxyData != nullptr)
	{
		ShaderParameters->BoundsMin							= ProxyData->BoundsOrigin - ProxyData->BoundsExtent;
		ShaderParameters->BoundsMax							= ProxyData->BoundsOrigin + ProxyData->BoundsExtent;
		ShaderParameters->NumPieces							= ProxyData->AssetBuffer->NumPieces;
		ShaderParameters->WorldTransformBuffer				= FNiagaraRenderer::GetSrvOrDefaultFloat4(ProxyData->AssetBuffer->WorldTransformBuffer.SRV);
		ShaderParameters->PrevWorldTransformBuffer			= FNiagaraRenderer::GetSrvOrDefaultFloat4(ProxyData->AssetBuffer->PrevWorldTransformBuffer.SRV);
		ShaderParameters->WorldInverseTransformBuffer		= FNiagaraRenderer::GetSrvOrDefaultFloat4(ProxyData->AssetBuffer->WorldInverseTransformBuffer.SRV);
		ShaderParameters->PrevWorldInverseTransformBuffer	= FNiagaraRenderer::GetSrvOrDefaultFloat4(ProxyData->AssetBuffer->PrevWorldInverseTransformBuffer.SRV);
		ShaderParameters->BoundsBuffer						= FNiagaraRenderer::GetSrvOrDefaultFloat4(ProxyData->AssetBuffer->BoundsBuffer.SRV);
	}
	else
	{
		ShaderParameters->BoundsMin							= FVector3f::ZeroVector;
		ShaderParameters->BoundsMax							= FVector3f::ZeroVector;
		ShaderParameters->NumPieces							= 0;
		ShaderParameters->WorldTransformBuffer				= FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->PrevWorldTransformBuffer			= FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->WorldInverseTransformBuffer		= FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->PrevWorldInverseTransformBuffer	= FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->BoundsBuffer						= FNiagaraRenderer::GetDummyFloat4Buffer();
	}
}

void UNiagaraDataInterfaceGeometryCollection::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
	FNDIGeometryCollectionData* GameThreadData = static_cast<FNDIGeometryCollectionData*>(PerInstanceData);
	FNDIGeometryCollectionData* RenderThreadData = static_cast<FNDIGeometryCollectionData*>(DataForRenderThread);

	if (GameThreadData != nullptr && RenderThreadData != nullptr)
	{		
		RenderThreadData->AssetBuffer = GameThreadData->AssetBuffer;				

		RenderThreadData->AssetArrays = new FNDIGeometryCollectionArrays();
		RenderThreadData->AssetArrays->CopyFrom(GameThreadData->AssetArrays);		
		RenderThreadData->TickingGroup = GameThreadData->TickingGroup;
		RenderThreadData->BoundsOrigin = GameThreadData->BoundsOrigin;
		RenderThreadData->BoundsExtent = GameThreadData->BoundsExtent;
	}
	check(Proxy);
}

#undef LOCTEXT_NAMESPACE
