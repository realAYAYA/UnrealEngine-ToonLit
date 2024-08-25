// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParameterCollection.h"
#include "UObject/UObjectIterator.h"
#include "RenderingThread.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "MaterialShared.h"
#include "MaterialCachedData.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/App.h"

int32 GDeferUpdateRenderStates = 1;
FAutoConsoleVariableRef CVarDeferUpdateRenderStates(
	TEXT("r.DeferUpdateRenderStates"),
	GDeferUpdateRenderStates,
	TEXT("Whether to defer updating the render states of material parameter collections when a paramter is changed until a rendering command needs them up to date.  Deferring updates is more efficient because multiple SetVectorParameterValue and SetScalarParameterValue calls in a frame will only result in one update."),
	ECVF_RenderThreadSafe
);

TMultiMap<FGuid, FMaterialParameterCollectionInstanceResource*> GDefaultMaterialParameterCollectionInstances;

UMaterialParameterCollection::UMaterialParameterCollection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ReleasedByRT(true)
{
	DefaultResource = nullptr;
}

void UMaterialParameterCollection::PostInitProperties()
{
	Super::PostInitProperties();

	if (LIKELY(!HasAnyFlags(RF_ClassDefaultObject) && FApp::CanEverRender()))
	{
		DefaultResource = new FMaterialParameterCollectionInstanceResource();
	}
}

void UMaterialParameterCollection::PostLoad()
{
	Super::PostLoad();
	
	if (!StateId.IsValid())
	{
		StateId = FGuid::NewGuid();
	}

	CreateBufferStruct();
	SetupWorldParameterCollectionInstances();
	UpdateDefaultResource(true);
}

void UMaterialParameterCollection::SetupWorldParameterCollectionInstances()
{
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* CurrentWorld = *It;
		ULevel* Level = CurrentWorld->PersistentLevel;
		const bool bIsWorldPartitionRuntimeCell = Level && Level->IsWorldPartitionRuntimeCell();
		if (!bIsWorldPartitionRuntimeCell)
		{
			CurrentWorld->AddParameterCollectionInstance(this, true);
		}
	}
}

void UMaterialParameterCollection::BeginDestroy()
{
	if (DefaultResource)
	{
		ReleasedByRT = false;

		FMaterialParameterCollectionInstanceResource* Resource = DefaultResource;
		FGuid Id = StateId;
		FThreadSafeBool* Released = &ReleasedByRT;
		ENQUEUE_RENDER_COMMAND(RemoveDefaultResourceCommand)(
			[Resource, Id, Released](FRHICommandListImmediate& RHICmdList)
			{	
				GDefaultMaterialParameterCollectionInstances.RemoveSingle(Id, Resource);
				*Released = true;
			}
		);
	}

	Super::BeginDestroy();
}

bool UMaterialParameterCollection::IsReadyForFinishDestroy()
{
	bool bIsReady = Super::IsReadyForFinishDestroy();
	return bIsReady && ReleasedByRT;
}

void UMaterialParameterCollection::FinishDestroy()
{
	if (DefaultResource)
	{
		DefaultResource->GameThread_Destroy();
		DefaultResource = nullptr;
	}

	Super::FinishDestroy();
}

#if WITH_EDITOR

