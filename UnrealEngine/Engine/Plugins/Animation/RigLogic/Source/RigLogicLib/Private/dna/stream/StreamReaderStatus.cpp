// Copyright Epic Games, Inc. All Rights Reserved.

#include "dna/stream/StreamReaderStatus.h"

#include "dna/StreamReader.h"

#include <status/Provider.h>

namespace dna {

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
sc::StatusProvider StreamReaderStatus::status{StreamReader::SignatureMismatchError,
                                              StreamReader::VersionMismatchError,
                                              StreamReader::InvalidDataError};
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

}  // namespace dna
