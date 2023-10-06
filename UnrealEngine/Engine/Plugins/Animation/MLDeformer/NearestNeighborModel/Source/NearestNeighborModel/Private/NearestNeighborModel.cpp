// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModel.h"
#include "NearestNeighborModelInputInfo.h"
#include "NearestNeighborModelInstance.h"
#include "NearestNeighborOptimizedNetwork.h"
#include "NearestNeighborOptimizedNetworkLoader.h"
#include "MLDeformerComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GeometryCache.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/ExternalMorphSet.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/MorphTargetVertexInfoBuffers.h"
#include "ShaderCore.h"
#include "UObject/UObjectGlobals.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(NearestNeighborModel)

#define LOCTEXT_NAMESPACE "UNearestNeighborModel"

NEARESTNEIGHBORMODEL_API DEFINE_LOG_CATEGORY(LogNearestNeighborModel)

namespace UE::NearestNeighborModel
{
	class NEARESTNEIGHBORMODEL_API FNearestNeighborModelModule
		: public IModuleInterface
	{
		void StartupModule()
		{
			FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("NearestNeighborModel"))->GetBaseDir(), TEXT("Shaders"));
			AddShaderSourceDirectoryMapping(TEXT("/Plugin/MLDeformer/NearestNeighborModel"), PluginShaderDir);
		}
	};
}
IMPLEMENT_MODULE(UE::NearestNeighborModel::FNearestNeighborModelModule, NearestNeighborModel)

namespace UE::NearestNeighborModel
{
	bool bNearestNeighborModelUseOptimizedNetwork = true;
	FAutoConsoleVariableRef CVarNearestNeighborModelUseOptimizedNetwork(
		TEXT("p.NearestNeighborModel.UseOptimizedNetwork"),
		bNearestNeighborModelUseOptimizedNetwork,
		TEXT("Whether to use the optimized network.")
	);

}

UNearestNeighborModel::UNearestNeighborModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	SetVizSettings(ObjectInitializer.CreateEditorOnlyDefaultSubobject<UNearestNeighborModelVizSettings>(this, TEXT("VizSettings")));
#endif
}

UMLDeformerInputInfo* UNearestNeighborModel::CreateInputInfo()
{
	UNearestNeighborModelInputInfo* NearestNeighborModelInputInfo = NewObject<UNearestNeighborModelInputInfo>(this);
#if WITH_EDITORONLY_DATA
	NearestNeighborModelInputInfo->InitRefBoneRotations(GetSkeletalMesh());
#endif
	return NearestNeighborModelInputInfo;
}

UMLDeformerModelInstance* UNearestNeighborModel::CreateModelInstance(UMLDeformerComponent* Component)
{
	return NewObject<UNearestNeighborModelInstance>(Component);
}

void UNearestNeighborModel::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);
}

void UNearestNeighborModel::PostLoad()
{
	Super::PostLoad();

	InitInputInfo();

#if WITH_EDITORONLY_DATA
	UpdateNetworkInputDim();
	UpdateNetworkOutputDim();
	UpdateNetworkSize();
	UpdateMorphTargetSize();
#endif
}

TArray<uint32> ReadTxt(const FString &Path)
{
	TArray<FString> StrArr;
	FFileHelper::LoadFileToStringArray(StrArr, *Path);
	TArray<uint32> UIntArr; UIntArr.SetNum(StrArr.Num());
	for (int32 i = 0; i < UIntArr.Num(); i++)
	{
		UIntArr[i] = FCString::Atoi(*StrArr[i]);
	}

	return UIntArr;
}

TArray<uint32> Range(const uint32 Start, const uint32 End)
{
	TArray<uint32> Arr; Arr.SetNum(End - Start);
	for (uint32 i = Start; i < End; i++)
	{
		Arr[i - Start] = i;
	}
	return Arr;
}

TArray<uint32> AddConstant(const TArray<uint32> &InArr, uint32 Constant)
{
	TArray<uint32> OutArr = InArr;
	for (int32 i = 0; i < InArr.Num(); i++)
	{
		OutArr[i] = InArr[i] + Constant;
	}
	return OutArr;
}

TArray<float> UNearestNeighborModel::ClipInputs(const TArray<float>& Inputs) const
{
	TArray<float> Result = Inputs;
	ClipInputs(Result.GetData(), Result.Num());
	return Result;
}

void UNearestNeighborModel::ClipInputs(float* InputPtr, int NumInputs) const
{
	if(InputsMin.Num() >= NumInputs && InputsMax.Num() >= NumInputs)
	{
		for(int32 i = 0; i < NumInputs; i++)
		{
			float Max = InputsMax[i];
			float Min = InputsMin[i];
			if (bUseInputMultipliers && i / 3 < InputMultipliers.Num())
			{
				const float Multiplier = InputMultipliers[i / 3][i % 3];
				Max *= Multiplier;
				Min *= Multiplier;	
			}

			if (InputPtr[i] > Max)
			{
				InputPtr[i] = Max;
			}
			if (InputPtr[i] < Min)
			{
				InputPtr[i] = Min;
			}
		}
	}
}

