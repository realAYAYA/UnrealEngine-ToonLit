// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PoseSearch/PoseSearchDerivedData.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "InstancedStruct.h"
#include "Misc/CoreDelegates.h"
#include "PoseSearchDatabaseIndexingContext.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedDataKey.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchNormalizationSet.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchEigenHelper.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/BulkDataRegistry.h"
#include "UObject/NoExportTypes.h"
#include "UObject/PackageReload.h"

namespace UE::PoseSearch
{
static const UE::DerivedData::FValueId Id(UE::DerivedData::FValueId::FromName("Data"));
static const UE::DerivedData::FCacheBucket Bucket("PoseSearchDatabase");

#if ENABLE_COOK_STATS
static FCookStats::FDDCResourceUsageStats UsageStats;
static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("MotionMatching.Usage"), TEXT(""));
	});
#endif

// helper struct to calculate mean deviations
struct FMeanDeviationCalculator
{
private:
	struct FEntry
	{
		int32 SchemaIndex = -1; // index of the Channel associated Schemas / SearchIndexBases index
		const UPoseSearchFeatureChannel* Channel;
	};

	typedef TArray<FEntry> FEntries;		// array of FEntry with channels that can be normalized together (for example it contains all the phases of the left foot from different schemas)
	typedef TArray<FEntries> FEntriesGroup;	// array of FEntries incompatible between each other

	static void Add(const UPoseSearchFeatureChannel* Channel, int32 SchemaIndex, FEntriesGroup& EntriesGroup)
	{
		bool bEntryFound = false;
		for (FEntries& Entries : EntriesGroup)
		{
			if (Entries[0].Channel->CanBeNormalizedWith(Channel))
			{
				Entries.Add({ SchemaIndex, Channel });
				bEntryFound = true;
				break;
			}
		}

		if (!bEntryFound)
		{
			FEntries& Entries = EntriesGroup[EntriesGroup.AddDefaulted()];
			Entries.Add({ SchemaIndex, Channel });
		}
	}

	static void AnalyzeChannelRecursively(const UPoseSearchFeatureChannel* Channel, int32 SchemaIndex, FEntriesGroup& EntriesGroup)
	{
		const TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> SubChannels = Channel->GetSubChannels();
		if (SubChannels.Num() == 0)
		{
			Add(Channel, SchemaIndex, EntriesGroup);
		}
		else
		{
			// the channel is a group channel, so we AnalyzeChannelRecursively
			for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : Channel->GetSubChannels())
			{
				if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
				{
					AnalyzeChannelRecursively(SubChannel, SchemaIndex, EntriesGroup);
				}
			}
		}
	}

	static void AnalyzeSchemas(TConstArrayView<const UPoseSearchSchema*> Schemas, FEntriesGroup& EntriesGroup)
	{
		for (int32 SchemaIndex = 0; SchemaIndex < Schemas.Num(); ++SchemaIndex)
		{
			const UPoseSearchSchema* Schema = Schemas[SchemaIndex];
			for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Schema->GetChannels())
			{
				AnalyzeChannelRecursively(ChannelPtr.Get(), SchemaIndex, EntriesGroup);
			}
		}
	}

	// given an array of channels that can be normalized together (Entries), with the same cardinality (Entries[0].Channel->GetChannelCardinality()),
	// it'll calculate the mean deviation of the associated data (from SearchIndexBases)
	static float CalculateEntriesMeanDeviation(const FEntries& Entries, TConstArrayView<FSearchIndexBase> SearchIndexBases, TConstArrayView<const UPoseSearchSchema*> Schemas)
	{
		check(Schemas.Num() == SearchIndexBases.Num());
	
		const int32 EntriesNum = Entries.Num();
		check(EntriesNum > 0);

		const int32 Cardinality = Entries[0].Channel->GetChannelCardinality();
		check(Cardinality > 0);

		int32 TotalNumPoses = 0;
		for (int32 EntryIdx = 0; EntryIdx < EntriesNum; ++EntryIdx)
		{
			TotalNumPoses += SearchIndexBases[Entries[EntryIdx].SchemaIndex].GetNumPoses();
		}

		int32 AccumulatedNumPoses = 0;
		RowMajorMatrix CenteredSubPoseMatrix(TotalNumPoses, Cardinality);
		for (int32 EntryIdx = 0; EntryIdx < EntriesNum; ++EntryIdx)
		{
			const FEntry& Entry = Entries[EntryIdx];
			check(Cardinality == Entry.Channel->GetChannelCardinality());

			const int32 DataSetIdx = Entry.SchemaIndex;

			const UPoseSearchSchema* Schema = Schemas[DataSetIdx];
			const FSearchIndexBase& SearchIndex = SearchIndexBases[DataSetIdx];

			const int32 NumPoses = SearchIndex.GetNumPoses();

			// Map input buffer with NumPoses as rows and NumDimensions	as cols
			RowMajorMatrixMapConst PoseMatrixSourceMap(SearchIndex.Values.GetData(), NumPoses, Schema->SchemaCardinality);

			// Given the sub matrix for the features, find the average distance to the feature's centroid.
			CenteredSubPoseMatrix.block(AccumulatedNumPoses, 0, NumPoses, Cardinality) = PoseMatrixSourceMap.block(0, Entry.Channel->GetChannelDataOffset(), NumPoses, Cardinality);
			AccumulatedNumPoses += NumPoses;
		}

		RowMajorVector SampleMean = CenteredSubPoseMatrix.colwise().mean();
		CenteredSubPoseMatrix = CenteredSubPoseMatrix.rowwise() - SampleMean;

		// after mean centering the data, the average distance to the centroid is simply the average norm.
		const float FeatureMeanDeviation = CenteredSubPoseMatrix.rowwise().norm().mean();

		return FeatureMeanDeviation;
	}

