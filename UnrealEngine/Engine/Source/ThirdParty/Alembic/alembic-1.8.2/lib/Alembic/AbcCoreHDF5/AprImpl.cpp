//-*****************************************************************************
//
// Copyright (c) 2009-2012,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include <Alembic/AbcCoreHDF5/AprImpl.h>

namespace Alembic {
namespace AbcCoreHDF5 {
namespace ALEMBIC_VERSION_NS {

//-*****************************************************************************
AprImpl::AprImpl( AbcA::CompoundPropertyReaderPtr iParent,
                  H5Node & iParentGroup,
                  PropertyHeaderPtr iHeader,
                  bool iIsScalarLike,
                  uint32_t iNumSamples,
                  uint32_t iFirstChangedIndex,
                  uint32_t iLastChangedIndex )
  : SimplePrImpl<AbcA::ArrayPropertyReader, AprImpl, AbcA::ArraySamplePtr&>
    ( iParent, iParentGroup, iHeader, iNumSamples, iFirstChangedIndex,
      iLastChangedIndex )
{
    if ( m_header->getPropertyType() != AbcA::kArrayProperty )
    {
        ABCA_THROW( "Attempted to create a ArrayPropertyReader from a "
                    "non-array property type" );
    }

    m_isScalarLike = iIsScalarLike;
}

//-*****************************************************************************
AbcA::ArrayPropertyReaderPtr AprImpl::asArrayPtr()
{
    return shared_from_this();
}

//-*****************************************************************************
bool AprImpl::isScalarLike()
{
    return m_isScalarLike;
}

//-*****************************************************************************
void AprImpl::getDimensions( index_t iSampleIndex, Dimensions & oDim )
{
    iSampleIndex = verifySampleIndex( iSampleIndex );

    std::string sampleName = getSampleName( m_header->getName(), iSampleIndex );
    H5Node parent;

    if ( iSampleIndex == 0 )
    {
        parent = m_parentGroup;
    }
    else
    {
        checkSamplesIGroup();
        parent = m_samplesIGroup;
    }

    std::string dimName = sampleName + ".dims";
    if ( AttrExists( parent, dimName.c_str() ) )
    {
        ReadDimensions( parent.getObject(), dimName, oDim );
    }
    else
    {
        ReadDataSetDimensions( parent.getObject(), sampleName,
                               m_header->getDataType().getExtent(), oDim );
    }
}

//-*****************************************************************************
void AprImpl::getAs( index_t iSampleIndex, void *iIntoLocation,
                     PlainOldDataType iPod )
{
    PlainOldDataType curPod = m_header->getDataType().getPod();

    ABCA_ASSERT( ( iPod != kStringPOD && iPod != kWstringPOD && 
        iPod != kFloat16POD && curPod != kStringPOD && curPod != kWstringPOD &&
        curPod != kFloat16POD) || ( iPod == curPod ),
        "Cannot convert the data to or from a string, wstring or float16_t." );

    hid_t nativeType = -1;
    bool clean = false;

    if ( iPod != kStringPOD && iPod != kWstringPOD )
    {
        AbcA::DataType dtype( iPod );
        nativeType = GetNativeH5T(dtype, clean);
    }

    iSampleIndex = verifySampleIndex( iSampleIndex );

    std::string sampleName = getSampleName( m_header->getName(), iSampleIndex );
    H5Node parent;

    if ( iSampleIndex == 0 )
    {
        parent = m_parentGroup;
    }
    else
    {
        checkSamplesIGroup();
        parent = m_samplesIGroup;
    }

    ReadArray( iIntoLocation, parent.getObject(),sampleName,
               m_header->getDataType(), nativeType );

    if ( clean )
    {
        H5Tclose( nativeType );
    }
}

//-*****************************************************************************
void AprImpl::readSample( hid_t iGroup,
                          const std::string &iSampleName,
                          index_t iSampleIndex,
                          AbcA::ArraySamplePtr& oSamplePtr )
{
    assert( iGroup >= 0 );

    // Check index integrity.
    assert( iSampleIndex >= 0 && iSampleIndex <= m_lastChangedIndex );

    // Read the array sample, possibly from the cache.
    const AbcA::DataType &dataType = m_header->getDataType();
    AbcA::ReadArraySampleCachePtr cachePtr =
        this->getObject()->getArchive()->getReadArraySampleCachePtr();
    oSamplePtr = ReadArray( cachePtr, iGroup, iSampleName, dataType,
                            m_fileDataType,
                            m_nativeDataType );
}

//-*****************************************************************************
bool AprImpl::readKey( hid_t iGroup,
                       const std::string &iSampleName,
                       AbcA::ArraySampleKey& oKey )
{
    assert( iGroup >= 0 );

    // Open the data set.
    hid_t dsetId = H5Dopen( iGroup, iSampleName.c_str(), H5P_DEFAULT );
    ABCA_ASSERT( dsetId >= 0, "Cannot open dataset: " << iSampleName );
    DsetCloser dsetCloser( dsetId );

    const AbcA::DataType &dataType = m_header->getDataType();
    if (ReadKey( dsetId, "key", oKey ))
    {
        hid_t dspaceId = H5Dget_space( dsetId );
        ABCA_ASSERT( dspaceId >= 0, "Could not get dataspace for dataSet: "
                     << iSampleName );
        DspaceCloser dspaceCloser( dspaceId );

        oKey.readPOD = dataType.getPod();
        oKey.origPOD = oKey.readPOD;

        oKey.numBytes = H5Sget_simple_extent_npoints( dspaceId );
        if (oKey.origPOD == kStringPOD || oKey.origPOD == kWstringPOD)
        {

            hid_t dsetFtype = H5Dget_type( dsetId );
            DtypeCloser dtypeCloser( dsetFtype );

            // string arrays get packed together
            oKey.numBytes *= H5Tget_size( dsetFtype );
        }
        else
        {
            oKey.numBytes *= PODNumBytes(dataType.getPod());
        }

        return true;
    }

    return false;
}

} // End namespace ALEMBIC_VERSION_NS
} // End namespace AbcCoreHDF5
} // End namespace Alembic
