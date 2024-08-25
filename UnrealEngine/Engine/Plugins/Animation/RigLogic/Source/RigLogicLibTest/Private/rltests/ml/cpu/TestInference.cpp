// Copyright Epic Games, Inc. All Rights Reserved.

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "rltests/Defs.h"
#include "rltests/StorageValueType.h"
#include "rltests/controls/ControlFixtures.h"
#include "rltests/ml/cpu/FixturesBlock4.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/cpu/Factory.h"
#include "riglogic/system/simd/Detect.h"
#include "riglogic/system/simd/SIMD.h"

#include <tuple>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4324)
#endif

namespace {

template<typename TTestTypes>
class MLBInferenceTest : public ::testing::Test {
    protected:
        void SetUp() override {
            MLBInferenceTest::SetUpImpl();
        }

        template<typename TestTypes = TTestTypes>
        typename std::enable_if<std::tuple_size<TestTypes>::value != 0ul, void>::type SetUpImpl() {
            using T = typename std::tuple_element<0, TestTypes>::type;
            using TF256 = typename std::tuple_element<1, TestTypes>::type;
            using TF128 = typename std::tuple_element<2, TestTypes>::type;
            evaluator = rl4::ml::cpu::Factory<T, TF256, TF128>::create(&reader, &memRes);
        }

        template<typename TestTypes = TTestTypes>
        typename std::enable_if<std::tuple_size<TestTypes>::value == 0ul, void>::type SetUpImpl() {
        }

    protected:
        pma::AlignedMemoryResource memRes;
        rltests::ml::block4::CanonicalReader reader;
        rl4::MachineLearnedBehaviorEvaluator::Pointer evaluator;

};

}  // namespace

#if defined(RL_BUILD_WITH_AVX) && defined(RL_BUILD_WITH_SSE)
    using InferenceTypeList = ::testing::Types<
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
    using InferenceTypeList = ::testing::Types<
    #ifdef RL_USE_HALF_FLOATS
            std::tuple<StorageValueType, trimd::avx::F256, trimd::sse::F128>
    #else
            std::tuple<StorageValueType, trimd::avx::F256, trimd::sse::F128>,
            std::tuple<StorageValueType, trimd::scalar::F256, trimd::scalar::F128>
    #endif  // RL_USE_HALF_FLOATS
        >;
#elif defined(RL_BUILD_WITH_SSE)
    using InferenceTypeList = ::testing::Types<
    #ifdef RL_USE_HALF_FLOATS
            std::tuple<StorageValueType, trimd::sse::F256, trimd::sse::F128>
    #else
            std::tuple<StorageValueType, trimd::sse::F256, trimd::sse::F128>,
            std::tuple<StorageValueType, trimd::scalar::F256, trimd::scalar::F128>
    #endif  // RL_USE_HALF_FLOATS
        >;
#else
    #ifndef RL_USE_HALF_FLOATS
        using InferenceTypeList = ::testing::Types<std::tuple<StorageValueType, trimd::scalar::F256, trimd::scalar::F128> >;
    #else
        using InferenceTypeList = ::testing::Types<std::tuple<> >;
    #endif  // RL_USE_HALF_FLOATS
#endif

TYPED_TEST_SUITE(MLBInferenceTest, InferenceTypeList, );

TYPED_TEST(MLBInferenceTest, InferencePerLOD) {
    if (this->evaluator == nullptr) {
        return;
    }

    auto inputInstanceFactory = ControlsFactory::getInstanceFactory(0,
                                                                    rltests::ml::block4::unoptimized::rawControlCount,
                                                                    0,
                                                                    rltests::ml::block4::unoptimized::mlControlCount);
    auto inputInstance = inputInstanceFactory(&this->memRes);
    auto inputBuffer = inputInstance->getInputBuffer();
    auto outputBuffer = inputBuffer.subview(rltests::ml::block4::unoptimized::rawControlCount,
                                            rltests::ml::block4::unoptimized::mlControlCount);
    std::copy(rltests::ml::block4::input::values.begin(), rltests::ml::block4::input::values.end(), inputBuffer.begin());

    auto intermediateOutputs = this->evaluator->createInstance(&this->memRes);

    for (std::uint16_t lod = 0u; lod < rltests::ml::block4::unoptimized::lodCount; ++lod) {
        std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
        this->evaluator->calculate(inputInstance.get(), intermediateOutputs.get(), lod);
        const auto& expected = rltests::ml::block4::output::valuesPerLOD[lod];
        #ifdef RL_USE_HALF_FLOATS
            static constexpr float threshold = 0.05f;
        #else
            static constexpr float threshold = 0.0001f;
        #endif  // RL_USE_HALF_FLOATS
        ASSERT_ELEMENTS_NEAR(outputBuffer, expected, expected.size(), threshold);
    }
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