public:

	// it returns an array of dimension Schemas[0]->SchemaCardinality containing the mean deviation calculated from the data passed in with SearchIndexBases following the layout described in the schemas channels:
	// channels from all the schemas get collected in groups that can be normalized together (FEntriesGroup, populated in AnalyzeSchemas) and then those homogeneous (in cardinality and meaning) groups get processed 
	// one by one in CalculateEntriesMeanDeviation to extract the group mean deviation against the input data contained in SearchIndexBases
	static TArray<float> Calculate(TConstArrayView<FSearchIndexBase> SearchIndexBases, TConstArrayView<const UPoseSearchSchema*> Schemas)
	{
		// This method performs a modified z-score normalization where features are normalized
		// by mean absolute deviation rather than standard deviation. Both methods are preferable
		// here to min-max scaling because they preserve outliers.
		// 
		// Mean absolute deviation is preferred here over standard deviation because the latter
		// emphasizes outliers since squaring the distance from the mean increases variance 
		// exponentially rather than additively and square rooting the sum of squares does not 
		// remove that bias. [1]
		//
		// References:
		// [1] Gorard, S. (2005), "Revisiting a 90-Year-Old Debate: The Advantages of the Mean Deviation."
		//     British Journal of Educational Studies, 53: 417-430.

		int32 ThisSchemaIndex = 0;
		check(SearchIndexBases.Num() == Schemas.Num() && Schemas.Num() > ThisSchemaIndex);
		const UPoseSearchSchema* ThisSchema = Schemas[ThisSchemaIndex];
		check(ThisSchema->IsValid());
		const int32 NumDimensions = ThisSchema->SchemaCardinality;

		TArray<float> MeanDeviations;
		MeanDeviations.Init(1.f, NumDimensions);
		RowMajorVectorMap MeanDeviationsMap(MeanDeviations.GetData(), 1, NumDimensions);

		const EPoseSearchDataPreprocessor DataPreprocessor = ThisSchema->DataPreprocessor;
		if (SearchIndexBases[ThisSchemaIndex].GetNumPoses() > 0 && (DataPreprocessor == EPoseSearchDataPreprocessor::Normalize || DataPreprocessor == EPoseSearchDataPreprocessor::NormalizeOnlyByDeviation))
		{
			FEntriesGroup EntriesGroup;

			AnalyzeSchemas(Schemas, EntriesGroup);

			for (const FEntries& Entries : EntriesGroup)
			{
				for (const FEntry& Entry : Entries)
				{
					if (Entry.Channel->GetChannelCardinality() > 0 && Entry.SchemaIndex == ThisSchemaIndex)
					{
						const float FeatureMeanDeviation = CalculateEntriesMeanDeviation(Entries, SearchIndexBases, Schemas);
						// the associated data to all the Entries data is going to be used to calculate the deviation of Deviation[Entry.Channel->GetChannelDataOffset()] to Deviation[Entry.Channel->GetChannelDataOffset() + Entry.Channel->GetChannelCardinality()]

						// Fill the feature's corresponding scaling axes with the average distance
						// Avoid scaling by zero by leaving near-zero deviations as 1.0
						static const float MinFeatureMeanDeviation = 0.1f;
						MeanDeviationsMap.segment(Entry.Channel->GetChannelDataOffset(), Entry.Channel->GetChannelCardinality()).setConstant(FeatureMeanDeviation > MinFeatureMeanDeviation ? FeatureMeanDeviation : 1.f);
					}
				}
			}
		}
	
		return MeanDeviations;
	}
};

static inline FFloatInterval GetEffectiveSamplingRange(const UAnimSequenceBase* Sequence, FFloatInterval RequestedSamplingRange)
{
	const bool bSampleAll = (RequestedSamplingRange.Min == 0.0f) && (RequestedSamplingRange.Max == 0.0f);
	const float SequencePlayLength = Sequence->GetPlayLength();
	FFloatInterval Range;
	Range.Min = bSampleAll ? 0.0f : RequestedSamplingRange.Min;
	Range.Max = bSampleAll ? SequencePlayLength : FMath::Min(SequencePlayLength, RequestedSamplingRange.Max);
	return Range;
}

static void FindValidSequenceIntervals(const UAnimSequenceBase* SequenceBase, FFloatInterval SamplingRange, bool bIsLooping,
	const FPoseSearchExcludeFromDatabaseParameters& ExcludeFromDatabaseParameters, TArray<FFloatRange>& ValidRanges)
{
	check(SequenceBase);

	const float SequenceLength = SequenceBase->GetPlayLength();

	const FFloatInterval EffectiveSamplingInterval = GetEffectiveSamplingRange(SequenceBase, SamplingRange);
	FFloatRange EffectiveSamplingRange = FFloatRange::Inclusive(EffectiveSamplingInterval.Min, EffectiveSamplingInterval.Max);
	if (!bIsLooping)
	{
		const FFloatRange ExcludeFromDatabaseRange(ExcludeFromDatabaseParameters.SequenceStartInterval, SequenceLength - ExcludeFromDatabaseParameters.SequenceEndInterval);
		EffectiveSamplingRange = FFloatRange::Intersection(EffectiveSamplingRange, ExcludeFromDatabaseRange);
	}

	// start from a single interval defined by the database sequence sampling range
	ValidRanges.Empty();
	ValidRanges.Add(EffectiveSamplingRange);

	FAnimNotifyContext NotifyContext;
	SequenceBase->GetAnimNotifies(0.0f, SequenceLength, NotifyContext);

	for (const FAnimNotifyEventReference& EventReference : NotifyContext.ActiveNotifies)
	{
		if (const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify())
		{
			if (const UAnimNotifyState_PoseSearchExcludeFromDatabase* ExclusionNotifyState = Cast<const UAnimNotifyState_PoseSearchExcludeFromDatabase>(NotifyEvent->NotifyStateClass))
			{
				FFloatRange ExclusionRange = FFloatRange::Inclusive(NotifyEvent->GetTriggerTime(), NotifyEvent->GetEndTriggerTime());

				// Split every valid range based on the exclusion range just found. Because this might increase the 
				// number of ranges in ValidRanges, the algorithm iterates from end to start.
				for (int RangeIdx = ValidRanges.Num() - 1; RangeIdx >= 0; --RangeIdx)
				{
					FFloatRange EvaluatedRange = ValidRanges[RangeIdx];
					ValidRanges.RemoveAt(RangeIdx);

					TArray<FFloatRange> Diff = FFloatRange::Difference(EvaluatedRange, ExclusionRange);
					ValidRanges.Append(Diff);
				}
			}
		}
	}
}

