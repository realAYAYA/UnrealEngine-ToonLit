// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/DataLayer.h"
#include "dna/Defs.h"
#include "dna/Reader.h"
#include "dna/types/Aliases.h"

namespace dna {

class DNAAPI StreamReader : public Reader {
    public:
        static const sc::StatusCode SignatureMismatchError;
        static const sc::StatusCode VersionMismatchError;
        static const sc::StatusCode InvalidDataError;

    public:
        /**
            @brief Factory method for creation of Reader
            @param stream
                Source stream from which data is going to be read.
            @param layer
                Specify the layer up to which the data needs to be loaded.
            @note
                The Definition data layer depends on and thus implicitly loads the Descriptor layer.
                The Behavior data layer depends on and thus implicitly loads the Definition layer.
                The Geometry data layer depends on and thus also implicitly loads the Definition layer.
            @param maxLOD
                The maximum level of details to be loaded.
            @note
                A value of zero indicates to load all LODs.
            @warning
                The maxLOD value must be less than the value returned by getLODCount.
            @see getLODCount
            @param memRes
                Memory resource to be used for allocations.
            @note
                If a memory resource is not given, a default allocation mechanism will be used.
            @warning
                User is responsible for releasing the returned pointer by calling destroy.
            @see destroy
        */
        static StreamReader* create(BoundedIOStream* stream,
                                    DataLayer layer = DataLayer::All,
                                    std::uint16_t maxLOD = 0u,
                                    MemoryResource* memRes = nullptr);
        /**
            @brief Factory method for creation of Reader
            @param stream
                Source stream from which data is going to be read.
            @param layer
                Specify the layer up to which the data needs to be loaded.
            @note
                The Definition data layer depends on and thus implicitly loads the Descriptor layer.
                The Behavior data layer depends on and thus implicitly loads the Definition layer.
                The Geometry data layer depends on and thus also implicitly loads the Definition layer.
            @param maxLOD
                The maximum level of details to be loaded.
            @param minLOD
                The minimum level of details to be loaded.
            @note
                A range of [0, LOD count - 1] for maxLOD / minLOD respectively indicates to load all LODs.
            @warning
                Both maxLOD and minLOD values must be less than the value returned by getLODCount.
            @see getLODCount
            @param memRes
                Memory resource to be used for allocations.
            @note
                If a memory resource is not given, a default allocation mechanism will be used.
            @warning
                User is responsible for releasing the returned pointer by calling destroy.
            @see destroy
        */
        static StreamReader* create(BoundedIOStream* stream,
                                    DataLayer layer,
                                    std::uint16_t maxLOD,
                                    std::uint16_t minLOD,
                                    MemoryResource* memRes = nullptr);
        /**
            @brief Factory method for creation of Reader
            @param stream
                Source stream from which data is going to be read.
            @param layer
                Specify the layer up to which the data needs to be loaded.
            @note
                The Definition data layer depends on and thus implicitly loads the Descriptor layer.
                The Behavior data layer depends on and thus implicitly loads the Definition layer.
                The Geometry data layer depends on and thus also implicitly loads the Definition layer.
            @param lods
                An array specifying which exact lods to load.
            @warning
                All values in the array must be less than the value returned by getLODCount.
            @see getLODCount
            @param lodCount
                The number of elements in the lods array.
            @warning
                There cannot be more elements in the array than the value returned by getLODCount.
            @see getLODCount
            @param memRes
                Memory resource to be used for allocations.
            @note
                If a memory resource is not given, a default allocation mechanism will be used.
            @warning
                User is responsible for releasing the returned pointer by calling destroy.
            @see destroy
        */
        static StreamReader* create(BoundedIOStream* stream,
                                    DataLayer layer,
                                    std::uint16_t* lods,
                                    std::uint16_t lodCount,
                                    MemoryResource* memRes = nullptr);
        /**
            @brief Method for freeing a Reader instance.
            @param instance
                Instance of Reader to be freed.
            @see create
        */
        static void destroy(StreamReader* instance);

        ~StreamReader() override;
        /**
           @brief read data from stream into internal structures.
        */
        virtual void read() = 0;

};

}  // namespace dna

namespace pma {

template<>
struct DefaultInstanceCreator<dna::StreamReader> {
    using type = pma::FactoryCreate<dna::StreamReader>;
};

template<>
struct DefaultInstanceDestroyer<dna::StreamReader> {
    using type = pma::FactoryDestroy<dna::StreamReader>;
};

}  // namespace pma
