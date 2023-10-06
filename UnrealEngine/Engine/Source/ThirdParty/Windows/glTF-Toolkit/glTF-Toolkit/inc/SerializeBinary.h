// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.#pragma once

#include "GLTFSDK.h"

#include <functional>
#include <memory>
#include <vector>

#include "AccessorUtils.h"

namespace Microsoft::glTF::Toolkit
{
    /// <summary>
    /// A function that determines to which type an accessor should be converted,
    /// based on the accessor metadata.
    /// </summary>
    typedef std::function<ComponentType(const Accessor&)> AccessorConversionStrategy;

    /// <summary>
    /// Serializes a glTF asset as a glTF binary (GLB) file.
    /// </summary>
    /// <param name="Document">The glTF asset manifest to be serialized.</param>
    /// <param name="inputStreamReader">A stream reader that is capable of accessing the resources used in the glTF asset by URI.</param>
    /// <param name="outputStreamFactory">A stream factory that is capable of creating an output stream where the GLB will be saved, and a temporary stream for
    /// use during the serialization process.</param>
    void SerializeBinary(const Document& document, std::shared_ptr<const IStreamReader> inputStreamReader, std::shared_ptr<const IStreamWriter> outputStreamWriter, const AccessorConversionStrategy& accessorConversion = nullptr);

    /// <summary>
    /// Serializes a glTF asset as a glTF binary (GLB) file.
    /// </summary>
    /// <param name="Document">The glTF asset manifest to be serialized.</param>
    /// <param name="resourceReader">A resource reader that is capable of accessing the resources used in the document.</param>
    /// <param name="outputStreamFactory">A stream factory that is capable of creating an output stream where the GLB will be saved, and a temporary stream for
    /// use during the serialization process.</param>
    void SerializeBinary(const Document& document, const GLTFResourceReader& resourceReader, std::shared_ptr<const IStreamWriter> outputStreamWriter, const AccessorConversionStrategy& accessorConversion = nullptr);
}