static void InitSearchIndexAssets(FSearchIndexBase& SearchIndex, const UPoseSearchDatabase* Database)
{
	using namespace UE::PoseSearch;

	SearchIndex.Assets.Empty();
	TArray<FFloatRange> ValidRanges;
	TArray<FBlendSampleData> BlendSamples;

	int32 TotalPoses = 0;
	for (int32 AnimationAssetIndex = 0; AnimationAssetIndex < Database->AnimationAssets.Num(); ++AnimationAssetIndex)
	{
		const FInstancedStruct& DatabaseAssetStruct = Database->GetAnimationAssetStruct(AnimationAssetIndex);
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
		{
			if (!DatabaseAsset->IsEnabled() || !DatabaseAsset->GetAnimationAsset())
			{
				continue;
			}

			const bool bAddUnmirrored = DatabaseAsset->GetMirrorOption() == EPoseSearchMirrorOption::UnmirroredOnly || DatabaseAsset->GetMirrorOption() == EPoseSearchMirrorOption::UnmirroredAndMirrored;
			const bool bAddMirrored = DatabaseAsset->GetMirrorOption() == EPoseSearchMirrorOption::MirroredOnly || DatabaseAsset->GetMirrorOption() == EPoseSearchMirrorOption::UnmirroredAndMirrored;
			const int32 SchemaSampleRate = Database->Schema->SampleRate;

			if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseBlendSpace>())
			{
				int32 HorizontalBlendNum, VerticalBlendNum;
				DatabaseBlendSpace->GetBlendSpaceParameterSampleRanges(HorizontalBlendNum, VerticalBlendNum);

				const bool bWrapInputOnHorizontalAxis = DatabaseBlendSpace->BlendSpace->GetBlendParameter(0).bWrapInput;
				const bool bWrapInputOnVerticalAxis = DatabaseBlendSpace->BlendSpace->GetBlendParameter(1).bWrapInput;
				for (int32 HorizontalIndex = 0; HorizontalIndex < HorizontalBlendNum; HorizontalIndex++)
				{
					for (int32 VerticalIndex = 0; VerticalIndex < VerticalBlendNum; VerticalIndex++)
					{
						const FVector BlendParameters = DatabaseBlendSpace->BlendParameterForSampleRanges(HorizontalIndex, VerticalIndex);

						int32 TriangulationIndex = 0;
						DatabaseBlendSpace->BlendSpace->GetSamplesFromBlendInput(BlendParameters, BlendSamples, TriangulationIndex, true);

						const float PlayLength = DatabaseBlendSpace->BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);

						for (int32 PermutationIdx = 0; PermutationIdx < Database->Schema->NumberOfPermutations; ++PermutationIdx)
						{
							if (bAddUnmirrored)
							{
								const FSearchIndexAsset PoseSearchIndexAsset(AnimationAssetIndex, TotalPoses, false, FFloatInterval(0.f, PlayLength), SchemaSampleRate, PermutationIdx, BlendParameters);
								if (PoseSearchIndexAsset.GetNumPoses() > 0)
								{
									SearchIndex.Assets.Add(PoseSearchIndexAsset);
									TotalPoses += PoseSearchIndexAsset.GetNumPoses();
								}
							}

							if (bAddMirrored)
							{
								const FSearchIndexAsset PoseSearchIndexAsset(AnimationAssetIndex, TotalPoses, true, FFloatInterval(0.f, PlayLength), SchemaSampleRate, PermutationIdx, BlendParameters);
								if (PoseSearchIndexAsset.GetNumPoses() > 0)
								{
									SearchIndex.Assets.Add(PoseSearchIndexAsset);
									TotalPoses += PoseSearchIndexAsset.GetNumPoses();
								}
							}
						}
					}
				}
			}
			// support for FPoseSearchDatabaseSequence, FPoseSearchDatabaseAnimComposite, FPoseSearchDatabaseAnimMontage
			else if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(DatabaseAsset->GetAnimationAsset()))
			{
				ValidRanges.Reset();

				FindValidSequenceIntervals(SequenceBase, DatabaseAsset->GetSamplingRange(), DatabaseAsset->IsLooping(), Database->ExcludeFromDatabaseParameters, ValidRanges);
				for (const FFloatRange& Range : ValidRanges)
				{
					for (int32 PermutationIdx = 0; PermutationIdx < Database->Schema->NumberOfPermutations; ++PermutationIdx)
					{
						if (bAddUnmirrored)
						{
							const FSearchIndexAsset PoseSearchIndexAsset(AnimationAssetIndex, TotalPoses, false, FFloatInterval(Range.GetLowerBoundValue(), Range.GetUpperBoundValue()), SchemaSampleRate, PermutationIdx);
							if (PoseSearchIndexAsset.GetNumPoses() > 0)
							{
								SearchIndex.Assets.Add(PoseSearchIndexAsset);
								TotalPoses += PoseSearchIndexAsset.GetNumPoses();
							}
						}

						if (bAddMirrored)
						{
							const FSearchIndexAsset PoseSearchIndexAsset(AnimationAssetIndex, TotalPoses, true, FFloatInterval(Range.GetLowerBoundValue(), Range.GetUpperBoundValue()), SchemaSampleRate, PermutationIdx);
							if (PoseSearchIndexAsset.GetNumPoses() > 0)
							{
								SearchIndex.Assets.Add(PoseSearchIndexAsset);
								TotalPoses += PoseSearchIndexAsset.GetNumPoses();
							}
						}
					}
				}
			}
			else
			{
				checkNoEntry();
			}
		}
	}
}

static void PreprocessSearchIndexWeights(FSearchIndex& SearchIndex, const UPoseSearchSchema* Schema, TConstArrayView<float> Deviation)
{
	const int32 NumDimensions = Schema->SchemaCardinality;
	SearchIndex.WeightsSqrt.Init(1.f, NumDimensions);

	for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Schema->GetChannels())
	{
		ChannelPtr->FillWeights(SearchIndex.WeightsSqrt);
	}

	EPoseSearchDataPreprocessor DataPreprocessor = Schema->DataPreprocessor;
	if (DataPreprocessor == EPoseSearchDataPreprocessor::Normalize)
	{
		// normalizing user weights: the idea behind this step is to be able to compare poses from databases using different schemas
		RowMajorVectorMap MapWeights(SearchIndex.WeightsSqrt.GetData(), 1, NumDimensions);
		const float WeightsSum = MapWeights.sum();
		if (!FMath::IsNearlyZero(WeightsSum))
		{
			MapWeights *= (1.f / WeightsSum);
		}
	}

	// extracting the square root
	for (int32 Dimension = 0; Dimension != NumDimensions; ++Dimension)
	{
		SearchIndex.WeightsSqrt[Dimension] = FMath::Sqrt(SearchIndex.WeightsSqrt[Dimension]);
	}

	if (DataPreprocessor == EPoseSearchDataPreprocessor::Normalize || DataPreprocessor == EPoseSearchDataPreprocessor::NormalizeOnlyByDeviation)
	{
		for (int32 Dimension = 0; Dimension != NumDimensions; ++Dimension)
		{
			// the idea here is to pre-multiply the weights by the inverse of the variance (proportional to the square of the deviation) to have a "weighted Mahalanobis" distance
			SearchIndex.WeightsSqrt[Dimension] /= Deviation[Dimension];
		}
	}
}