template<typename ParameterType>
FName CreateUniqueName(TArray<ParameterType>& Parameters, int32 RenameParameterIndex)
{
	FString RenameString;
	Parameters[RenameParameterIndex].ParameterName.ToString(RenameString);

	int32 NumberStartIndex = RenameString.FindLastCharByPredicate([](TCHAR Letter){ return !FChar::IsDigit(Letter); }) + 1;
	
	int32 RenameNumber = 0;
	if (NumberStartIndex < RenameString.Len() - 1)
	{
		FString RenameStringNumberPart = RenameString.RightChop(NumberStartIndex);
		ensure(RenameStringNumberPart.IsNumeric());

		TTypeFromString<int32>::FromString(RenameNumber, *RenameStringNumberPart);
	}

	FString BaseString = RenameString.Left(NumberStartIndex);

	FName Renamed = FName(*FString::Printf(TEXT("%s%u"), *BaseString, ++RenameNumber));

	bool bMatchFound = false;
	
	do
	{
		bMatchFound = false;

		for (int32 i = 0; i < Parameters.Num(); ++i)
		{
			if (Parameters[i].ParameterName == Renamed && RenameParameterIndex != i)
			{
				Renamed = FName(*FString::Printf(TEXT("%s%u"), *BaseString, ++RenameNumber));
				bMatchFound = true;
				break;
			}
		}
	} while (bMatchFound);
	
	return Renamed;
}

template<typename ParameterType>
void SanitizeParameters(TArray<ParameterType>& Parameters)
{
	for (int32 i = 0; i < Parameters.Num() - 1; ++i)
	{
		for (int32 j = i + 1; j < Parameters.Num(); ++j)
		{
			if (Parameters[i].Id == Parameters[j].Id)
			{
				FPlatformMisc::CreateGuid(Parameters[j].Id);
			}

			if (Parameters[i].ParameterName == Parameters[j].ParameterName)
			{
				Parameters[j].ParameterName = CreateUniqueName(Parameters, j);
			}
		}
	}
}

bool UMaterialParameterCollection::SetScalarParameterDefaultValueByInfo(FCollectionScalarParameter ScalarParameter)
{
	if(GetScalarParameterByName(ScalarParameter.ParameterName))
	{
		// if the input parameter exists, pass the name and value down to SetScalarParameterDefaultValue
		// since we want to preserve the Guid of the parameter that's already on the asset
		return SetScalarParameterDefaultValue(ScalarParameter.ParameterName, ScalarParameter.DefaultValue);
	}
	// otherwise, return false
	return false;
}

bool UMaterialParameterCollection::SetScalarParameterDefaultValue(FName ParameterName, const float Value)
{
	// make sure the input parameter name exists in the array
	const int32 ParameterIndex = GetScalarParameterIndexByName(ParameterName);
	if(ParameterIndex == -1)
	{
		return false;
	}
	// if so, change the value on the parameter in the array itself to maintain its GUID
	ScalarParameters[ParameterIndex].DefaultValue = Value;

	// and return a positive result
	return true;
}

bool UMaterialParameterCollection::SetVectorParameterDefaultValueByInfo(FCollectionVectorParameter VectorParameter)
{
	if(GetScalarParameterByName(VectorParameter.ParameterName))
    {
    	// if the input parameter exists, pass the name and value down to SetVectorParameterDefaultValue
    	// since we want to preserve the Guid of the parameter that's already on the asset
    	return SetVectorParameterDefaultValue(VectorParameter.ParameterName, VectorParameter.DefaultValue);
    }
	// otherwise, return false
    return false;
}

bool UMaterialParameterCollection::SetVectorParameterDefaultValue(FName ParameterName, const FLinearColor& Value)
{
	// make sure the input parameter name exists in the array
	const int32 ParameterIndex = GetVectorParameterIndexByName(ParameterName);
	if(ParameterIndex == -1)
	{
		return false;
	}
	// if so, change the value on the parameter in the array itself to maintain its GUID
	VectorParameters[ParameterIndex].DefaultValue = Value;

	// and return a positive result
	return true;
}

int32 PreviousNumScalarParameters = 0;
int32 PreviousNumVectorParameters = 0;

void UMaterialParameterCollection::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	PreviousNumScalarParameters = ScalarParameters.Num();
	PreviousNumVectorParameters = VectorParameters.Num();
}

void UMaterialParameterCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SanitizeParameters(ScalarParameters);
	SanitizeParameters(VectorParameters);

	// If the array counts have changed, an element has been added or removed, and we need to update the uniform buffer layout,
	// Which also requires recompiling any referencing materials
	if (ScalarParameters.Num() != PreviousNumScalarParameters || VectorParameters.Num() != PreviousNumVectorParameters)
	{
		// Limit the count of parameters to fit within uniform buffer limits
		const uint32 MaxScalarParameters = 1024;

		if (ScalarParameters.Num() > MaxScalarParameters)
		{
			ScalarParameters.RemoveAt(MaxScalarParameters, ScalarParameters.Num() - MaxScalarParameters);
		}

		const uint32 MaxVectorParameters = 1024;

		if (VectorParameters.Num() > MaxVectorParameters)
		{
			VectorParameters.RemoveAt(MaxVectorParameters, VectorParameters.Num() - MaxVectorParameters);
		}

		// Generate a new Id so that unloaded materials that reference this collection will update correctly on load
		// Now that we changed the guid, we must recompile all materials which reference this collection
		StateId = FGuid::NewGuid();

		// Update the uniform buffer layout
		CreateBufferStruct();

		// Create a material update context so we can safely update materials using this parameter collection.
		{
			FMaterialUpdateContext UpdateContext;

			// Go through all materials in memory and recompile them if they use this material parameter collection
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;

				bool bRecompile = false;

				// Preview materials often use expressions for rendering that are not in their Expressions array, 
				// And therefore their MaterialParameterCollectionInfos are not up to date.
				if (CurrentMaterial->bIsPreviewMaterial || CurrentMaterial->bIsFunctionPreviewMaterial)
				{
					bRecompile = true;
				}
				else
				{
					for (int32 FunctionIndex = 0; FunctionIndex < CurrentMaterial->GetCachedExpressionData().ParameterCollectionInfos.Num() && !bRecompile; FunctionIndex++)
					{
						if (CurrentMaterial->GetCachedExpressionData().ParameterCollectionInfos[FunctionIndex].ParameterCollection == this)
						{
							bRecompile = true;
							break;
						}
					}
				}

				if (bRecompile)
				{
					UpdateContext.AddMaterial(CurrentMaterial);

					// Propagate the change to this material
					CurrentMaterial->PreEditChange(nullptr);
					CurrentMaterial->PostEditChange();
					CurrentMaterial->MarkPackageDirty();
				}
			}

			// Recreate all uniform buffers based off of this collection
			for (TObjectIterator<UWorld> It; It; ++It)
			{
				UWorld* CurrentWorld = *It;
				CurrentWorld->UpdateParameterCollectionInstances(true, true);
			}

			UpdateDefaultResource(true);
		}
	}
	else
	{
		// We didn't need to recreate the uniform buffer, just update its contents
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* CurrentWorld = *It;
			CurrentWorld->UpdateParameterCollectionInstances(true, false);
		}

		UpdateDefaultResource(false);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

int32 UMaterialParameterCollection::GetScalarParameterIndexByName(FName ParameterName) const
{
	// loop over all the available scalar parameters and look for a name match
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		if(ScalarParameters[ParameterIndex].ParameterName == ParameterName)
		{
			return ParameterIndex;
		}
	}
	// if not found, return -1
	return -1;
}

int32 UMaterialParameterCollection::GetVectorParameterIndexByName(FName ParameterName) const
{
	// loop over all the available vector parameters and look for a name match
	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		if(VectorParameters[ParameterIndex].ParameterName == ParameterName)
		{
			return ParameterIndex;
		}
	}
	// if not found, return -1
	return -1;
}

TArray<FName> UMaterialParameterCollection::GetScalarParameterNames() const
{
	TArray<FName> Names;
	GetParameterNames(Names, false);
	return Names;
}

TArray<FName> UMaterialParameterCollection::GetVectorParameterNames() const
{
	TArray<FName> Names;
	GetParameterNames(Names, true);
	return Names;
}

