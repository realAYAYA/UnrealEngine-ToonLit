// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "rltests/Defs.h"
#include "rltests/StorageValueType.h"
#include "rltests/dna/FakeReader.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/LODSpec.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstdint>
#include <functional>
#include <memory>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rltests {

namespace ml {

namespace block4 {

using namespace rl4;

namespace unoptimized {

extern const std::uint16_t rawControlCount;
extern const std::uint16_t mlControlCount;
extern const std::uint16_t lodCount;
extern const std::uint16_t neuralNetworkCount;
extern const Matrix<std::uint16_t> neuralNetworkIndicesPerLOD;

// [neuralNetIndex][inputIndex]
extern const Matrix<std::uint16_t> mlbNetInputIndices;
// [neuralNetIndex][outputIndex]
extern const Matrix<std::uint16_t> mlbNetOutputIndices;
// [neuralNetIndex][layerIndex]
extern const Matrix<dna::ActivationFunction> mlbNetActivationFunctions;
// [neuralNetIndex][layerIndex][paramIndex]
extern const Vector<Matrix<float> > mlbNetActivationFunctionParameters;
// [neuralNetIndex][layerIndex][weightIndex]
extern const Vector<Matrix<float> > mlbNetWeights;
// [neuralNetIndex][layerIndex][biasIndex]
extern const Vector<Matrix<float> > mlbNetBiases;

}  // namespace unoptimized

namespace optimized {

extern const Vector<AlignedMatrix<float> > mlbNetWeightsFloat;
extern const Vector<AlignedMatrix<std::uint16_t> > mlbNetWeightsHalfFloat;
extern const Vector<AlignedMatrix<float> > mlbNetBiasesFloat;
extern const Vector<AlignedMatrix<std::uint16_t> > mlbNetBiasesHalfFloat;
extern const LODSpec lods;

template<typename TValue>
struct Values {
    static const rl4::Vector<rl4::AlignedMatrix<TValue> >& weights();
    static const rl4::Vector<rl4::AlignedMatrix<TValue> >& biases();
};

}  // namespace optimized

namespace input {

// Calculation input values
extern const Vector<float> values;

}  // namespace input

namespace output {

// Calculation output values
extern const Matrix<float> valuesPerLOD;

}  // namespace output

class CanonicalReader : public dna::FakeReader {
    public:
        ~CanonicalReader();

        std::uint16_t getLODCount() const override {
            return unoptimized::lodCount;
        }

        std::uint16_t getNeuralNetworkCount() const override {
            return unoptimized::neuralNetworkCount;
        }

        ConstArrayView<std::uint16_t> getNeuralNetworkIndicesForLOD(std::uint16_t lod) const override {
            return ConstArrayView<std::uint16_t>{unoptimized::neuralNetworkIndicesPerLOD[lod]};
        }

        ConstArrayView<std::uint16_t> getNeuralNetworkInputIndices(std::uint16_t neuralNetIndex) const override {
            return ConstArrayView<std::uint16_t>{unoptimized::mlbNetInputIndices[neuralNetIndex]};
        }

        ConstArrayView<std::uint16_t> getNeuralNetworkOutputIndices(std::uint16_t neuralNetIndex) const override {
            return ConstArrayView<std::uint16_t>{unoptimized::mlbNetOutputIndices[neuralNetIndex]};
        }

        std::uint16_t getNeuralNetworkLayerCount(std::uint16_t neuralNetIndex) const override {
            return static_cast<std::uint16_t>(unoptimized::mlbNetWeights[neuralNetIndex].size());
        }

        dna::ActivationFunction getNeuralNetworkLayerActivationFunction(std::uint16_t neuralNetIndex,
                                                                        std::uint16_t layerIndex) const override {
            return unoptimized::mlbNetActivationFunctions[neuralNetIndex][layerIndex];
        }

        ConstArrayView<float> getNeuralNetworkLayerActivationFunctionParameters(std::uint16_t neuralNetIndex,
                                                                                std::uint16_t layerIndex) const override {
            return ConstArrayView<float>{unoptimized::mlbNetActivationFunctionParameters[neuralNetIndex][layerIndex]};
        }

        ConstArrayView<float> getNeuralNetworkLayerBiases(std::uint16_t neuralNetIndex, std::uint16_t layerIndex) const override {
            return ConstArrayView<float>{unoptimized::mlbNetBiases[neuralNetIndex][layerIndex]};
        }

        ConstArrayView<float> getNeuralNetworkLayerWeights(std::uint16_t neuralNetIndex,
                                                           std::uint16_t layerIndex) const override {
            return ConstArrayView<float>{unoptimized::mlbNetWeights[neuralNetIndex][layerIndex]};
        }

};

}  // namespace block4

}  // namespace ml

}  // namespace rltests

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