// it calculates Mean, PCAValues, and PCAProjectionMatrix
static void PreprocessSearchIndexPCAData(FSearchIndex& SearchIndex, int32 NumDimensions, uint32 NumberOfPrincipalComponents, EPoseSearchMode PoseSearchMode)
{
	// binding SearchIndex.Values and SearchIndex.PCAValues Eigen row major matrix maps
	const int32 NumPoses = SearchIndex.GetNumPoses();

	SearchIndex.PCAExplainedVariance = 0.f;

	SearchIndex.PCAValues.Reset();
	SearchIndex.Mean.Reset();
	SearchIndex.PCAProjectionMatrix.Reset();

	if (PoseSearchMode != EPoseSearchMode::BruteForce && NumDimensions > 0 && NumPoses > 0 && NumberOfPrincipalComponents > 0)
	{
		SearchIndex.PCAValues.AddZeroed(NumPoses * NumberOfPrincipalComponents);
		SearchIndex.Mean.AddZeroed(NumDimensions);
		SearchIndex.PCAProjectionMatrix.AddZeroed(NumDimensions * NumberOfPrincipalComponents);

		const RowMajorVectorMapConst MapWeightsSqrt(SearchIndex.WeightsSqrt.GetData(), 1, NumDimensions);
		const RowMajorMatrixMapConst MapValues(SearchIndex.Values.GetData(), NumPoses, NumDimensions);
		const RowMajorMatrix WeightedValues = MapValues.array().rowwise() * MapWeightsSqrt.array();
		RowMajorMatrixMap MapPCAValues(SearchIndex.PCAValues.GetData(), NumPoses, NumberOfPrincipalComponents);

		// calculating the mean
		RowMajorVectorMap Mean(SearchIndex.Mean.GetData(), 1, NumDimensions);
		Mean = WeightedValues.colwise().mean();

		// use the mean to center the data points
		const RowMajorMatrix CenteredValues = WeightedValues.rowwise() - Mean;

		// estimating the covariance matrix (with dimensionality of NumDimensions, NumDimensions)
		// formula: https://en.wikipedia.org/wiki/Covariance_matrix#Estimation
		// details: https://en.wikipedia.org/wiki/Estimation_of_covariance_matrices
		const ColMajorMatrix CovariantMatrix = (CenteredValues.transpose() * CenteredValues) / float(NumPoses - 1);
		const Eigen::SelfAdjointEigenSolver<ColMajorMatrix> EigenSolver(CovariantMatrix);

		check(EigenSolver.info() == Eigen::Success);

		// validating EigenSolver results
		const ColMajorMatrix EigenVectors = EigenSolver.eigenvectors().real();

		if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate && NumberOfPrincipalComponents == NumDimensions)
		{
			const RowMajorVector ReciprocalWeightsSqrt = MapWeightsSqrt.cwiseInverse();
			const RowMajorMatrix ProjectedValues = CenteredValues * EigenVectors;
			for (Eigen::Index RowIndex = 0; RowIndex < MapValues.rows(); ++RowIndex)
			{
				const RowMajorVector WeightedReconstructedPoint = ProjectedValues.row(RowIndex) * EigenVectors.transpose() + Mean;
				const RowMajorVector ReconstructedPoint = WeightedReconstructedPoint.array() * ReciprocalWeightsSqrt.array();
				const float Error = (ReconstructedPoint - MapValues.row(RowIndex)).squaredNorm();
				check(Error < UE_KINDA_SMALL_NUMBER);
			}
		}

		// sorting EigenVectors by EigenValues, so we pick the most significant ones to compose our PCA projection matrix.
		const RowMajorVector EigenValues = EigenSolver.eigenvalues().real();
		TArray<size_t> Indexer;
		Indexer.Reserve(NumDimensions);
		for (size_t DimensionIndex = 0; DimensionIndex < NumDimensions; ++DimensionIndex)
		{
			Indexer.Push(DimensionIndex);
		}
		Indexer.Sort([&EigenValues](size_t a, size_t b)
			{
				return EigenValues[a] > EigenValues[b];
			});

		// composing the PCA projection matrix with the PCANumComponents most significant EigenVectors
		ColMajorMatrixMap PCAProjectionMatrix(SearchIndex.PCAProjectionMatrix.GetData(), NumDimensions, NumberOfPrincipalComponents);
		float AccumulatedVariance = 0.f;
		for (size_t PCAComponentIndex = 0; PCAComponentIndex < NumberOfPrincipalComponents; ++PCAComponentIndex)
		{
			PCAProjectionMatrix.col(PCAComponentIndex) = EigenVectors.col(Indexer[PCAComponentIndex]);
			AccumulatedVariance += EigenValues[Indexer[PCAComponentIndex]];
		}

		// calculating the total variance knowing that eigen values measure variance along the principal components:
		const float TotalVariance = EigenValues.sum();
		// and explained variance as ratio between AccumulatedVariance and TotalVariance: https://ro-che.info/articles/2017-12-11-pca-explained-variance
		SearchIndex.PCAExplainedVariance = TotalVariance > UE_KINDA_SMALL_NUMBER ? AccumulatedVariance / TotalVariance : 0.f;

		MapPCAValues = CenteredValues * PCAProjectionMatrix;

		if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate && NumberOfPrincipalComponents == NumDimensions)
		{
			const RowMajorVector ReciprocalWeightsSqrt = MapWeightsSqrt.cwiseInverse();
			for (Eigen::Index RowIndex = 0; RowIndex < MapValues.rows(); ++RowIndex)
			{
				const RowMajorVector WeightedReconstructedValues = MapPCAValues.row(RowIndex) * PCAProjectionMatrix.transpose() + Mean;
				const RowMajorVector ReconstructedValues = WeightedReconstructedValues.array() * ReciprocalWeightsSqrt.array();
				const float Error = (ReconstructedValues - MapValues.row(RowIndex)).squaredNorm();
				check(Error < UE_KINDA_SMALL_NUMBER);
			}

			TArray<float> ReconstructedPoseValues;
			ReconstructedPoseValues.SetNumZeroed(NumDimensions);
			for (int32 PoseIdx = 0; PoseIdx < NumPoses; ++PoseIdx)
			{
				SearchIndex.GetReconstructedPoseValues(PoseIdx, ReconstructedPoseValues);
				TConstArrayView<float> PoseValues = SearchIndex.GetPoseValues(PoseIdx);

				check(ReconstructedPoseValues.Num() == PoseValues.Num());
				Eigen::Map<const Eigen::ArrayXf> VA(ReconstructedPoseValues.GetData(), ReconstructedPoseValues.Num());
				Eigen::Map<const Eigen::ArrayXf> VB(PoseValues.GetData(), PoseValues.Num());

				const float Error = (VA - VB).square().sum();
				check(Error < UE_KINDA_SMALL_NUMBER);
			}
		}
	}
}

