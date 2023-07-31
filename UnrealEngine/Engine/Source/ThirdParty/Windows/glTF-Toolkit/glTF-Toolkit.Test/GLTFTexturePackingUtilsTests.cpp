// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.
#include "pch.h"
#include <CppUnitTest.h>  

#include "GLTFSDK/IStreamWriter.h"
#include "GLTFSDK/Constants.h"
#include "GLTFSDK/Serialize.h"
#include "GLTFSDK/Deserialize.h"
#include "GLTFSDK/GLBResourceReader.h"
#include "GLTFSDK/GLTFResourceWriter.h"

#include "GLTFTexturePackingUtils.h"

#include "Helpers/WStringUtils.h"
#include "Helpers/TestUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

namespace Microsoft::glTF::Toolkit::Test
{
    TEST_CLASS(GLTFTexturePackingUtilsTests)
    {
        // Asset with no materials
        const char* c_cubeAsset3DJson = "Resources\\gltf\\cubeAsset3D.gltf";

        // Asset with loose images and all textures
        const char* c_waterBottleJson = "Resources\\gltf\\WaterBottle\\WaterBottle.gltf";

        // Loads and packs a complex asset with the specified packing
        void ExecutePackingTest(const char* gltfRelativePath, TexturePacking packing)
        {
            // This asset has all textures
            TestUtils::LoadAndExecuteGLTFTest(gltfRelativePath, [packing](auto doc, auto path)
            {
                auto material = doc.materials.Elements()[0];
                auto packedDoc = GLTFTexturePackingUtils::PackMaterialForWindowsMR(std::make_shared<TestStreamReader>(path), doc, material, packing, "");
                auto packedMaterial = packedDoc.materials.Elements()[0];

                // Check that the material changed
                Assert::IsTrue(material != packedMaterial);

                // Check that the new material replaces the old one
                Assert::IsTrue(material.id == packedMaterial.id);
                Assert::IsTrue(doc.materials.Size() == packedDoc.materials.Size());

                size_t expectedExtensionsSize = material.extensions.size();

                // Check the new extension is not empty
                if (packing & (TexturePacking::OcclusionRoughnessMetallic | TexturePacking::RoughnessMetallicOcclusion))
                {
                    expectedExtensionsSize++;

                    auto packingOrmExtension = packedMaterial.extensions.at(std::string(EXTENSION_MSFT_PACKING_ORM));
                    Assert::IsFalse(packingOrmExtension.empty());

                    // Check the new extension contains an ORM texture
                    rapidjson::Document ormJson;
                    ormJson.Parse(packingOrmExtension.c_str());

                    if (packing & TexturePacking::OcclusionRoughnessMetallic)
                    {
                        Assert::IsTrue(ormJson[MSFT_PACKING_ORM_ORMTEXTURE_KEY].IsObject());
                        Assert::IsTrue(ormJson[MSFT_PACKING_ORM_ORMTEXTURE_KEY].HasMember(MSFT_PACKING_INDEX_KEY));
                    }

                    if (packing & TexturePacking::RoughnessMetallicOcclusion)
                    {
                        Assert::IsTrue(ormJson[MSFT_PACKING_ORM_RMOTEXTURE_KEY].IsObject());
                        Assert::IsTrue(ormJson[MSFT_PACKING_ORM_RMOTEXTURE_KEY].HasMember(MSFT_PACKING_INDEX_KEY));
                    }

                    if (!material.normalTexture.textureId.empty())
                    {
                        // Check the new extension contains a normal texture
                        Assert::IsTrue(ormJson[MSFT_PACKING_ORM_NORMALTEXTURE_KEY].IsObject());
                        Assert::IsTrue(ormJson[MSFT_PACKING_ORM_NORMALTEXTURE_KEY].HasMember(MSFT_PACKING_INDEX_KEY));
                    }
                }

                if (packing & TexturePacking::NormalRoughnessMetallic)
                {
                    expectedExtensionsSize++;

                    auto packingNrmExtension = packedMaterial.extensions.at(std::string(EXTENSION_MSFT_PACKING_NRM));
                    Assert::IsFalse(packingNrmExtension.empty());

                    // Check the new extension contains an NRM texture
                    rapidjson::Document nrmJson;
                    nrmJson.Parse(packingNrmExtension.c_str());

                    Assert::IsTrue(nrmJson[MSFT_PACKING_NRM_KEY].IsObject());
                    Assert::IsTrue(nrmJson[MSFT_PACKING_NRM_KEY].HasMember(MSFT_PACKING_INDEX_KEY));
                }

                // Check that the packed material has the new extension
                Assert::IsTrue(expectedExtensionsSize == packedMaterial.extensions.size());
            });
        }

        TEST_METHOD(GLTFTexturePackingUtils_NoMaterials)
        {
            // This asset has no textures
            TestUtils::LoadAndExecuteGLTFTest(c_cubeAsset3DJson, [](auto doc, auto path)
            {
                auto material = doc.materials.Elements()[0];
                auto packedDoc = GLTFTexturePackingUtils::PackMaterialForWindowsMR(std::make_shared<TestStreamReader>(path), doc, material, TexturePacking::OcclusionRoughnessMetallic, "");

                // Check that nothing changed
                Assert::IsTrue(doc == packedDoc);
            });
        }

        TEST_METHOD(GLTFTexturePackingUtils_NoPacking)
        {
            // This asset has all textures
            TestUtils::LoadAndExecuteGLTFTest(c_waterBottleJson, [](auto doc, auto path)
            {
                auto material = doc.materials.Elements()[0];
                auto packedDoc = GLTFTexturePackingUtils::PackMaterialForWindowsMR(std::make_shared<TestStreamReader>(path), doc, material, TexturePacking::None, "");

                // Check that nothing changed
                Assert::IsTrue(doc == packedDoc);
            });
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackORM)
        {
            ExecutePackingTest(c_waterBottleJson, TexturePacking::OcclusionRoughnessMetallic);
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackRMO)
        {
            ExecutePackingTest(c_waterBottleJson, TexturePacking::RoughnessMetallicOcclusion);
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackORMandRMO)
        {
            ExecutePackingTest(c_waterBottleJson, (TexturePacking)(TexturePacking::OcclusionRoughnessMetallic | TexturePacking::RoughnessMetallicOcclusion));
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackNRM)
        {
            ExecutePackingTest(c_waterBottleJson, TexturePacking::NormalRoughnessMetallic);
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackNRMandORM)
        {
            // Default for RS4+ compatible with both HoloLens and Desktop
            ExecutePackingTest(c_waterBottleJson, (TexturePacking)(TexturePacking::OcclusionRoughnessMetallic | TexturePacking::NormalRoughnessMetallic));
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackNRMandORMandRMO)
        {
            // Maximum compatibility: all packings
            ExecutePackingTest(c_waterBottleJson, (TexturePacking)(TexturePacking::OcclusionRoughnessMetallic | TexturePacking::NormalRoughnessMetallic | TexturePacking::RoughnessMetallicOcclusion));
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackAllWithNoMaterials)
        {
            // This asset has no materials
            TestUtils::LoadAndExecuteGLTFTest(c_cubeAsset3DJson, [](auto doc, auto path)
            {
                auto packedDoc = GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(std::make_shared<TestStreamReader>(path), doc, TexturePacking::OcclusionRoughnessMetallic, "");

                // Check that nothing changed
                Assert::IsTrue(doc == packedDoc);
            });
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackAllWithPackingNone)
        {
            // This asset has all textures
            TestUtils::LoadAndExecuteGLTFTest(c_waterBottleJson, [](auto doc, auto path)
            {
                auto packedDoc = GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(std::make_shared<TestStreamReader>(path), doc, TexturePacking::None, "");

                // Check that nothing changed
                Assert::IsTrue(doc == packedDoc);
            });
        }

        TEST_METHOD(GLTFTexturePackingUtils_PackAllWithOneMaterial)
        {
            std::unique_ptr<Document> documentPackedSingleTexture;
            std::unique_ptr<Document> documentPackedAllTextures;

            // This asset has all textures
            TestUtils::LoadAndExecuteGLTFTest(c_waterBottleJson, [&documentPackedSingleTexture](auto doc, auto path)
            {
                documentPackedSingleTexture = std::make_unique<Document>(GLTFTexturePackingUtils::PackMaterialForWindowsMR(std::make_shared<TestStreamReader>(path), doc, doc.materials.Elements()[0], TexturePacking::OcclusionRoughnessMetallic, ""));
            });

            TestUtils::LoadAndExecuteGLTFTest(c_waterBottleJson, [&documentPackedAllTextures](auto doc, auto path)
            {
                documentPackedAllTextures = std::make_unique<Document>(GLTFTexturePackingUtils::PackAllMaterialsForWindowsMR(std::make_shared<TestStreamReader>(path), doc, TexturePacking::OcclusionRoughnessMetallic, ""));
            });

            // Assert there's one material
            Assert::IsTrue(documentPackedSingleTexture->materials.Size() == 1);
            Assert::IsTrue(documentPackedAllTextures->materials.Size() == 1);

            // Check that they're the same when there's one material
            Assert::IsTrue(*documentPackedSingleTexture == *documentPackedAllTextures);
        }
    };
}