int32 UNearestNeighborModel::GetTotalNumPCACoeffs() const
{
	int32 TotalNumPCACoeffs = 0;
	for (int32 PartId = 0; PartId < GetNumParts(); PartId++)
	{
		TotalNumPCACoeffs += GetPCACoeffNum(PartId);
	}
	return TotalNumPCACoeffs;
}

int32 UNearestNeighborModel::GetTotalNumNeighbors() const
{
	int32 TotalNumNeighbors = 0;
	for (int32 PartId = 0; PartId < GetNumParts(); PartId++)
	{
		TotalNumNeighbors += GetNumNeighbors(PartId);
	}
	return TotalNumNeighbors;
}

class FMorphTargetBuffersInfo : public FMorphTargetVertexInfoBuffers
{
public:
	uint32 GetMorphDataSize() const { return MorphData.Num() * sizeof(uint32); }
	void ResetCPUData() { FMorphTargetVertexInfoBuffers::ResetCPUData(); }
};

void UNearestNeighborModel::ResetMorphBuffers()
{
	if (GetMorphTargetSet())
	{
		FMorphTargetBuffersInfo* MorphBuffersInfo = static_cast<FMorphTargetBuffersInfo*>(&GetMorphTargetSet()->MorphBuffers);
		MorphBuffersInfo->ResetCPUData();
	}
}

#if WITH_EDITORONLY_DATA
void UNearestNeighborModel::UpdatePCACoeffNums()
{
	uint32 PCACoeffStart = 0;
	for(int32 PartId = 0; PartId < GetNumParts(); PartId++)
	{
		ClothPartData[PartId].PCACoeffStart = PCACoeffStart;
		PCACoeffStart += ClothPartData[PartId].PCACoeffNum;
	}
}

void UNearestNeighborModel::UpdateNetworkInputDim()
{
	InputDim = 3 * GetBoneIncludeList().Num();
}

void UNearestNeighborModel::UpdateNetworkOutputDim()
{
	OutputDim = 0;
	for (int32 i = 0; i < GetNumParts(); i++)
	{
		OutputDim += ClothPartData[i].PCACoeffNum; 
	}
}

void UNearestNeighborModel::UpdateInputMultipliers()
{
	if (bUseInputMultipliers)
	{
		const int32 NumMultipliers = InputMultipliers.Num();
		const int32 NumBones = GetBoneIncludeList().Num();
		InputMultipliers.SetNum(NumBones);
		for (int32 Index = NumMultipliers; Index < NumBones; Index++)
		{
			InputMultipliers[Index] = FVector3f(1.f, 1.f, 1.f);
		}
	}
}

UE::NearestNeighborModel::EUpdateResult UNearestNeighborModel::UpdateVertexMap(int32 PartId, const FString& VertexMapPath, const FSkelMeshImportedMeshInfo& Info)
{
	using namespace UE::NearestNeighborModel;
	uint8 ReturnCode = EUpdateResult::SUCCESS;
	const int32 StartIndex = Info.StartImportedVertex;
	const int32 NumVertices = Info.NumVertices;
	bool bIsVertexMapValid = true;

	if (VertexMapPath.IsEmpty())
	{
		bIsVertexMapValid = false;
	}
	else if (!FPaths::FileExists(VertexMapPath))
	{
		bIsVertexMapValid = false;
		ReturnCode |= EUpdateResult::ERROR;
		UE_LOG(LogNearestNeighborModel, Error, TEXT("Part %d txt path %s does not exist"), PartId, *VertexMapPath, NumVertices);
	}
	else if (GetSkeletalMesh() == nullptr)
	{
		ReturnCode |= EUpdateResult::ERROR;
		UE_LOG(LogNearestNeighborModel, Error, TEXT("SkeletalMesh is None"));
	}
	else
	{
		const TArray<uint32> PartVertexMap = ReadTxt(ClothPartEditorData[PartId].VertexMapPath);
		const uint32 MaxVertexIndex = FMath::Max(PartVertexMap);
		if ((int32)MaxVertexIndex >= GetSkeletalMesh()->GetNumImportedVertices())
		{
			ReturnCode |= EUpdateResult::ERROR;
			UE_LOG(LogNearestNeighborModel, Error, TEXT("Part %d vertex map contains invalid vertex index %d, max vertex index is %d"), PartId, MaxVertexIndex, GetSkeletalMesh()->GetNumImportedVertices() - 1);
		}
		else
		{
			ClothPartData[PartId].VertexMap = PartVertexMap;
		}
		return (EUpdateResult)ReturnCode;
	}

	ClothPartData[PartId].VertexMap = Range(StartIndex, StartIndex + NumVertices);
	return (EUpdateResult)ReturnCode;
}

