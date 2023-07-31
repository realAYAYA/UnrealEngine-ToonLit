// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "GLTFTextureUtils.h"
#include "GLTFTexturePackingUtils.h"
#include "GLTFTextureCompressionUtils.h"
#include "DeviceResources.h"

// Usings for ComPtr
using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;

// Usings for glTF
using namespace Microsoft::glTF;
using namespace Microsoft::glTF::Toolkit;

#include <DirectXTex.h>

const char* Microsoft::glTF::Toolkit::EXTENSION_MSFT_TEXTURE_DDS = "MSFT_texture_dds";

Document GLTFTextureCompressionUtils::CompressTextureAsDDS(std::shared_ptr<IStreamReader> streamReader, const Document & doc, const Texture & texture, TextureCompression compression, const std::string& outputDirectory, size_t maxTextureSize, bool generateMipMaps, bool retainOriginalImage, bool treatAsLinear)
{
    Document outputDoc(doc);

    // Early return cases:
    // - No compression requested
    // - This texture doesn't have an image associated
    // - The texture already has a DDS extension
    if (compression == TextureCompression::None ||
        texture.imageId.empty() ||
        texture.extensions.find(EXTENSION_MSFT_TEXTURE_DDS) != texture.extensions.end())
    {
        // Return copy of document
        return outputDoc;
    }

    auto image = std::make_unique<DirectX::ScratchImage>(GLTFTextureUtils::LoadTexture(streamReader, doc, texture.id, treatAsLinear));

    // Resize up to a multiple of 4
    auto metadata = image->GetMetadata();
    auto resizedWidth = metadata.width;
    auto resizedHeight = metadata.height;

    if (maxTextureSize < resizedWidth || maxTextureSize < resizedHeight)
    {
        // Scale
        auto scaleFactor = static_cast<double>(maxTextureSize) / std::max(metadata.width, metadata.height);
        resizedWidth = static_cast<size_t>(std::llround(metadata.width * scaleFactor));
        resizedHeight = static_cast<size_t>(std::llround(metadata.height * scaleFactor));
    }

    if (resizedWidth % 4 != 0 || resizedHeight % 4 != 0)
    {
        static const std::function<size_t(size_t)> roundUpToMultipleOf4 = [](size_t input)
        {
            return input % 4 == 0 ? input : (input + 4) - (input % 4);
        };

        resizedWidth = roundUpToMultipleOf4(resizedWidth);
        resizedHeight = roundUpToMultipleOf4(resizedHeight);
    }

    if (resizedWidth != metadata.width || resizedHeight != metadata.height)
    {
        auto resized = std::make_unique<DirectX::ScratchImage>();
        if (FAILED(DirectX::Resize(image->GetImages(), image->GetImageCount(), image->GetMetadata(), resizedWidth, resizedHeight, DirectX::TEX_FILTER_SEPARATE_ALPHA, *resized)))
        {
            throw GLTFException("Failed to resize image.");
        }

        image = std::move(resized);
    }

    if (generateMipMaps)
    {
        auto mipChain = std::make_unique<DirectX::ScratchImage>();
        if (FAILED(DirectX::GenerateMipMaps(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DirectX::TEX_FILTER_SEPARATE_ALPHA, 0, *mipChain)))
        {
            throw GLTFException("Failed to generate mip maps.");
        }

        image = std::move(mipChain);
    }

    CompressImage(*image, compression);

    // Save image to file
    std::string outputImagePath = "texture_" + texture.id;

    if (!generateMipMaps)
    {
        // The default is to have mips, so note on the texture when it doesn't
        outputImagePath += "_nomips";
    }

    switch (compression)
    {
    case TextureCompression::BC3:
        outputImagePath += "_BC3";
        break;
    case TextureCompression::BC5:
        outputImagePath += "_BC5";
        break;
    case TextureCompression::BC7:
    case TextureCompression::BC7_SRGB:
        outputImagePath += "_BC7";
        break;
    default:
        throw GLTFException("Invalid compression.");
        break;
    }

    outputImagePath += ".dds";
    std::wstring outputImagePathW(outputImagePath.begin(), outputImagePath.end());

    wchar_t outputImageFullPath[MAX_PATH];

    std::wstring outputDirectoryW(outputDirectory.begin(), outputDirectory.end());

    if (FAILED(::PathCchCombine(outputImageFullPath, ARRAYSIZE(outputImageFullPath), outputDirectoryW.c_str(), outputImagePathW.c_str())))
    {
        throw GLTFException("Failed to compose output file path.");
    }

    if (FAILED(SaveToDDSFile(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, outputImageFullPath)))
    {
        throw GLTFException("Failed to save image as DDS.");
    }

    std::wstring outputImageFullPathW(outputImageFullPath);
    std::string outputImageFullPathA(outputImageFullPathW.begin(), outputImageFullPathW.end());

    // Add back to GLTF
    std::string ddsImageId(texture.imageId);

    Image ddsImage(doc.images.Get(texture.imageId));
    ddsImage.mimeType = "image/vnd-ms.dds";
    ddsImage.uri = outputImageFullPathA;

    if (retainOriginalImage)
    {
        ddsImage.id.clear();
        ddsImageId = outputDoc.images.Append(ddsImage, AppendIdPolicy::GenerateOnEmpty).id;
    }
    else
    {
        outputDoc.images.Replace(ddsImage);
    }

    Texture ddsTexture(texture);

    // Create the JSON for the DDS extension element
    rapidjson::Document ddsExtensionJson;
    ddsExtensionJson.SetObject();

    ddsExtensionJson.AddMember("source", rapidjson::Value(outputDoc.images.GetIndex(ddsImageId)), ddsExtensionJson.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    ddsExtensionJson.Accept(writer);

    ddsTexture.extensions.insert(std::pair<std::string, std::string>(EXTENSION_MSFT_TEXTURE_DDS, buffer.GetString()));

    outputDoc.textures.Replace(ddsTexture);

    outputDoc.extensionsUsed.insert(EXTENSION_MSFT_TEXTURE_DDS);

    if (!retainOriginalImage)
    {
        outputDoc.extensionsRequired.insert(EXTENSION_MSFT_TEXTURE_DDS);
    }

    return outputDoc;
}

Document GLTFTextureCompressionUtils::CompressAllTexturesForWindowsMR(std::shared_ptr<IStreamReader> streamReader, const Document & doc, const std::string& outputDirectory, size_t maxTextureSize, bool retainOriginalImages)
{
    Document outputDoc(doc);

    for (auto material : doc.materials.Elements())
    {
        auto compressIfNotEmpty = [&outputDoc, &streamReader, &outputDirectory, maxTextureSize, retainOriginalImages](const std::string& textureId, TextureCompression compression, bool treatAsLinear = true)
        {
            if (!textureId.empty())
            {
                outputDoc = CompressTextureAsDDS(streamReader, outputDoc, outputDoc.textures.Get(textureId), compression, outputDirectory, maxTextureSize, true, retainOriginalImages, treatAsLinear);
            }
        };

        // Compress base and emissive texture as BC7
        compressIfNotEmpty(material.metallicRoughness.baseColorTexture.textureId, TextureCompression::BC7_SRGB, false);
        compressIfNotEmpty(material.emissiveTexture.textureId, TextureCompression::BC7_SRGB, false);

        // Get textures from the MSFT_packing_occlusionRoughnessMetallic extension
        if (material.extensions.find(EXTENSION_MSFT_PACKING_ORM) != material.extensions.end())
        {
            rapidjson::Document packingOrmContents;
            packingOrmContents.Parse(material.extensions[EXTENSION_MSFT_PACKING_ORM].c_str());

            // Compress packed textures as BC7
            if (packingOrmContents.HasMember(MSFT_PACKING_ORM_RMOTEXTURE_KEY))
            {
                auto rmoTextureId = packingOrmContents[MSFT_PACKING_ORM_RMOTEXTURE_KEY][MSFT_PACKING_INDEX_KEY].GetInt();
                compressIfNotEmpty(std::to_string(rmoTextureId), TextureCompression::BC7);
            }

            if (packingOrmContents.HasMember(MSFT_PACKING_ORM_ORMTEXTURE_KEY))
            {
                auto ormTextureId = packingOrmContents[MSFT_PACKING_ORM_ORMTEXTURE_KEY][MSFT_PACKING_INDEX_KEY].GetInt();
                compressIfNotEmpty(std::to_string(ormTextureId), TextureCompression::BC7);
            }

            // Compress normal texture as BC5
            if (packingOrmContents.HasMember(MSFT_PACKING_ORM_NORMALTEXTURE_KEY))
            {
                auto normalTextureId = packingOrmContents[MSFT_PACKING_ORM_NORMALTEXTURE_KEY][MSFT_PACKING_INDEX_KEY].GetInt();
                compressIfNotEmpty(std::to_string(normalTextureId), TextureCompression::BC5);
            }
        }

        // Get textures from the MSFT_packing_normalRoughnessMetallic extension
        if (material.extensions.find(EXTENSION_MSFT_PACKING_NRM) != material.extensions.end())
        {
            rapidjson::Document packingNrmContents;
            packingNrmContents.Parse(material.extensions[EXTENSION_MSFT_PACKING_NRM].c_str());

            // Compress packed texture as BC7
            if (packingNrmContents.HasMember(MSFT_PACKING_NRM_KEY))
            {
                auto nrmTextureId = packingNrmContents[MSFT_PACKING_NRM_KEY][MSFT_PACKING_INDEX_KEY].GetInt();
                compressIfNotEmpty(std::to_string(nrmTextureId), TextureCompression::BC7, false); // This tool generates sRGB-packaged images
            }
        }
    }

    return outputDoc;
}

void GLTFTextureCompressionUtils::CompressImage(DirectX::ScratchImage& image, TextureCompression compression)
{
    if (compression == TextureCompression::None)
    {
        return;
    }

    DXGI_FORMAT compressionFormat = DXGI_FORMAT_BC7_UNORM;
    switch (compression)
    {
    case TextureCompression::BC3:
        compressionFormat = DXGI_FORMAT_BC3_UNORM;
        break;
    case TextureCompression::BC5:
        compressionFormat = DXGI_FORMAT_BC5_UNORM;
        break;
    case TextureCompression::BC7:
        compressionFormat = DXGI_FORMAT_BC7_UNORM;
        break;
    case TextureCompression::BC7_SRGB:
        compressionFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
        break;
    default:
        throw std::invalid_argument("Invalid compression specified.");
        break;
    }

    bool gpuCompressionSuccessful = false;
    DirectX::ScratchImage compressedImage;

    try
    {
        DX::DeviceResources deviceResources;
        deviceResources.CreateDeviceResources();
        ComPtr<ID3D11Device> device(deviceResources.GetD3DDevice());

        if (device != nullptr)
        {
            if (SUCCEEDED(DirectX::Compress(device.Get(), image.GetImages(), image.GetImageCount(), image.GetMetadata(), compressionFormat, DirectX::TEX_COMPRESS_DEFAULT, 1, compressedImage)))
            {
                gpuCompressionSuccessful = true;
            }
        }
    }
    catch (std::exception e)
    {
        // Failed to initialize device - GPU is not available
    }

    if (!gpuCompressionSuccessful)
    {
        // Try software compression
        if (FAILED(DirectX::Compress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), compressionFormat, DirectX::TEX_COMPRESS_PARALLEL, 1, compressedImage)))
        {
            throw GLTFException("Failed to compress data using software compression");
        }
    }

    image = std::move(compressedImage);
}