float UMaterialParameterCollection::GetScalarParameterDefaultValue(FName ParameterName, bool& bParameterFound) const
{
	const int32 ParameterIndex = GetScalarParameterIndexByName(ParameterName);
	bParameterFound = true;
	if(ParameterIndex == -1)
	{
		bParameterFound = false;
		return 0.0;
	}
	
	return ScalarParameters[ParameterIndex].DefaultValue;
}

FLinearColor UMaterialParameterCollection::GetVectorParameterDefaultValue(FName ParameterName, bool& bParameterFound) const
{
	const int32 ParameterIndex = GetVectorParameterIndexByName(ParameterName);
	bParameterFound = true;
	if(ParameterIndex == -1)
	{
		bParameterFound = false;
		return FLinearColor::Black;
	}
	
	return VectorParameters[ParameterIndex].DefaultValue;
}


FName UMaterialParameterCollection::GetParameterName(const FGuid& Id) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			return Parameter.ParameterName;
		}
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			return Parameter.ParameterName;
		}
	}

	return NAME_None;
}

FGuid UMaterialParameterCollection::GetParameterId(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return Parameter.Id;
		}
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return Parameter.Id;
		}
	}

	return FGuid();
}

void UMaterialParameterCollection::GetParameterIndex(const FGuid& Id, int32& OutIndex, int32& OutComponentIndex) const
{
	// The parameter and component index allocated in this function must match the memory layout in UMaterialParameterCollectionInstance::GetParameterData

	OutIndex = -1;
	OutComponentIndex = -1;

	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			// Scalar parameters are packed into float4's
			OutIndex = ParameterIndex / 4;
			OutComponentIndex = ParameterIndex % 4;
			break;
		}
	}

	const int32 VectorParameterBase = FMath::DivideAndRoundUp(ScalarParameters.Num(), 4);

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			OutIndex = ParameterIndex + VectorParameterBase;
			break;
		}
	}
}

void UMaterialParameterCollection::GetParameterNames(TArray<FName>& OutParameterNames, bool bVectorParameters) const
{
	if (bVectorParameters)
	{
		for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
		{
			const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];
			OutParameterNames.Add(Parameter.ParameterName);
		}
	}
	else
	{
		for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
		{
			const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];
			OutParameterNames.Add(Parameter.ParameterName);
		}
	}
}

const FCollectionScalarParameter* UMaterialParameterCollection::GetScalarParameterByName(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return &Parameter;
		}
	}

	return nullptr;
}

const FCollectionVectorParameter* UMaterialParameterCollection::GetVectorParameterByName(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return &Parameter;
		}
	}

	return nullptr;
}

void UMaterialParameterCollection::CreateBufferStruct()
{	
	if (UNLIKELY(!FApp::CanEverRenderOrProduceRenderData()))
	{
		return;
	}

	TArray<FShaderParametersMetadata::FMember> Members;
	uint32 NextMemberOffset = 0;

	const uint32 NumVectors = FMath::DivideAndRoundUp(ScalarParameters.Num(), 4) + VectorParameters.Num();
	new(Members) FShaderParametersMetadata::FMember(TEXT("Vectors"),TEXT(""),__LINE__,NextMemberOffset,UBMT_FLOAT32,EShaderPrecisionModifier::Half,1,4,NumVectors, nullptr);
	const uint32 VectorArraySize = NumVectors * sizeof(FVector4f);
	NextMemberOffset += VectorArraySize;
	const uint32 StructSize = Align(NextMemberOffset, SHADER_PARAMETER_STRUCT_ALIGNMENT);

	// If Collections ever get non-numeric resources (eg Textures), OutEnvironment.ResourceTableMap has a map by name
	// and the N ParameterCollection Uniform Buffers ALL are named "MaterialCollection" with different hashes!
	// (and the hlsl cbuffers are named MaterialCollection0, etc, so the names don't match the layout)
	UniformBufferStruct = MakeUnique<FShaderParametersMetadata>(
		FShaderParametersMetadata::EUseCase::DataDrivenUniformBuffer,
		EUniformBufferBindingFlags::Shader,
		TEXT("MaterialCollection"),
		TEXT("MaterialCollection"),
		TEXT("MaterialCollection"),
		nullptr,
		__FILE__,
		__LINE__,
		StructSize,
		Members
		);
}

