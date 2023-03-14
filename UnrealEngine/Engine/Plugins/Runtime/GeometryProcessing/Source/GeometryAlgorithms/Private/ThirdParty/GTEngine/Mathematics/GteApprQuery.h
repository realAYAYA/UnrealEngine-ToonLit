// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <algorithm>
#include <random>
#include <vector>

// Class ApprQuery supports the RANSAC algorithm for fitting and uses the
// Curiously Recurring Template Pattern.  The ModelType must be a class or
// struct with the following interfaces:
//
//   // The minimum number of observations required to fit the model.
//   int ModelType::GetMinimumRequired() const;
//
//   // Compute the model error for the specified observation for the current
//   // model parameters.
//   Real Error(ObservationType const& observation) const;
//
//   // Estimate the model parameters for all observations specified by the
//   // indices.  The three Fit() functions of ApprQuery manipulate their
//   // inputs in order to pass them to ModelType::Fit().
//   ModelType::Fit(std::vector<ObservationType> const& observations,
//       std::vector<size_t> const& indices);

namespace gte
{

template <typename Real, typename ModelType, typename ObservationType>
class ApprQuery
{
public:
    // Estimate the model parameters for all observations.
    bool Fit(std::vector<ObservationType> const& observations);

    // Estimate the model parameters for a contiguous subset of observations.
    bool Fit(std::vector<ObservationType> const& observations,
        int const imin, int const imax);

    // Estimate the model parameters for the subset of observations specified
    // by the indices and the number of indices that is possibly smaller than
    // indices.size().
    bool Fit(std::vector<ObservationType> const& observations,
        std::vector<int> const& indices, int const numIndices);

    // Apply the RANdom SAmple Consensus algorithm for fitting a model to
    // observations.
    static bool RANSAC(
        ModelType& candidateModel,
        std::vector<ObservationType> const& observations,
        int const numRequiredForGoodFit, Real const maxErrorForGoodFit,
        int const numIterations, std::vector<int>& bestConsensus,
        ModelType& bestModel);
};


template <typename Real, typename ModelType, typename ObservationType>
bool ApprQuery<Real, ModelType, ObservationType>::Fit(
    std::vector<ObservationType> const& observations)
{
    std::vector<int> indices(observations.size());
    int i = 0;
    for (auto& index : indices)
    {
        index = i++;
    }

    return ((ModelType*)this)->Fit(observations, indices);
}

template <typename Real, typename ModelType, typename ObservationType>
bool ApprQuery<Real, ModelType, ObservationType>::Fit(
    std::vector<ObservationType> const& observations,
    int const imin, int const imax)
{
    if (imin <= imax)
    {
        int numIndices = imax - imin + 1;
        std::vector<int> indices(numIndices);
        int i = imin;
        for (auto& index : indices)
        {
            index = i++;
        }

        return ((ModelType*)this)->Fit(observations, indices);
    }
    else
    {
        return false;
    }
}

template <typename Real, typename ModelType, typename ObservationType>
bool ApprQuery<Real, ModelType, ObservationType>::Fit(
    std::vector<ObservationType> const& observations,
    std::vector<int> const& indices, int const numIndices)
{
    int imax = std::min(numIndices, static_cast<int>(observations.size()));
    std::vector<int> localindices(imax);
    int i = 0;
    for (auto& index : localindices)
    {
        index = indices[i++];
    }

    return ((ModelType*)this)->Fit(observations, indices);
}

template <typename Real, typename ModelType, typename ObservationType>
bool ApprQuery<Real, ModelType, ObservationType>::RANSAC(
    ModelType& candidateModel,
    std::vector<ObservationType> const& observations,
    int const numRequiredForGoodFit, Real const maxErrorForGoodFit,
    int const numIterations, std::vector<int>& bestConsensus,
    ModelType& bestModel)
{
    int const numObservations = static_cast<int>(observations.size());
    int const minRequired = candidateModel.GetMinimumRequired();
    if (numObservations < minRequired)
    {
        // Too few observations for model fitting.
        return false;
    }

    // The first part of the array will store the consensus set, initially
    // filled with the minimumu number of indices that correspond to the
    // candidate inliers.  The last part will store the remaining indices.
    // These points are tested against the model and are added to the
    // consensus set when they fit.  All the index manipulation is done
    // in place.  Initially, the candidates are the identity permutation.
    std::vector<int> candidates(numObservations);
    int j = 0;
    for (auto& c : candidates)
    {
        c = j++;
    }

    if (numObservations == minRequired)
    {
        // We have the minimum number of observations to generate the model,
        // so RANSAC cannot be used.  Compute the model with the entire set
        // of observations.
        bestConsensus = candidates;
        return bestModel.Fit(observations);
    }

    int bestNumFittedObservations = minRequired;

    for (int i = 0; i < numIterations; ++i)
    {
        // Randomly permute the previous candidates, partitioning the array
        // into GetMinimumRequired() indices (the candidate inliers) followed
        // by the remaining indices (candidates for testing against the
        // model).
        std::shuffle(candidates.begin(), candidates.end(),
            std::default_random_engine());

        // Fit the model to the inliers.
        if (candidateModel.Fit(observations, candidates, minRequired))
        {
            // Test each remaining observation whether it fits the model.  If
            // it does, include it in the consensus set.
            int numFittedObservations = minRequired;
            for (j = minRequired; j < numObservations; ++j)
            {
                if (candidateModel.Error(observations[candidates[j]])
                    <= maxErrorForGoodFit)
                {
                    std::swap(candidates[j],
                        candidates[numFittedObservations]);
                    ++numFittedObservations;
                }
            }

            if (numFittedObservations >= numRequiredForGoodFit)
            {
                // We have observations that fit the model.  Update the best
                // model using the consensus set.
                candidateModel.Fit(observations, candidates,
                    numFittedObservations);
                if (numFittedObservations > bestNumFittedObservations)
                {
                    // The consensus set is larger than the previous consensus
                    // set, so its model becomes the best one.
                    bestModel = candidateModel;
                    bestConsensus.resize(numFittedObservations);
                    std::copy(candidates.begin(),
                        candidates.begin() + numFittedObservations,
                        bestConsensus.begin());
                    bestNumFittedObservations = numFittedObservations;
                }
            }
        }
    }

    return bestNumFittedObservations >= numRequiredForGoodFit;
}


}