UE::NearestNeighborModel::EUpdateResult UNearestNeighborModel::UpdateClothPartData()
{
	using namespace UE::NearestNeighborModel;
	uint8 ReturnCode = EUpdateResult::SUCCESS;
	if(ClothPartEditorData.Num() == 0)
	{
		UE_LOG(LogNearestNeighborModel, Error, TEXT("There should be at least 1 cloth part"));
		return EUpdateResult::ERROR;
	}
	if (!GetSkeletalMesh() || !GetSkeletalMesh()->GetImportedModel() || GetSkeletalMesh()->GetImportedModel()->LODModels[0].ImportedMeshInfos.IsEmpty())
	{
		UE_LOG(LogNearestNeighborModel, Error, TEXT("SkeletalMesh is None or SkeletalMesh has no imported model."));
		return EUpdateResult::ERROR;
	}
	const TArray<FSkelMeshImportedMeshInfo>& SkelMeshInfos = GetSkeletalMesh()->GetImportedModel()->LODModels[0].ImportedMeshInfos;

	ClothPartData.SetNum(ClothPartEditorData.Num());
	for(int32 PartId = 0; PartId < GetNumParts(); PartId++)
	{
		ClothPartData[PartId].PCACoeffNum = ClothPartEditorData[PartId].PCACoeffNum;
		int32& MeshIndex = ClothPartEditorData[PartId].MeshIndex;
		// Set to 0 if MeshIndex is invalid. This could happen if the skeletal mesh is changed.
		MeshIndex = MeshIndex < SkelMeshInfos.Num() ? MeshIndex : 0;
		ReturnCode |= UpdateVertexMap(PartId, ClothPartEditorData[PartId].VertexMapPath, SkelMeshInfos[MeshIndex]);
		ClothPartData[PartId].NumVertices = ClothPartData[PartId].VertexMap.Num();

		if (!CheckPCAData(PartId))
		{
			ClothPartData[PartId].VertexMean.SetNumZeroed(ClothPartData[PartId].NumVertices * 3);
			ClothPartData[PartId].PCABasis.SetNumZeroed(ClothPartData[PartId].NumVertices * 3 * ClothPartData[PartId].PCACoeffNum);

			// Init default neighbor data.
			ClothPartData[PartId].NeighborCoeffs.SetNumZeroed(ClothPartData[PartId].PCACoeffNum);
			ClothPartData[PartId].NeighborOffsets.SetNumZeroed(ClothPartData[PartId].NumVertices * 3);
			ClothPartData[PartId].NumNeighbors = 1;
		}
	}
	UpdatePCACoeffNums();

	NearestNeighborData.SetNumZeroed(GetNumParts());
	UpdateNetworkInputDim();
	UpdateNetworkOutputDim();
	bClothPartDataValid = true;
	return (EUpdateResult)ReturnCode;
}

TObjectPtr<UAnimSequence> UNearestNeighborModel::GetNearestNeighborSkeletons(int32 PartId)
{ 
	if (PartId < NearestNeighborData.Num())
	{
		return NearestNeighborData[PartId].Skeletons; 
	}
	return nullptr;
}

const TObjectPtr<UAnimSequence> UNearestNeighborModel::GetNearestNeighborSkeletons(int32 PartId) const
{ 
	if (PartId < NearestNeighborData.Num())
	{
		return NearestNeighborData[PartId].Skeletons; 
	}
	return nullptr;
}

TObjectPtr<UGeometryCache> UNearestNeighborModel::GetNearestNeighborCache(int32 PartId)
{ 
	if (PartId < NearestNeighborData.Num())
	{
		return NearestNeighborData[PartId].Cache;
	}
	return nullptr;
}

const TObjectPtr<UGeometryCache> UNearestNeighborModel::GetNearestNeighborCache(int32 PartId) const
{ 
	if (PartId < NearestNeighborData.Num())
	{
		return NearestNeighborData[PartId].Cache;
	}
	return nullptr;
}

int32 UNearestNeighborModel::GetNumNeighborsFromGeometryCache(int32 PartId) const
{
	const UGeometryCache* Cache = GetNearestNeighborCache(PartId);
	if (Cache)
	{
		const int32 StartFrame = Cache->GetStartFrame();
		const int32 EndFrame = Cache->GetEndFrame();
		return EndFrame - StartFrame + 1;
	}
	else
	{
		return 0;
	}
}

int32 UNearestNeighborModel::GetNumNeighborsFromAnimSequence(int32 PartId) const
{
	const UAnimSequence* Anim = GetNearestNeighborSkeletons(PartId);
	if (Anim)
	{
		return Anim->GetDataModel()->GetNumberOfKeys();
	}
	else
	{
		return 0;
	}
}