void UMaterialParameterCollection::GetDefaultParameterData(TArray<FVector4f>& ParameterData) const
{
	// The memory layout created here must match the index assignment in UMaterialParameterCollection::GetParameterIndex

	ParameterData.Empty(FMath::DivideAndRoundUp(ScalarParameters.Num(), 4) + VectorParameters.Num());

	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		// Add a new vector for each packed vector
		if (ParameterIndex % 4 == 0)
		{
			ParameterData.Add(FVector4f(0, 0, 0, 0));
		}

		FVector4f& CurrentVector = ParameterData.Last();
		// Pack into the appropriate component of this packed vector
		CurrentVector[ParameterIndex % 4] = Parameter.DefaultValue;
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];
		ParameterData.Add(FVector4f(Parameter.DefaultValue));
	}
}

void UMaterialParameterCollection::UpdateDefaultResource(bool bRecreateUniformBuffer)
{
	if (UNLIKELY(!FApp::CanEverRender()))
	{
		return;
	}

	// Propagate the new values to the rendering thread
	TArray<FVector4f> ParameterData;
	GetDefaultParameterData(ParameterData);
	DefaultResource->GameThread_UpdateContents(StateId, ParameterData, GetFName(), bRecreateUniformBuffer);

	FGuid Id = StateId;
	FMaterialParameterCollectionInstanceResource* Resource = DefaultResource;
	ENQUEUE_RENDER_COMMAND(UpdateDefaultResourceCommand)(
		[Id, Resource](FRHICommandListImmediate& RHICmdList)
		{	
			GDefaultMaterialParameterCollectionInstances.Add(Id, Resource);
		}
	);
}

UMaterialParameterCollectionInstance::UMaterialParameterCollectionInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Resource = nullptr;
	bNeedsRenderStateUpdate = false;
}

void UMaterialParameterCollectionInstance::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject) && FApp::CanEverRender())
	{
		Resource = new FMaterialParameterCollectionInstanceResource();
	}
}

void UMaterialParameterCollectionInstance::SetCollection(UMaterialParameterCollection* InCollection, UWorld* InWorld)
{
	Collection = InCollection;
	World = InWorld;
}

