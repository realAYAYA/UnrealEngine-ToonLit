// Copyright Epic Games, Inc. All Rights Reserved.

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "rltests/Defs.h"
#include "rltests/StorageValueType.h"
#include "rltests/ml/cpu/FixturesBlock4.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/cpu/Factory.h"
#include "riglogic/system/simd/Detect.h"

#include <tuple>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4324)
#endif

namespace rl4 {

namespace ml {

namespace cpu {

template<typename T, typename TF256, typename TF128>
struct Evaluator<T, TF256, TF128>::Accessor {

    static void assertRawDataEqual(const Evaluator<T, TF256, TF128>& result) {
        ASSERT_EQ(result.lods.netIndicesPerLOD, rltests::ml::block4::optimized::lods.netIndicesPerLOD);
        ASSERT_EQ(result.lods.netCount, rltests::ml::block4::optimized::lods.netCount);
        ASSERT_EQ(result.neuralNets.size(), rltests::ml::block4::optimized::lods.netCount);
        for (std::uint16_t netIndex = {}; netIndex < rltests::ml::block4::unoptimized::neuralNetworkCount; ++netIndex) {
            const auto& inference = result.neuralNets[netIndex];
            ASSERT_EQ(inference.layerEvaluators.size(),
                      rltests::ml::block4::unoptimized::mlbNetActivationFunctions[netIndex].size());
            ASSERT_EQ(inference.neuralNet.layers.size(),
                      rltests::ml::block4::unoptimized::mlbNetActivationFunctions[netIndex].size());
            for (std::uint16_t layerIndex = {}; layerIndex < inference.neuralNet.layers.size(); ++layerIndex) {
                const auto& layer = inference.neuralNet.layers[layerIndex];
                using OptimizedValues = typename rltests::ml::block4::optimized::Values<T>;
                ASSERT_EQ(layer.weights.values, OptimizedValues::weights()[netIndex][layerIndex]);
                ASSERT_EQ(layer.biases, OptimizedValues::biases()[netIndex][layerIndex]);
                ASSERT_EQ(layer.activationFunction,
                          rltests::ml::block4::unoptimized::mlbNetActivationFunctions[netIndex][layerIndex]);
                ASSERT_EQ(layer.activationFunctionParameters,
                          rltests::ml::block4::unoptimized::mlbNetActivationFunctionParameters[netIndex][
                              layerIndex]);
            }
            ASSERT_EQ(inference.neuralNet.inputIndices,
                      rltests::ml::block4::unoptimized::mlbNetInputIndices[netIndex]);
            ASSERT_EQ(inference.neuralNet.outputIndices,
                      rltests::ml::block4::unoptimized::mlbNetOutputIndices[netIndex]);
        }
    }

};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

namespace {

template<typename TTestTypes>
class MLBSStorageBuilderTest : public ::testing::Test {
    protected:
        template<typename TestTypes = TTestTypes>
        typename std::enable_if<std::tuple_size<TestTypes>::value != 0ul, void>::type buildStorage() {
            using T = typename std::tuple_element<0, TTestTypes>::type;
            using TF256 = typename std::tuple_element<1, TTestTypes>::type;
            using TF128 = typename std::tuple_element<2, TTestTypes>::type;
            auto evaluator = rl4::ml::cpu::Factory<T, TF256, TF128>::create(&reader, &memRes);
            auto evaluatorImpl = static_cast<rl4::ml::cpu::Evaluator<T, TF256, TF128>*>(evaluator.get());
            rl4::ml::cpu::Evaluator<T, TF256, TF128>::Accessor::assertRawDataEqual(*evaluatorImpl);
        }

        template<typename TestTypes = TTestTypes>
        typename std::enable_if<std::tuple_size<TestTypes>::value == 0ul, void>::type buildStorage() {
        }

    protected:
        pma::AlignedMemoryResource memRes;
        rltests::ml::block4::CanonicalReader reader;

};

}  // namespace

#if defined(RL_BUILD_WITH_AVX) && defined(RL_BUILD_WITH_SSE)
    using StorageValueTypeList = ::testing::Types<
    #ifdef RL_USE_HALF_FLOATS
            std::tuple<StorageValueType, trimd::avx::F256, trimd::sse::F128>,
            std::tuple<StorageValueType, trimd::sse::F256, trimd::sse::F128>
    #else
            std::tuple<StorageValueType, trimd::avx::F256, trimd::sse::F128>,
            std::tuple<StorageValueType, trimd::sse::F256, trimd::sse::F128>,
            std::tuple<StorageValueType, trimd::scalar::F256, trimd::scalar::F128>
    #endif  // RL_USE_HALF_FLOATS
        >;
#elif defined(RL_BUILD_WITH_AVX)
    using StorageValueTypeList = ::testing::Types<
    #ifdef RL_USE_HALF_FLOATS
            std::tuple<StorageValueType, trimd::avx::F256, trimd::sse::F128>
    #else
            std::tuple<StorageValueType, trimd::avx::F256, trimd::sse::F128>,
            std::tuple<StorageValueType, trimd::scalar::F256, trimd::scalar::F128>
    #endif  // RL_USE_HALF_FLOATS
        >;
#elif defined(RL_BUILD_WITH_SSE)
    using StorageValueTypeList = ::testing::Types<
    #ifdef RL_USE_HALF_FLOATS
            std::tuple<StorageValueType, trimd::sse::F256, trimd::sse::F128>
    #else
            std::tuple<StorageValueType, trimd::sse::F256, trimd::sse::F128>,
            std::tuple<StorageValueType, trimd::scalar::F256, trimd::scalar::F128>
    #endif  // RL_USE_HALF_FLOATS
        >;
#else
    #ifndef RL_USE_HALF_FLOATS
        using StorageValueTypeList = ::testing::Types<std::tuple<StorageValueType, trimd::scalar::F256, trimd::scalar::F128> >;
    #else
        using StorageValueTypeList = ::testing::Types<std::tuple<> >;
    #endif  // RL_USE_HALF_FLOATS
#endif

TYPED_TEST_SUITE(MLBSStorageBuilderTest, StorageValueTypeList, );

TYPED_TEST(MLBSStorageBuilderTest, LayoutOptimization) {
    this->buildStorage();
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
