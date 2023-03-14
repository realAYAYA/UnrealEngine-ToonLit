// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MLDeformerModel.h"
#include "MLDeformerVizSettings.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerMorphModel.h"
#include "NearestNeighborModelVizSettings.h"
#include "UObject/Object.h"
#include "NearestNeighborModel.generated.h"

class USkeletalMesh;
class UGeometryCache;
class UAnimSequence;
class UNeuralNetwork;
class UMLDeformerAsset;
class USkeleton;
class IPropertyHandle;

NEARESTNEIGHBORMODEL_API DECLARE_LOG_CATEGORY_EXTERN(LogNearestNeighborModel, Log, All);

namespace UE::NearestNeighborModel
{
	template<class T>
	class FArrayBufferWithSRV : public FVertexBufferWithSRV
	{
	public:
		void Init(TArray<T>* InArray, const FString& InDebugName);

	private:
		void InitRHI() override;

		TArray<T> *Array = nullptr;
		FString DebugName;
	};

	template<class T>
	class FVertexBufferWithUAV : public FVertexBufferWithSRV
	{
	public:
		void Init(int32 InNumElements, const FString& InDebugName);

	private:
		void InitRHI() override;

		int32 NumElements = 0;
		FString DebugName;
	};
};

USTRUCT(BlueprintType, Blueprintable)
struct FSkeletonCachePair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nearest Neighbors")
	TObjectPtr<UAnimSequence> Skeletons = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nearest Neighbors")
	TObjectPtr<UGeometryCache> Cache = nullptr;
};

USTRUCT(BlueprintType, Blueprintable)
struct FClothPartEditorData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "128"), Category = "Nearest Neighbors")
	int32 PCACoeffNum = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nearest Neighbors")
	FString VertexMapPath;
};

USTRUCT(BlueprintType, Blueprintable)
struct FClothPartData
{
	GENERATED_BODY()

	/** Number of PCA coefficients for this cloth part. */
	UPROPERTY()
	int32 PCACoeffNum = 128;

	/** The start index of PCA coeffcients of this cloth part */
	UPROPERTY()
	uint32 PCACoeffStart = 0;

	/** Number of vertices in this cloth part */
	UPROPERTY()
	uint32 NumVertices = 0;

	/** Number of neighbors used for this cloth part */
	UPROPERTY()
	uint32 NumNeighbors = 0;

	/** Vertex indices for this cloth part */
	UPROPERTY()
	TArray<uint32> VertexMap;

	/** PCA basis for this cloth part. This is a flattened array of size (PCACoeffNum, NumVertices * 3)  */
	UPROPERTY()
	TArray<float> PCABasis;

	/** Vertex mean for PCA computation. This has the size of NumVertices * 3 */
	UPROPERTY()
	TArray<float> VertexMean;

	/** PCA coefficients of the nearest neighbors. This is a flattened array of size (PCACoeffNum, NumNeighbors) */
	UPROPERTY()
	TArray<float> NeighborCoeffs;

	/** The remaining offsets of the nearest neighbor shapes (after reducing PCA offsets). This is a flattened array of size (NumNeighbors, PCACoeffNum) */
	UPROPERTY()
	TArray<float> NeighborOffsets;
};

/**
 * The nearest neighbor model.
 * This model contains the PCA basis of the cloth vertex deltas and a small set of cloth for nearest neighbor search. 
 * Given a new pose, the pre-trained neural network first predicts the PCA coefficients of the vertex deltas. 
 * Then this model uses the predicted PCA coeffcients to find a nearest neighbor in the small cloth set.
 * The total vertex delta is computed by
 * 		vertex_delta = mean_delta + pca_basis * pca_coeff + nearest_neighbor_delta
 * To prevent popping, a time filtering is applied on predicted vertex deltas. The vertex delta at time t is computed by  
 * 		vertex_delta(t) = decay_factor * vertex_delta(t-1) + (1 - decay_factor) * vertex_delta 
 * The cloth can be separated into several parts (e.g. shirt, pants...). The nearest neighbor search is carried out separately for each part. 
 * The pca basis and the nearest neighbor data are compressed into morph targets.
 */