bool UMaterialParameterCollectionInstance::SetScalarParameterValue(FName ParameterName, float ParameterValue)
{
	if (!World.IsValid())
	{
		return false;
	}

	check(Collection.IsValid());

	if (Collection->GetScalarParameterByName(ParameterName))
	{
		float* ExistingValue = ScalarParameterValues.Find(ParameterName);
		bool bUpdateUniformBuffer = false;

		if (ExistingValue && *ExistingValue != ParameterValue)
		{
			// Update the existing instance override if the new value is different
			bUpdateUniformBuffer = true;
			*ExistingValue = ParameterValue;
		}
		else if (!ExistingValue)
		{
			// Add a new instance override
			bUpdateUniformBuffer = true;
			ScalarParameterValues.Add(ParameterName, ParameterValue);
		}

		if (bUpdateUniformBuffer)
		{
			UpdateRenderState(false);
			ScalarParameterUpdatedDelegate.Broadcast(ScalarParameterUpdate(ParameterName, ParameterValue));
		}

		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::SetVectorParameterValue(FName ParameterName, const FLinearColor& ParameterValue)
{
	if (!World.IsValid())
	{
		return false;
	}

	check(Collection.IsValid());

	if (Collection->GetVectorParameterByName(ParameterName))
	{
		FLinearColor* ExistingValue = VectorParameterValues.Find(ParameterName);
		bool bUpdateUniformBuffer = false;

		if (ExistingValue && *ExistingValue != ParameterValue)
		{
			// Update the existing instance override if the new value is different
			bUpdateUniformBuffer = true;
			*ExistingValue = ParameterValue;
		}
		else if (!ExistingValue)
		{
			// Add a new instance override
			bUpdateUniformBuffer = true;
			VectorParameterValues.Add(ParameterName, ParameterValue);
		}

		if (bUpdateUniformBuffer)
		{
			UpdateRenderState(false);
			VectorParameterUpdatedDelegate.Broadcast(VectorParameterUpdate(ParameterName, ParameterValue));
		}

		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetScalarParameterValue(FName ParameterName, float& OutParameterValue) const
{
	if (const FCollectionScalarParameter* Parameter = Collection->GetScalarParameterByName(ParameterName))
	{
		return GetScalarParameterValue(*Parameter, OutParameterValue);
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetVectorParameterValue(FName ParameterName, FLinearColor& OutParameterValue) const
{
	if (const FCollectionVectorParameter* Parameter = Collection->GetVectorParameterByName(ParameterName))
	{
		return GetVectorParameterValue(*Parameter, OutParameterValue);
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetScalarParameterValue(const FCollectionScalarParameter& Parameter, float& OutParameterValue) const
{
	const float* InstanceValue = ScalarParameterValues.Find(Parameter.ParameterName);
	OutParameterValue = InstanceValue != nullptr ? *InstanceValue : Parameter.DefaultValue;
	return true;
}

bool UMaterialParameterCollectionInstance::GetVectorParameterValue(const FCollectionVectorParameter& Parameter, FLinearColor& OutParameterValue) const
{
	const FLinearColor* InstanceValue = VectorParameterValues.Find(Parameter.ParameterName);
	OutParameterValue = InstanceValue != nullptr ? *InstanceValue : Parameter.DefaultValue;
	return true;
}

void UMaterialParameterCollectionInstance::UpdateRenderState(bool bRecreateUniformBuffer)
{
	// Don't need material parameters on the server
	if (!World.IsValid() || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bNeedsRenderStateUpdate = true;
	World->SetMaterialParameterCollectionInstanceNeedsUpdate();

	if (!GDeferUpdateRenderStates || bRecreateUniformBuffer)
	{
		DeferredUpdateRenderState(bRecreateUniformBuffer);
	}
}

void UMaterialParameterCollectionInstance::DeferredUpdateRenderState(bool bRecreateUniformBuffer)
{
	checkf(bNeedsRenderStateUpdate || !bRecreateUniformBuffer, TEXT("DeferredUpdateRenderState was told to recreate the uniform buffer, but there's nothing to update"));

	if (bNeedsRenderStateUpdate && World.IsValid())
	{
		// Propagate the new values to the rendering thread
		TArray<FVector4f> ParameterData;
		GetParameterData(ParameterData);
		Resource->GameThread_UpdateContents(Collection.IsValid() ? Collection->StateId : FGuid(), ParameterData, GetFName(), bRecreateUniformBuffer);
	}

	bNeedsRenderStateUpdate = false;
}

void UMaterialParameterCollectionInstance::GetParameterData(TArray<FVector4f>& ParameterData) const
{
	// The memory layout created here must match the index assignment in UMaterialParameterCollection::GetParameterIndex

	if (Collection.IsValid())
	{
		ParameterData.Empty(FMath::DivideAndRoundUp(Collection->ScalarParameters.Num(), 4) + Collection->VectorParameters.Num());

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->ScalarParameters.Num(); ParameterIndex++)
		{
			const FCollectionScalarParameter& Parameter = Collection->ScalarParameters[ParameterIndex];

			// Add a new vector for each packed vector
			if (ParameterIndex % 4 == 0)
			{
				ParameterData.Add(FVector4f(0, 0, 0, 0));
			}

			FVector4f& CurrentVector = ParameterData.Last();
			const float* InstanceData = ScalarParameterValues.Find(Parameter.ParameterName);
			// Pack into the appropriate component of this packed vector
			CurrentVector[ParameterIndex % 4] = InstanceData ? *InstanceData : Parameter.DefaultValue;
		}

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->VectorParameters.Num(); ParameterIndex++)
		{
			const FCollectionVectorParameter& Parameter = Collection->VectorParameters[ParameterIndex];
			const FLinearColor* InstanceData = VectorParameterValues.Find(Parameter.ParameterName);
			ParameterData.Add(InstanceData ? FVector4f(*InstanceData) : FVector4f(Parameter.DefaultValue));
		}
	}
}

void UMaterialParameterCollectionInstance::FinishDestroy()
{
	if (Resource)
	{
		Resource->GameThread_Destroy();
		Resource = nullptr;
	}

	Super::FinishDestroy();
}

void FMaterialParameterCollectionInstanceResource::GameThread_UpdateContents(const FGuid& InGuid, const TArray<FVector4f>& Data, const FName& InOwnerName, bool bRecreateUniformBuffer)
{
	if (UNLIKELY(!FApp::CanEverRender()))
	{
		return;
	}

	FMaterialParameterCollectionInstanceResource* Resource = this;
	ENQUEUE_RENDER_COMMAND(UpdateCollectionCommand)(
		[InGuid, Data, InOwnerName, Resource, bRecreateUniformBuffer](FRHICommandListImmediate& RHICmdList)
		{
			Resource->UpdateContents(InGuid, Data, InOwnerName, bRecreateUniformBuffer);
		}
	);
}

void FMaterialParameterCollectionInstanceResource::GameThread_Destroy()
{
	FMaterialParameterCollectionInstanceResource* Resource = this;
	ENQUEUE_RENDER_COMMAND(DestroyCollectionCommand)(
		[Resource](FRHICommandListImmediate& RHICmdList)
		{
			Resource->UniformBuffer.SafeRelease();

			// FRHIUniformBuffer instances take raw pointers to the layout struct.
			// Delete the resource instance (and its layout) on the RHI thread to avoid deleting the layout
			// whilst the RHI is using it, and also avoid having to completely flush the RHI thread.
			RHICmdList.EnqueueLambda([Resource](FRHICommandListImmediate&)
			{
				delete Resource;
			});
		}
	);
}

FMaterialParameterCollectionInstanceResource::FMaterialParameterCollectionInstanceResource() = default;

FMaterialParameterCollectionInstanceResource::~FMaterialParameterCollectionInstanceResource()
{
	check(!UniformBuffer.IsValid());
}

void FMaterialParameterCollectionInstanceResource::UpdateContents(const FGuid& InId, const TArray<FVector4f>& Data, const FName& InOwnerName, bool bRecreateUniformBuffer)
{
	Id = InId;
	OwnerName = InOwnerName;

	if (InId != FGuid() && Data.Num() > 0)
	{
		const uint32 NewSize = Data.GetTypeSize() * Data.Num();
		check(UniformBufferLayout == nullptr || UniformBufferLayout->Resources.Num() == 0);

		if (!bRecreateUniformBuffer && IsValidRef(UniformBuffer))
		{
			check(NewSize == UniformBufferLayout->ConstantBufferSize);
			check(UniformBuffer->GetLayoutPtr() == UniformBufferLayout);
			FRHICommandListImmediate::Get().UpdateUniformBuffer(UniformBuffer, Data.GetData());
		}
		else
		{
			FRHIUniformBufferLayoutInitializer UniformBufferLayoutInitializer(TEXT("MaterialParameterCollectionInstanceResource"));
			UniformBufferLayoutInitializer.ConstantBufferSize = NewSize;
			UniformBufferLayoutInitializer.ComputeHash();

			UniformBufferLayout = RHICreateUniformBufferLayout(UniformBufferLayoutInitializer);

			UniformBuffer = RHICreateUniformBuffer(Data.GetData(), UniformBufferLayout, UniformBuffer_MultiFrame);
		}
	}
}