void UNearestNeighborModel::UpdateNetworkSize()
{

	SavedNetworkSize = 0.0f;
}

void UNearestNeighborModel::UpdateMorphTargetSize()
{
	if (GetMorphTargetSet())
	{
		const FMorphTargetBuffersInfo* MorphBuffersInfo = static_cast<FMorphTargetBuffersInfo*>(&GetMorphTargetSet()->MorphBuffers);
		const double Size = MorphBuffersInfo->GetMorphDataSize();
		MorphDataSize = Size / 1024 / 1024;
	}
	else
	{
		MorphDataSize = 0.0f;
	}
}

FString UNearestNeighborModel::GetModelDir() const
{
	if (bUseFileCache)
	{
		return FileCacheDirectory;
	}
	else
	{
		return FPaths::ProjectIntermediateDir() + TEXT("NearestNeighborModel/");
	}
}

bool UNearestNeighborModel::ShouldUseOptimizedNetwork() const
{
#if NEARESTNEIGHBORMODEL_USE_ISPC
	return UE::NearestNeighborModel::bNearestNeighborModelUseOptimizedNetwork;
#else
	return false;
#endif
}

void UNearestNeighborModel::SetUseOptimizedNetwork(bool bInUseOptimizedNetwork)
{
	bUseOptimizedNetwork = bInUseOptimizedNetwork;
}

int32 UNearestNeighborModel::GetMaxPartMeshIndex() const
{
	int32 MaxIndex = -1;
	for (const FClothPartEditorData& PartData : ClothPartEditorData)
	{
		MaxIndex = FMath::Max(MaxIndex, PartData.MeshIndex);
	}
	return MaxIndex;
}
#endif

void UNearestNeighborModel::InitInputInfo()
{
	UNearestNeighborModelInputInfo* NearestNeighborModelInputInfo = static_cast<UNearestNeighborModelInputInfo*>(GetInputInfo());
	check(NearestNeighborModelInputInfo != nullptr);
#if WITH_EDITORONLY_DATA
	NearestNeighborModelInputInfo->InitRefBoneRotations(GetSkeletalMesh());
#endif
}

bool UNearestNeighborModel::CheckPCAData(int32 PartId) const
{
	const FClothPartData& Data = ClothPartData[PartId];
	return Data.VertexMap.Num() > 0 && Data.PCABasis.Num() == Data.VertexMap.Num() * 3 * Data.PCACoeffNum;
}

void UNearestNeighborModel::SetOptimizedNetwork(UNearestNeighborOptimizedNetwork* InOptimizedNetwork)
{
	OptimizedNetwork = InOptimizedNetwork;
	GetReinitModelInstanceDelegate().Broadcast();
}

bool UNearestNeighborModel::DoesUseOptimizedNetwork() const
{
	return bUseOptimizedNetwork;
}

/**
 * Get derived class default object (CDO)
 * @return A pointer to a CDO of the derived python class. 
 */
template<class T>
T* GetDerivedCDO()
{
	TArray<UClass*> Classes;
	GetDerivedClasses(T::StaticClass(), Classes);
	if (Classes.IsEmpty())
	{
		return nullptr;
	}

	T* Object = Cast<T>(Classes.Last()->GetDefaultObject());
	return Object;
}

bool UNearestNeighborModel::LoadOptimizedNetwork(const FString& OnnxPath)
{
	const FString OnnxFile = FPaths::ConvertRelativePathToFull(OnnxPath);
	if (FPaths::FileExists(OnnxFile))
	{
		UE_LOG(LogNearestNeighborModel, Display, TEXT("Loading Onnx file '%s'..."), *OnnxFile);
		UNearestNeighborOptimizedNetwork* Result = NewObject<UNearestNeighborOptimizedNetwork>(this, UNearestNeighborOptimizedNetwork::StaticClass());

		UNearestNeighborOptimizedNetworkLoader* Loader = GetDerivedCDO<UNearestNeighborOptimizedNetworkLoader>();
		if (Loader == nullptr)
		{
			return false;
		}
		Loader->SetOptimizedNetwork(Result);
		const bool bSuccess = Loader->LoadOptimizedNetwork(OnnxFile);
		if (bSuccess)
		{
			// Clear optimized network to avoid error messages in SetOptimizedNetwork. 
			OptimizedNetwork = nullptr;
			SetOptimizedNetwork(Result);
			return true;
		}
	}
	else
	{
		UE_LOG(LogNearestNeighborModel, Error, TEXT("Onnx file '%s' does not exist!"), *OnnxFile);
	}

	return false;
}

int32 UNearestNeighborModel::GetOptimizedNetworkNumOutputs() const
{
	return OptimizedNetwork ? OptimizedNetwork->GetNumOutputs() : 0;
}

#undef LOCTEXT_NAMESPACE