UCLASS()
class NEARESTNEIGHBORMODEL_API UNearestNeighborModel 
	: public UMLDeformerMorphModel
{
	GENERATED_BODY()

public:
	UNearestNeighborModel(const FObjectInitializer& ObjectInitializer);
	virtual void PostLoad() override;

	// UMLDeformerModel overrides.
	virtual UMLDeformerModelInstance* CreateModelInstance(UMLDeformerComponent* Component);
	virtual UMLDeformerInputInfo* CreateInputInfo();
	virtual FString GetDisplayName() const override { return "Nearest Neighbor Model"; }
	// ~END UMLDeformerModel overrides.

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	int32 GetNumParts() const { return ClothPartData.Num(); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	int32 GetPartNumVerts(int32 PartId) const { return ClothPartData[PartId].NumVertices; }
	
	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	int32 GetPCACoeffStart(int32 PartId) const { return ClothPartData[PartId].PCACoeffStart; }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	int32 GetPCACoeffNum(int32 PartId) const { return ClothPartData[PartId].PCACoeffNum; }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	int32 GetNumNeighbors(int32 PartId) const { return ClothPartData[PartId].NumNeighbors; }

	float GetDecayFactor() const { return DecayFactor; };
	float GetNearestNeighborOffsetWeight() const { return NearestNeighborOffsetWeight; }

	const TArray<uint32>& PartVertexMap(int32 PartId) const { return ClothPartData[PartId].VertexMap; }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const TArray<float>& PCABasis(int32 PartId) const { return ClothPartData[PartId].PCABasis; }

	TArray<float>& PCABasis(int32 PartId) { return ClothPartData[PartId].PCABasis; }

	UFUNCTION(BlueprintCallable, Category = "Nearest Neighbor Model")
	void SetPCABasis(int32 PartId, const TArray<float>& PCABasis) { ClothPartData[PartId].PCABasis = PCABasis; }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const TArray<float>& VertexMean(int32 PartId) const { return ClothPartData[PartId].VertexMean; }

	TArray<float>& VertexMean(int32 PartId) { return ClothPartData[PartId].VertexMean; }

	UFUNCTION(BlueprintCallable, Category = "Nearest Neighbor Model")
	void SetVertexMean(int32 PartId, const TArray<float>& VertexMean) { ClothPartData[PartId].VertexMean = VertexMean; }

	UFUNCTION(BlueprintCallable, Category = "Nearest Neighbor Model")
	void SetNumNeighbors(int32 PartId, int32 InNumNeighbors) { ClothPartData[PartId].NumNeighbors = InNumNeighbors; }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const TArray<float>& NeighborCoeffs(int32 PartId) const { return ClothPartData[PartId].NeighborCoeffs; }

	TArray<float>& NeighborCoeffs(int32 PartId) { return ClothPartData[PartId].NeighborCoeffs; }

	UFUNCTION(BlueprintCallable, Category = "Nearest Neighbor Model")
	void SetNeighborCoeffs(int32 PartId, const TArray<float>& NeighborCoeffs) { ClothPartData[PartId].NeighborCoeffs = NeighborCoeffs; }


	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const TArray<float>& NeighborOffsets(int32 PartId) const { return ClothPartData[PartId].NeighborOffsets; }

	TArray<float>& NeighborOffsets(int32 PartId) { return ClothPartData[PartId].NeighborOffsets; }

	UFUNCTION(BlueprintCallable, Category = "Nearest Neighbor Model")
	void SetNeighborOffsets(int32 PartId, const TArray<float>& NeighborOffsets) { ClothPartData[PartId].NeighborOffsets = NeighborOffsets; }

	void ClipInputs(float* InputPtr, int NumInputs);

	bool CheckPCAData(int32 PartId) const;

	void InitInputInfo();
	void InitPreviousWeights();

#if WITH_EDITORONLY_DATA
	TObjectPtr<UAnimSequence> GetNearestNeighborSkeletons(int32 PartId);
	const TObjectPtr<UAnimSequence> GetNearestNeighborSkeletons(int32 PartId) const;
	TObjectPtr<UGeometryCache> GetNearestNeighborCache(int32 PartId);
	const TObjectPtr<UGeometryCache> GetNearestNeighborCache(int32 PartId) const;
	int32 GetNumNeighborsFromGeometryCache(int32 PartId) const;

	void UpdateNetworkInputDim();
	void UpdateNetworkOutputDim();
	void UpdateClothPartData();
	void UpdatePCACoeffNums();
	void UpdateNetworkSize();
	void UpdateMorphTargetSize();

	void InvalidateClothPartData() { bClothPartDataValid = false; bNearestNeighborDataValid = false; }
	void ValidateClothPartData() { bClothPartDataValid = true; }
	void InvalidateNearestNeighborData() { bNearestNeighborDataValid = false; }
	void ValidateNearestNeighborData() {bNearestNeighborDataValid = true; }
	bool IsClothPartDataValid() { return bClothPartDataValid; }
	bool IsNearestNeighborDataValid() { return bNearestNeighborDataValid; }
#endif

#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const int32	GetInputDim() const { return InputDim; }
	static FName GetInputDimPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, InputDim); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const TArray<int32>& GetHiddenLayerDims() const { return HiddenLayerDims; }
	static FName GetHiddenLayerDimsPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, HiddenLayerDims); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const int32	GetOutputDim() const { return OutputDim; }
	static FName GetOutputDimPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, OutputDim); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const int32	GetNumEpochs() const { return NumEpochs; }
	static FName GetNumEpochsPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, NumEpochs); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const int32	GetBatchSize() const { return BatchSize; }
	static FName GetBatchSizePropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, BatchSize); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const float	GetLearningRate() const { return LearningRate; }
	static FName GetLearningRatePropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, LearningRate); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const TArray<FClothPartEditorData>&	GetClothPartEditorData() const { return ClothPartEditorData; }
	static FName GetClothPartEditorDataPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, ClothPartEditorData); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	const TArray<FSkeletonCachePair>& GetNearestNeighborData() const { return NearestNeighborData; }
	static FName GetNearestNeighborDataPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, NearestNeighborData); }

	static FName GetNearestNeighborOffsetWeightPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, NearestNeighborOffsetWeight); }

	static FName GetSavedNetworkSizePropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, SavedNetworkSize); }
	static FName GetMorphDataSizePropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, MorphDataSize); }

	bool GetUseFileCache() const { return bUseFileCache; }
	static FName GetUseFileCachePropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, bUseFileCache); }
	static FName GetFileCacheDirectoryPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, FileCacheDirectory); }
	static FName GetRecomputeDeltasPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, bRecomputeDeltas); }
	static FName GetRecomputePCAPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, bRecomputePCA); }


	bool GetUsePartOnlyMesh() const { return bUsePartOnlyMesh; }
	static FName GetUsePartOnlyMeshPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModel, bUsePartOnlyMesh); }

	UFUNCTION(BlueprintPure, Category = "Nearest Neighbor Model")
	FString GetModelDir() const;