static void PreprocessSearchIndexKDTree(FSearchIndex& SearchIndex, int32 NumDimensions, uint32 NumberOfPrincipalComponents, EPoseSearchMode PoseSearchMode, int32 KDTreeMaxLeafSize, int32 KDTreeQueryNumNeighbors)
{
	SearchIndex.KDTree.Reset();
	if (PoseSearchMode != EPoseSearchMode::BruteForce && NumDimensions > 0)
	{
		const int32 NumPoses = SearchIndex.GetNumPoses();
		SearchIndex.KDTree.Construct(NumPoses, NumberOfPrincipalComponents, SearchIndex.PCAValues.GetData(), KDTreeMaxLeafSize);

		if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate)
		{
			// testing the KDTree is returning the proper searches for all the points in pca space
			int32 NumberOfFailingPoints = 0;
			for (size_t PointIndex = 0; PointIndex < NumPoses; ++PointIndex)
			{
				TArray<size_t> ResultIndexes;
				TArray<float> ResultDistanceSqr;
				ResultIndexes.SetNum(KDTreeQueryNumNeighbors + 1);
				ResultDistanceSqr.SetNum(KDTreeQueryNumNeighbors + 1);
				FKDTree::KNNResultSet ResultSet(KDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);
				SearchIndex.KDTree.FindNeighbors(ResultSet, &SearchIndex.PCAValues[PointIndex * NumberOfPrincipalComponents]);

				size_t ResultIndex = 0;
				for (; ResultIndex < ResultSet.Num(); ++ResultIndex)
				{
					if (PointIndex == ResultIndexes[ResultIndex])
					{
						check(ResultDistanceSqr[ResultIndex] < UE_KINDA_SMALL_NUMBER);
						break;
					}
				}
				if (ResultIndex == ResultSet.Num())
				{
					++NumberOfFailingPoints;
				}
			}

			check(NumberOfFailingPoints == 0);

			// testing the KDTree is returning the proper searches for all the original points transformed in pca space
			NumberOfFailingPoints = 0;
			for (size_t PointIndex = 0; PointIndex < NumPoses; ++PointIndex)
			{
				TArray<size_t> ResultIndexes;
				TArray<float> ResultDistanceSqr;
				ResultIndexes.SetNum(KDTreeQueryNumNeighbors + 1);
				ResultDistanceSqr.SetNum(KDTreeQueryNumNeighbors + 1);
				FKDTree::KNNResultSet ResultSet(KDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);

				const RowMajorVectorMapConst MapValues(&SearchIndex.Values[PointIndex * NumDimensions], 1, NumDimensions);
				const RowMajorVectorMapConst MapWeightsSqrt(SearchIndex.WeightsSqrt.GetData(), 1, NumDimensions);
				const RowMajorVectorMapConst Mean(SearchIndex.Mean.GetData(), 1, NumDimensions);
				const ColMajorMatrixMapConst PCAProjectionMatrix(SearchIndex.PCAProjectionMatrix.GetData(), NumDimensions, NumberOfPrincipalComponents);

				const RowMajorMatrix WeightedValues = MapValues.array() * MapWeightsSqrt.array();
				const RowMajorMatrix CenteredValues = WeightedValues - Mean;
				const RowMajorVector ProjectedValues = CenteredValues * PCAProjectionMatrix;

				SearchIndex.KDTree.FindNeighbors(ResultSet, ProjectedValues.data());

				size_t ResultIndex = 0;
				for (; ResultIndex < ResultSet.Num(); ++ResultIndex)
				{
					if (PointIndex == ResultIndexes[ResultIndex])
					{
						check(ResultDistanceSqr[ResultIndex] < UE_KINDA_SMALL_NUMBER);
						break;
					}
				}
				if (ResultIndex == ResultSet.Num())
				{
					++NumberOfFailingPoints;
				}
			}

			check(NumberOfFailingPoints == 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseAsyncCacheTask
struct FPoseSearchDatabaseAsyncCacheTask
{
	enum class EState
	{
		Notstarted,	// key generation failed (not all the asset has been post loaded). It'll be retried to StartNewRequestIfNeeded the next Update
		Prestarted,	// key has been successfully generated and we kicked the DDC get
		Cancelled,	// the task has been cancelled
		Ended,		// the task has ended successfully
		Failed		// the task has ended unsuccessfully
	};

	// these methods MUST be protected by FPoseSearchDatabaseAsyncCacheTask::Mutex! and to make sure we pass the mutex as input param
	FPoseSearchDatabaseAsyncCacheTask(UPoseSearchDatabase* InDatabase, FCriticalSection& OuterMutex);
	void StartNewRequestIfNeeded(FCriticalSection& OuterMutex);
	bool CancelIfDependsOn(const UObject* Object, FCriticalSection& OuterMutex);
	void Update(FCriticalSection& OuterMutex);
	void Wait(FCriticalSection& OuterMutex);
	void Cancel(FCriticalSection& OuterMutex);
	bool Poll(FCriticalSection& OuterMutex) const;
	void AddReferencedObjects(FReferenceCollector& Collector, FCriticalSection& OuterMutex);
	bool ContainsDatabase(const UPoseSearchDatabase* OtherDatabase, FCriticalSection& OuterMutex) const;
	bool IsValid(FCriticalSection& OuterMutex) const;
	const FIoHash& GetDerivedDataKey() const { return DerivedDataKey; }
	const UPoseSearchDatabase* GetDatabase() const { return Database.Get(); }

	~FPoseSearchDatabaseAsyncCacheTask();
	EState GetState() const { return EState(ThreadSafeState.GetValue()); }

private:
	FPoseSearchDatabaseAsyncCacheTask(const FPoseSearchDatabaseAsyncCacheTask& Other) = delete;
	FPoseSearchDatabaseAsyncCacheTask(FPoseSearchDatabaseAsyncCacheTask&& Other) = delete;
	FPoseSearchDatabaseAsyncCacheTask& operator=(const FPoseSearchDatabaseAsyncCacheTask& Other) = delete;
	FPoseSearchDatabaseAsyncCacheTask& operator=(FPoseSearchDatabaseAsyncCacheTask&& Other) = delete;

	void OnGetComplete(UE::DerivedData::FCacheGetResponse&& Response);
	void SetState(EState State) { ThreadSafeState.Set(int32(State)); }

	TWeakObjectPtr<UPoseSearchDatabase> Database;
	FSearchIndex SearchIndex;
	UE::DerivedData::FRequestOwner Owner;
	FIoHash DerivedDataKey = FIoHash::Zero;
	TSet<TWeakObjectPtr<const UObject>> DatabaseDependencies;
		
	FThreadSafeCounter ThreadSafeState = int32(EState::Notstarted);
	bool bBroadcastOnDerivedDataRebuild = false;
};

class FPoseSearchDatabaseAsyncCacheTasks : public TArray<TUniquePtr<FPoseSearchDatabaseAsyncCacheTask>> {};

FPoseSearchDatabaseAsyncCacheTask::FPoseSearchDatabaseAsyncCacheTask(UPoseSearchDatabase* InDatabase, FCriticalSection& OuterMutex)
	: Database(InDatabase)
	, Owner(UE::DerivedData::EPriority::Normal)
	, DerivedDataKey(FIoHash::Zero)
{
	if (IsInGameThread())
	{
		// it is safe to compose DDC key only on the game thread, since assets can modified in this thread execution
		StartNewRequestIfNeeded(OuterMutex);
	}
	else
	{
		UE_LOG(LogPoseSearch, Log, TEXT("Delaying DDC until on the game thread    - %s"), *Database->GetName());
	}
}

FPoseSearchDatabaseAsyncCacheTask::~FPoseSearchDatabaseAsyncCacheTask()
{
	Database = nullptr;
	SearchIndex.Reset();
	Owner.Cancel();
	DerivedDataKey = FIoHash::Zero;
	DatabaseDependencies.Reset();
}

void FPoseSearchDatabaseAsyncCacheTask::StartNewRequestIfNeeded(FCriticalSection& OuterMutex)
{
	using namespace UE::DerivedData;

	check(IsInGameThread());

	FScopeLock Lock(&OuterMutex);

	// making sure there are no active requests
	Owner.Cancel();

	// composing the key
	const FKeyBuilder KeyBuilder(Database.Get(), true);
	if (KeyBuilder.AnyAssetNotReady())
	{
		DerivedDataKey = FIoHash::Zero;
		SetState(EState::Notstarted);
	
		UE_LOG(LogPoseSearch, Log, TEXT("Delaying DDC until dependents post load  - %s"), *Database->GetName());
	}
	else
	{
		const FIoHash NewDerivedDataKey(KeyBuilder.Finalize());
		const bool bHasKeyChanged = NewDerivedDataKey != DerivedDataKey;
		if (bHasKeyChanged)
		{
			DerivedDataKey = NewDerivedDataKey;

			DatabaseDependencies.Reset();
			for (const UObject* Dependency : KeyBuilder.GetDependencies())
			{
				DatabaseDependencies.Add(Dependency);
			}

			SetState(EState::Prestarted);

			UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BeginCache"), *LexToString(DerivedDataKey), *Database->GetName());

			TArray<FCacheGetRequest> CacheRequests;
			const FCacheKey CacheKey{ Bucket, DerivedDataKey };
			CacheRequests.Add({ { Database->GetPathName() }, CacheKey, ECachePolicy::Default });

			Owner = FRequestOwner(EPriority::Normal);
			GetCache().Get(CacheRequests, Owner, [this](FCacheGetResponse&& Response)
				{
					OnGetComplete(MoveTemp(Response));
				});
		}
	}
}

// it cancels and waits for the task to be done and reset the local SearchIndex. SetState to Cancelled
void FPoseSearchDatabaseAsyncCacheTask::Cancel(FCriticalSection& OuterMutex)
{
	check(IsInGameThread());

	FScopeLock Lock(&OuterMutex);

	Owner.Cancel();
	SearchIndex.Reset();
	DerivedDataKey = FIoHash::Zero;
	SetState(EState::Cancelled);
}

bool FPoseSearchDatabaseAsyncCacheTask::CancelIfDependsOn(const UObject* Object, FCriticalSection& OuterMutex)
{
	if (Object)
	{
		FScopeLock Lock(&OuterMutex);

		// DatabaseDependencies is updated only in StartNewRequestIfNeeded when there are no active requests, so it's thread safe to access it 
		if (DatabaseDependencies.Contains(Object))
		{
			UE_LOG(LogPoseSearch, Log, TEXT("%s - %s Cancelled because of %s"), *LexToString(DerivedDataKey), *Database->GetName(), *Object->GetName());
			Cancel(OuterMutex);
			return true;
		}
	}
	return false;
}

void FPoseSearchDatabaseAsyncCacheTask::Update(FCriticalSection& OuterMutex)
{
	check(IsInGameThread());

	FScopeLock Lock(&OuterMutex);

	check(GetState() != EState::Cancelled); // otherwise FPoseSearchDatabaseAsyncCacheTask should have been already removed

	if (GetState() == EState::Notstarted)
	{
		StartNewRequestIfNeeded(OuterMutex);
	}

	if (GetState() == EState::Prestarted && Poll(OuterMutex))
	{
		// task is done: we need to update the state form Prestarted to Ended/Failed
		Wait(OuterMutex);
	}

	if (bBroadcastOnDerivedDataRebuild)
	{
		Database->NotifyDerivedDataRebuild();
		bBroadcastOnDerivedDataRebuild = false;
	}
}

// it waits for the task to be done and SetSearchIndex on the database. SetState to Ended/Failed
void FPoseSearchDatabaseAsyncCacheTask::Wait(FCriticalSection& OuterMutex)
{
	check(GetState() == EState::Prestarted);

	Owner.Wait();

	FScopeLock Lock(&OuterMutex);

	const bool bFailedIndexing = SearchIndex.IsEmpty();
	if (!bFailedIndexing)
	{
		Database->SetSearchIndex(SearchIndex); // @todo: implement FSearchIndex move ctor and assignment operator and use a MoveTemp(SearchIndex) here

		check(Database->Schema && Database->Schema->IsValid() && !SearchIndex.IsEmpty() && SearchIndex.WeightsSqrt.Num() == Database->Schema->SchemaCardinality);

		SetState(EState::Ended);
		bBroadcastOnDerivedDataRebuild = true;
	}
	else
	{
		check(!bBroadcastOnDerivedDataRebuild);
		SetState(EState::Failed);
	}
	SearchIndex.Reset();
}

// true is the task is done executing
bool FPoseSearchDatabaseAsyncCacheTask::Poll(FCriticalSection& OuterMutex) const
{
	return Owner.Poll();
}

bool FPoseSearchDatabaseAsyncCacheTask::ContainsDatabase(const UPoseSearchDatabase* OtherDatabase, FCriticalSection& OuterMutex) const
{
	FScopeLock Lock(&OuterMutex);
	return Database.Get() == OtherDatabase;
}

bool FPoseSearchDatabaseAsyncCacheTask::IsValid(FCriticalSection& OuterMutex) const
{
	FScopeLock Lock(&OuterMutex);
	return Database.IsValid();
}

// called once the task is done:
// if EStatus::Ok (data has been retrieved from DDC) we deserialize the payload into the local SearchIndex
// if EStatus::Error we BuildIndex and if that's successful we 'Put' it on DDC
void FPoseSearchDatabaseAsyncCacheTask::OnGetComplete(UE::DerivedData::FCacheGetResponse&& Response)
{
	using namespace UE::DerivedData;

	const FCacheKey FullIndexKey = Response.Record.GetKey();

	// The database is part of the derived data cache and up to date, skip re-building it.
	bool bCacheCorrupted = false;
	if (Response.Status == EStatus::Ok)
	{
		COOK_STAT(auto Timer = UsageStats.TimeAsyncWait());

		// we found the cached data associated to the PendingDerivedDataKey: we'll deserialized into SearchIndex
		SearchIndex.Reset();
		FSharedBuffer RawData = Response.Record.GetValue(Id).GetData().Decompress();
		FMemoryReaderView Reader(RawData);
		Reader << SearchIndex;

		check(Database->Schema && Database->Schema->IsValid());
		// cache can be corrupted in case the version of the derived data cache has not being updated while 
		// developing channels that changes their cardinality without impacting any asset properties
		// so to account for this, we just reindex the database and update the associated DDC 
		if (!SearchIndex.IsEmpty() && SearchIndex.WeightsSqrt.Num() == Database->Schema->SchemaCardinality)
		{
			UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex From Cache"), *LexToString(FullIndexKey.Hash), *Database->GetName());
		}
		else
		{
			UE_LOG(LogPoseSearch, Warning, TEXT("%s - %s BuildIndex From Cache Corrupted!"), *LexToString(FullIndexKey.Hash), *Database->GetName());
			bCacheCorrupted = true;
		}

		COOK_STAT(Timer.AddHit(RawData.GetSize()));
	}
	
	if (Response.Status == EStatus::Canceled)
	{
		SearchIndex.Reset();
		UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Cancelled"), *LexToString(FullIndexKey.Hash), *Database->GetName());
	}
	
	if (Response.Status == EStatus::Error || bCacheCorrupted)
	{
		// we didn't find the cached data associated to the PendingDerivedDataKey: we'll BuildIndex to update SearchIndex and "Put" the data over the DDC
		Owner.LaunchTask(TEXT("PoseSearchDatabaseBuild"), [this, FullIndexKey]
			{
				COOK_STAT(auto Timer = UsageStats.TimeSyncWork());

				// collecting all the databases that need to be built to gather their FSearchIndexBase
				TArray<TObjectPtr<const UPoseSearchDatabase>> IndexBaseDatabases;
				IndexBaseDatabases.Add(Database.Get()); // the first one is always this Database
				if (Database->NormalizationSet)
				{
					Database->NormalizationSet->AddUniqueDatabases(IndexBaseDatabases);
				}

				// @todo: DDC or parallelize this code
				TArray<FSearchIndexBase> SearchIndexBases;
				TArray<const UPoseSearchSchema*> Schemas;
				SearchIndexBases.AddDefaulted(IndexBaseDatabases.Num());
				Schemas.AddDefaulted(IndexBaseDatabases.Num());
				for (int32 IndexBaseIdx = 0; IndexBaseIdx < IndexBaseDatabases.Num(); ++IndexBaseIdx)
				{
					const UPoseSearchDatabase* IndexBaseDatabase = IndexBaseDatabases[IndexBaseIdx].Get();
					check(IndexBaseDatabase);
					FSearchIndexBase& SearchIndexBase = SearchIndexBases[IndexBaseIdx];
					Schemas[IndexBaseIdx] = IndexBaseDatabase->Schema;

					// early out for invalid indexing conditions
					if (!IndexBaseDatabase->Schema || !IndexBaseDatabase->Schema->IsValid() || IndexBaseDatabase->Schema->SchemaCardinality <= 0)
					{
						if (IndexBaseDatabase == Database)
						{
							UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Failed"), *LexToString(FullIndexKey.Hash), *Database->GetName());
						}
						else
						{
							UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Failed because of dependent database fail '%s'"), *LexToString(FullIndexKey.Hash), *Database->GetName(), *IndexBaseDatabase->GetName());
						}
						SearchIndex.Reset();
						return;
					}

					// validating that the missing MirrorDataTable is not necessary 
					if (!IndexBaseDatabase->Schema->MirrorDataTable)
					{
						for (int32 AnimationAssetIndex = 0; AnimationAssetIndex < IndexBaseDatabase->AnimationAssets.Num(); ++AnimationAssetIndex)
						{
							if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = IndexBaseDatabase->GetAnimationAssetBase(AnimationAssetIndex))
							{
								if (DatabaseAsset->GetMirrorOption() == EPoseSearchMirrorOption::MirroredOnly || DatabaseAsset->GetMirrorOption() == EPoseSearchMirrorOption::UnmirroredAndMirrored)
								{
									// want to sample a mirrored asset
									UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Failed because '%s' requires a MirrorDataTable to sample mirrored animation assets"), *LexToString(FullIndexKey.Hash), *Database->GetName(), *IndexBaseDatabase->Schema->GetName());
									SearchIndex.Reset();
									return;
								}
							}
						}
					}

					if (Owner.IsCanceled())
					{
						UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Cancelled"), *LexToString(FullIndexKey.Hash), *Database->GetName());
						SearchIndex.Reset();
						return;
					}

					// Building all the related FPoseSearchBaseIndex first
					InitSearchIndexAssets(SearchIndexBase, IndexBaseDatabase);

					if (Owner.IsCanceled())
					{
						UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Cancelled"), *LexToString(FullIndexKey.Hash), *Database->GetName());
						SearchIndex.Reset();
						return;
					}

					FDatabaseIndexingContext DbIndexingContext;
					if (!DbIndexingContext.IndexDatabase(SearchIndexBase, *IndexBaseDatabase, Owner))
					{
						if (Owner.IsCanceled())
						{
							UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Cancelled"), *LexToString(FullIndexKey.Hash), *Database->GetName());
						}
						else if (IndexBaseDatabase == Database)
						{
							UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Failed"), *LexToString(FullIndexKey.Hash), *Database->GetName());
						}
						else
						{
							UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Failed because of dependent database fail '%s'"), *LexToString(FullIndexKey.Hash), *Database->GetName(), *IndexBaseDatabase->GetName());
						}

						SearchIndex.Reset();
						return;
					}
				}

				static_cast<FSearchIndexBase&>(SearchIndex) = SearchIndexBases[0];
				
				TArray<float> Deviation = FMeanDeviationCalculator::Calculate(SearchIndexBases, Schemas);

				// Building FSearchIndex
				PreprocessSearchIndexWeights(SearchIndex, Database->Schema, Deviation);
				if (Owner.IsCanceled())
				{
					UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Cancelled"), *LexToString(FullIndexKey.Hash), *Database->GetName());
					SearchIndex.Reset();
					return;
				}

				PreprocessSearchIndexPCAData(SearchIndex, Database->Schema->SchemaCardinality, Database->GetNumberOfPrincipalComponents(), Database->PoseSearchMode);
				if (Owner.IsCanceled())
				{
					UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Cancelled"), *LexToString(FullIndexKey.Hash), *Database->GetName());
					SearchIndex.Reset();
					return;
				}

				PreprocessSearchIndexKDTree(SearchIndex, Database->Schema->SchemaCardinality, Database->GetNumberOfPrincipalComponents(), Database->PoseSearchMode, Database->KDTreeMaxLeafSize, Database->KDTreeQueryNumNeighbors);
				if (Owner.IsCanceled())
				{
					UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Cancelled"), *LexToString(FullIndexKey.Hash), *Database->GetName());
					SearchIndex.Reset();
					return;
				}

				// removing SearchIndex.Values and relying on FSearchIndex::GetReconstructedPoseValues to reconstruct the Values data from the PCAValues
				if (Database->PoseSearchMode == EPoseSearchMode::PCAKDTree && Database->KDTreeQueryNumNeighbors <= 1)
				{
					SearchIndex.Values.Reset();
				}

				UE_LOG(LogPoseSearch, Log, TEXT("%s - %s BuildIndex Succeeded"), *LexToString(FullIndexKey.Hash), *Database->GetName());

				// putting SearchIndex to DDC
				TArray<uint8> RawBytes;
				FMemoryWriter Writer(RawBytes);
				Writer << SearchIndex;
				FSharedBuffer RawData = MakeSharedBufferFromArray(MoveTemp(RawBytes));
				const int32 BytesProcessed = RawData.GetSize();

				FCacheRecordBuilder Builder(FullIndexKey);
				Builder.AddValue(Id, RawData);
				GetCache().Put({ { { Database->GetPathName() }, Builder.Build() } }, Owner, [this, FullIndexKey](FCachePutResponse&& Response)
					{
						if (Response.Status == EStatus::Error)
						{
							UE_LOG(LogPoseSearch, Log, TEXT("%s - %s Failed to store DDC"), *LexToString(FullIndexKey.Hash), *Database->GetName());
						}
					});

				COOK_STAT(Timer.AddMiss(BytesProcessed));
			});
	}
}

void FPoseSearchDatabaseAsyncCacheTask::AddReferencedObjects(FReferenceCollector& Collector, FCriticalSection& OuterMutex)
{
	FScopeLock Lock(&OuterMutex);

	const EState State = GetState();
	if (State != EState::Ended && State != EState::Failed)
	{
		// keeping around the assets for starting or in progress tasks
		Collector.AddReferencedObject(Database);

		for (TWeakObjectPtr<const UObject>& Dependency : DatabaseDependencies )
		{
			Collector.AddReferencedObject(Dependency);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FAsyncPoseSearchDatabasesManagement
FCriticalSection FAsyncPoseSearchDatabasesManagement::Mutex;

FAsyncPoseSearchDatabasesManagement& FAsyncPoseSearchDatabasesManagement::Get()
{
	FScopeLock Lock(&Mutex);

	static FAsyncPoseSearchDatabasesManagement SingletonInstance;
	return SingletonInstance;
}

FAsyncPoseSearchDatabasesManagement::FAsyncPoseSearchDatabasesManagement()
	: Tasks(*(new FPoseSearchDatabaseAsyncCacheTasks()))
{
	FScopeLock Lock(&Mutex);

	OnObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FAsyncPoseSearchDatabasesManagement::OnObjectModified);
	OnPackageReloadedHandle = FCoreUObjectDelegates::OnPackageReloaded.AddRaw(this, &FAsyncPoseSearchDatabasesManagement::OnPackageReloaded);

	FCoreDelegates::OnPreExit.AddRaw(this, &FAsyncPoseSearchDatabasesManagement::Shutdown);
}

FAsyncPoseSearchDatabasesManagement::~FAsyncPoseSearchDatabasesManagement()
{
	FScopeLock Lock(&Mutex);

	FCoreDelegates::OnPreExit.RemoveAll(this);
	Shutdown();

	delete &Tasks;
}

// we're listening to OnObjectModified to cancel any pending Task indexing databases depending from Object to avoid multi threading issues
void FAsyncPoseSearchDatabasesManagement::OnObjectModified(UObject* Object)
{
	check(IsInGameThread());

	FScopeLock Lock(&Mutex);

	// iterating backwards because of the possible RemoveAtSwap
	for (int32 TaskIndex = Tasks.Num() - 1; TaskIndex >= 0; --TaskIndex)
	{
		if (Tasks[TaskIndex]->CancelIfDependsOn(Object, Mutex))
		{
			Tasks.RemoveAtSwap(TaskIndex, 1, false);
		}
	}
}

void FAsyncPoseSearchDatabasesManagement::OnPackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent)
{
	check(IsInGameThread());

	if (InPackageReloadPhase == EPackageReloadPhase::PostPackageFixup && InPackageReloadedEvent)
	{
		FScopeLock Lock(&Mutex);

		// @todo: figure out why we don't find the correct dependency into InPackageReloadedEvent->GetRepointedObjects()
		//		  for now we invalidate all the DDC cache to be on the safe side
		//for (const TPair<UObject*, UObject*>& Pair : InPackageReloadedEvent->GetRepointedObjects())
		//{
		//	OnObjectModified(Pair.Key);
		//}

		for (TUniquePtr<FPoseSearchDatabaseAsyncCacheTask>& TaskPtr : Tasks)
		{
			UE_LOG(LogPoseSearch, Log, TEXT("%s - %s Cancelled because of OnPackageReloaded"), *LexToString(TaskPtr->GetDerivedDataKey()), *TaskPtr->GetDatabase()->GetName());
		}
		Tasks.Reset();
	}
}

void FAsyncPoseSearchDatabasesManagement::Shutdown()
{
	FScopeLock Lock(&Mutex);

	FCoreUObjectDelegates::OnObjectModified.Remove(OnObjectModifiedHandle);
	OnObjectModifiedHandle.Reset();

	FCoreUObjectDelegates::OnPackageReloaded.Remove(OnPackageReloadedHandle);
	OnPackageReloadedHandle.Reset();
}

void FAsyncPoseSearchDatabasesManagement::Tick(float DeltaTime)
{
	FScopeLock Lock(&Mutex);

	check(IsInGameThread());

	// iterating backwards because of the possible RemoveAtSwap 
	for (int32 TaskIndex = Tasks.Num() - 1; TaskIndex >= 0; --TaskIndex)
	{
		if (!Tasks[TaskIndex]->IsValid(Mutex))
		{
			Tasks.RemoveAtSwap(TaskIndex, 1, false);
		}
		else
		{
			Tasks[TaskIndex]->Update(Mutex);
		}
			
		// @todo: check key validity every few ticks, or perhaps delete unused for a long time Tasks
	}
}

void FAsyncPoseSearchDatabasesManagement::TickCook(float DeltaTime, bool bCookCompete)
{
	Tick(DeltaTime);
}

TStatId FAsyncPoseSearchDatabasesManagement::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncPoseSearchDatabasesManagement, STATGROUP_Tickables);
}

void FAsyncPoseSearchDatabasesManagement::AddReferencedObjects(FReferenceCollector& Collector)
{
	FScopeLock Lock(&Mutex);

	for (TUniquePtr<FPoseSearchDatabaseAsyncCacheTask>& TaskPtr : Tasks)
	{
		TaskPtr->AddReferencedObjects(Collector, Mutex);
	}
}

// returns true if the index has been built and the Database updated correctly  
bool FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(const UPoseSearchDatabase* Database, ERequestAsyncBuildFlag Flag)
{
	if (!Database)
	{
		return false;
	}

	FScopeLock Lock(&Mutex);

	check(Database);
	check(EnumHasAnyFlags(Flag, ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::ContinueRequest));

	FAsyncPoseSearchDatabasesManagement& This = FAsyncPoseSearchDatabasesManagement::Get();

	FPoseSearchDatabaseAsyncCacheTask* Task = nullptr;
	for (TUniquePtr<FPoseSearchDatabaseAsyncCacheTask>& TaskPtr : This.Tasks)
	{
		if (TaskPtr->ContainsDatabase(Database, Mutex))
		{
			Task = TaskPtr.Get();

			if (EnumHasAnyFlags(Flag, ERequestAsyncBuildFlag::NewRequest))
			{
				if (Task->GetState() == FPoseSearchDatabaseAsyncCacheTask::EState::Prestarted)
				{
					Task->Cancel(Mutex);
				}
				Task->StartNewRequestIfNeeded(Mutex);
			}
			break;
		}
	}
		
	if (!Task)
	{
		// we didn't find the Task, so we Emplace a new one
		This.Tasks.Emplace(MakeUnique<FPoseSearchDatabaseAsyncCacheTask>(const_cast<UPoseSearchDatabase*>(Database), Mutex));
		Task = This.Tasks.Last().Get();
	}

	if (EnumHasAnyFlags(Flag, ERequestAsyncBuildFlag::WaitForCompletion))
	{
		check(Task->GetState() != FPoseSearchDatabaseAsyncCacheTask::EState::Notstarted);
		if (Task->GetState() == FPoseSearchDatabaseAsyncCacheTask::EState::Prestarted)
		{
			Task->Wait(Mutex);
		}
	}

	return Task->GetState() == FPoseSearchDatabaseAsyncCacheTask::EState::Ended;
}

} // namespace UE::PoseSearch
#endif // WITH_EDITOR
