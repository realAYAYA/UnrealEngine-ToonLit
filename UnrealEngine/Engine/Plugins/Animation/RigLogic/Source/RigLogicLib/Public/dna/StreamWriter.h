// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/Defs.h"
#include "dna/Writer.h"
#include "dna/types/Aliases.h"

namespace dna {

class DNAAPI StreamWriter : public Writer {
    public:
        /**
            @brief Factory method for creation of Writer
            @param stream
                Stream into which the data is going to be written.
            @param memRes
                Memory resource to be used for allocations.
            @note
                If a memory resource is not given, a default allocation mechanism will be used.
            @warning
                User is responsible for releasing the returned pointer by calling destroy.
            @see destroy
        */
        static StreamWriter* create(BoundedIOStream* stream, MemoryResource* memRes = nullptr);
        /**
            @brief Method for freeing a Writer instance.
            @param instance
                Instance of Writer to be freed.
            @see create
        */
        static void destroy(StreamWriter* instance);

        ~StreamWriter() override;
        /**
            @brief Write data to stream from internal structures.
         */
        virtual void write() = 0;
};

}  // namespace dna

namespace pma {

template<>
struct DefaultInstanceCreator<dna::StreamWriter> {
    using type = pma::FactoryCreate<dna::StreamWriter>;
};

template<>
struct DefaultInstanceDestroyer<dna::StreamWriter> {
    using type = pma::FactoryDestroy<dna::StreamWriter>;
};

}  // namespace pma