#endif


protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Settings")
	int32 InputDim = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Settings")
	TArray<int32> HiddenLayerDims;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Settings")
	int32 OutputDim = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Settings", meta = (ClampMin = "1"))
	int32 NumEpochs = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Settings", meta = (ClampMin = "1"))
	int32 BatchSize = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Settings", meta = (ClampMin = "0.000001", ClampMax = "1.0"))
	float LearningRate = 0.001f;

	UPROPERTY()
	bool bClothPartDataValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloth Parts")
	TArray<FClothPartEditorData> ClothPartEditorData;

	UPROPERTY()
	bool bNearestNeighborDataValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nearest Neighbors")
	bool bUsePartOnlyMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nearest Neighbors")
	TArray<FSkeletonCachePair> NearestNeighborData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Settings", meta = (ClampMin = "0.0"))
	float SavedNetworkSize = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Morph Targets", meta = (ClampMin = "0.0"))
	float MorphDataSize = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Cache")
	bool bUseFileCache = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Cache")
	FString FileCacheDirectory = FPaths::ProjectIntermediateDir() + "NearestNeighborModel";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Cache")
	bool bRecomputeDeltas = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Cache")
	bool bRecomputePCA = true;
#endif

public:
	UPROPERTY()
	TArray<FClothPartData> ClothPartData;

	UPROPERTY(BlueprintReadWrite, Category = "Network Inputs")
	TArray<float> InputsMin;

	UPROPERTY(BlueprintReadWrite, Category = "Network Inputs")
	TArray<float> InputsMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KMeans Pose Generator")
	TArray<TObjectPtr<UAnimSequence>> SourceSkeletons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KMeans Pose Generator", meta = (ClampMin = "1"))
	int32 NumClusters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nearest Neighbors", META = (ClampMin = "0", ClampMax = "1"))
	float DecayFactor = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nearest Neighbors", META = (ClampMin = "0", ClampMax = "1"))
	float NearestNeighborOffsetWeight = 1.0f;

	TArray<float> PreviousWeights; 
};
