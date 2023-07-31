//-*****************************************************************************
//
// Copyright (c) 2009-2013,
//  Sony Pictures Imageworks Inc. and
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

#include <Alembic/AbcCoreHDF5/CprData.h>
#include <Alembic/AbcCoreHDF5/ReadUtil.h>
#include <Alembic/AbcCoreHDF5/HDF5Util.h>
#include <Alembic/AbcCoreHDF5/CprImpl.h>
#include <Alembic/AbcCoreHDF5/SprImpl.h>
#include <Alembic/AbcCoreHDF5/AprImpl.h>

namespace Alembic {
namespace AbcCoreHDF5 {
namespace ALEMBIC_VERSION_NS {

//-*****************************************************************************
struct CprAttrVisitor
{
    CprAttrVisitor() {}

    std::vector<std::string> properties;

    void operator() ( const std::string &iName )
    {
        properties.push_back( iName );
    }
};

//-*****************************************************************************
herr_t CprVisitAllAttrsCB( hid_t iGroup,
                           const char *iAttrName,
                           const H5A_info_t *iAinfo,
                           void *iOpData )
{
    // Get the visitor class. This being NULL is a programmer error
    CprAttrVisitor *visitor = ( CprAttrVisitor * )iOpData;
    assert( visitor != NULL );

    if ( iAttrName == NULL || iAttrName[0] == 0 )
    {
        return 0;
    }

    // We only care about the attr name.
    // our names are in the form (for header info)
    // $NAME.info       Property Type, Data Type, and possibly TimeSampling
    //                  index, number of samples, first changed sample,
    //                  last changed sample, and is scalar like hint.
    // $NAME.meta       Meta Data
    // $NAME.smp0       First sample for smaller properties.
    //                  Larger properties store it in an HDF5 dataset.
    // $NAME.dims       Dimension hint for strings and complex multi-dimensional
    //                  data sets.
    // we ignore it everything else
    std::string attrName( iAttrName );
    size_t attrNameLen = attrName.size();
    if ( attrNameLen < 6 )
    {
        return 0;
    }

    // Last 5 characters.
    std::string suffix( attrName, attrNameLen-5 );
    if ( suffix == ".info" && attrNameLen > 0)
    {
        std::string propertyName( attrName, 0, attrNameLen-5 );
        (*visitor)( propertyName );
    }

    return 0;
}

//-*****************************************************************************
CprData::CprData( H5Node & iParentGroup, int32_t iArchiveVersion,
                  const std::string &iName )
  : m_subPropertyMutexes( NULL )
{
    ABCA_ASSERT( iParentGroup.isValidObject(), "invalid parent group" );

    // If our group exists, open it. If it does not, this is not a problem!
    // It just means we don't have any subproperties.
    if ( !GroupExists( iParentGroup, iName ) )
    {
        // No group. It's okay!
        // Just return - it means we have no subprops!
        return;
    }

    m_group = OpenGroup( iParentGroup, iName.c_str() );

    ABCA_ASSERT( m_group.isValidObject(),
                 "Could not open compound property group named: "
                 << iName << ", H5Gopen2 failed" );

    // Do the visiting to gather property names.

    CprAttrVisitor visitor;

    HDF5Hierarchy* h5HPtr = iParentGroup.getH5HPtr();
    if ( h5HPtr )
    {
        h5HPtr->visitAllAttributes( iParentGroup.getRef(), iName, visitor );
    }
    else
    {
        try
        {
            herr_t status = H5Aiterate2( m_group.getObject(),
                                         H5_INDEX_CRT_ORDER,
                                         H5_ITER_INC,
                                         NULL,
                                         CprVisitAllAttrsCB,
                                         ( void * )&visitor );

            ABCA_ASSERT( status >= 0,
                     "CprData::CprData(): H5Aiterate failed" );
        }
        catch ( std::exception & exc )
        {
            ABCA_THROW( "Could not attr iterate property group named: "
                        << iName << ", reason: " << exc.what() );
        }
        catch ( ... )
        {
            ABCA_THROW( "Could not attr iterate property group named: "
                        << iName << ", unknown reason" );
        }
    }


    size_t index = 0;
    m_propertyHeaders.resize( visitor.properties.size() );
    m_subPropertyMutexes =
        new Alembic::Util::mutex[ visitor.properties.size() ];

    // For each property name, read the various pieces of information
    for ( std::vector<std::string>::iterator siter = visitor.properties.begin();
          siter != visitor.properties.end(); ++siter, ++index )
    {
        m_subProperties[(*siter)] = index;
        m_propertyHeaders[index].name = (*siter);
        m_propertyHeaders[index].numSamples = 0;
        m_propertyHeaders[index].firstChangedIndex = 0;
        m_propertyHeaders[index].lastChangedIndex = 0;
        m_propertyHeaders[index].isScalarLike = false;
    }
}

//-*****************************************************************************
CprData::~CprData()
{
    delete[] m_subPropertyMutexes;
    CloseObject( m_group );
}

//-*****************************************************************************
size_t CprData::getNumProperties()
{
    // fixed length and resize called in ctor, so multithread safe.
    return m_propertyHeaders.size();
}

//-*****************************************************************************
const AbcA::PropertyHeader &
CprData::getPropertyHeader( AbcA::CompoundPropertyReaderPtr iParent, size_t i )
{
    // fixed length and resize called in ctor, so multithread safe.
    if ( i > m_propertyHeaders.size() )
    {
        ABCA_THROW( "Out of range index in "
                    << "CprData::getPropertyHeader: " << i );
    }
    Alembic::Util::scoped_lock l( m_subPropertyMutexes[i] );

    // read the property header stuff if we haven't yet
    if ( m_propertyHeaders[i].header == NULL )
    {
        uint32_t tsid = 0;

        PropertyHeaderPtr iPtr( new AbcA::PropertyHeader() );
        ReadPropertyHeader( m_group, m_propertyHeaders[i].name, *iPtr,
                            m_propertyHeaders[i].isScalarLike,
                            m_propertyHeaders[i].numSamples,
                            m_propertyHeaders[i].firstChangedIndex,
                            m_propertyHeaders[i].lastChangedIndex, tsid );

        if ( iPtr->isSimple() )
        {
            AbcA::TimeSamplingPtr tsPtr =
                iParent->getObject()->getArchive()->getTimeSampling( tsid );

            iPtr->setTimeSampling(tsPtr);
        }

        m_propertyHeaders[i].header = iPtr;

        // don't need name anymore (it's in the header)
        m_propertyHeaders[i].name = "";
    }

    return *(m_propertyHeaders[i].header);
}

//-*****************************************************************************
const AbcA::PropertyHeader *
CprData::getPropertyHeader( AbcA::CompoundPropertyReaderPtr iParent,
                            const std::string &iName )
{
    // map of names to indexes filled by ctor (CprAttrVistor),
    // so multithread safe.
    SubPropertiesMap::iterator fiter = m_subProperties.find( iName );
    if ( fiter == m_subProperties.end() )
    {
        return NULL;
    }

    return &(getPropertyHeader(iParent, fiter->second));
}

//-*****************************************************************************
AbcA::ScalarPropertyReaderPtr
CprData::getScalarProperty( AbcA::CompoundPropertyReaderPtr iParent,
                            const std::string &iName )
{
    // map of names to indexes filled by ctor (CprAttrVistor),
    // so multithread safe.
    SubPropertiesMap::iterator fiter = m_subProperties.find( iName );
    if ( fiter == m_subProperties.end() )
    {
        return AbcA::ScalarPropertyReaderPtr();
    }

    // make sure we've read the header
    getPropertyHeader( iParent, fiter->second );
    SubProperty & sub = m_propertyHeaders[fiter->second];

    if ( !(sub.header->isScalar()) )
    {
        ABCA_THROW( "Tried to read a scalar property from a non-scalar: "
                    << iName << ", type: "
                    << sub.header->getPropertyType() );
    }

    Alembic::Util::scoped_lock l( m_subPropertyMutexes[fiter->second] );
    AbcA::BasePropertyReaderPtr bptr = sub.made.lock();
    if ( ! bptr )
    {
        // Make a new one.
        bptr = Alembic::Util::shared_ptr<SprImpl>(
            new SprImpl( iParent, m_group, sub.header, sub.numSamples,
                         sub.firstChangedIndex, sub.lastChangedIndex ) );

        sub.made = bptr;
    }

    AbcA::ScalarPropertyReaderPtr ret =
        Alembic::Util::dynamic_pointer_cast<AbcA::ScalarPropertyReader,
        AbcA::BasePropertyReader>( bptr );
    return ret;
}

//-*****************************************************************************
AbcA::ArrayPropertyReaderPtr
CprData::getArrayProperty( AbcA::CompoundPropertyReaderPtr iParent,
                           const std::string &iName )
{
    // map of names to indexes filled by ctor (CprAttrVistor),
    // so multithread safe.
    SubPropertiesMap::iterator fiter = m_subProperties.find( iName );
    if ( fiter == m_subProperties.end() )
    {
        return AbcA::ArrayPropertyReaderPtr();
    }

    // make sure we've read the header
    getPropertyHeader( iParent, fiter->second );
    SubProperty & sub = m_propertyHeaders[fiter->second];

    if ( !(sub.header->isArray()) )
    {
        ABCA_THROW( "Tried to read an array property from a non-array: "
                    << iName << ", type: "
                    << sub.header->getPropertyType() );
    }

    Alembic::Util::scoped_lock l( m_subPropertyMutexes[fiter->second] );
    AbcA::BasePropertyReaderPtr bptr = sub.made.lock();
    if ( ! bptr )
    {
        // Make a new one.
        bptr = Alembic::Util::shared_ptr<AprImpl>(
            new AprImpl( iParent, m_group, sub.header, sub.isScalarLike,
                         sub.numSamples, sub.firstChangedIndex,
                         sub.lastChangedIndex ) );

        sub.made = bptr;
    }

    AbcA::ArrayPropertyReaderPtr ret =
        Alembic::Util::dynamic_pointer_cast<AbcA::ArrayPropertyReader,
        AbcA::BasePropertyReader>( bptr );
    return ret;
}

//-*****************************************************************************
AbcA::CompoundPropertyReaderPtr
CprData::getCompoundProperty( AbcA::CompoundPropertyReaderPtr iParent,
                              const std::string &iName )
{
    // map of names to indexes filled by ctor (CprAttrVistor),
    // so multithread safe.
    SubPropertiesMap::iterator fiter = m_subProperties.find( iName );
    if ( fiter == m_subProperties.end() )
    {
        return AbcA::CompoundPropertyReaderPtr();
    }

    // make sure we've read the header
    getPropertyHeader( iParent, fiter->second );
    SubProperty & sub = m_propertyHeaders[fiter->second];

    if ( !(sub.header->isCompound()) )
    {
        ABCA_THROW( "Tried to read a compound property from a non-compound: "
                    << iName << ", type: "
                    << sub.header->getPropertyType() );
    }

    Alembic::Util::scoped_lock l( m_subPropertyMutexes[fiter->second] );
    AbcA::BasePropertyReaderPtr bptr = sub.made.lock();
    if ( ! bptr )
    {
        // Make a new one.
        bptr = Alembic::Util::shared_ptr<CprImpl>(
            new CprImpl( iParent, m_group, sub.header ) );

        sub.made = bptr;
    }

    AbcA::CompoundPropertyReaderPtr ret =
        Alembic::Util::dynamic_pointer_cast<AbcA::CompoundPropertyReader,
        AbcA::BasePropertyReader>( bptr );
    return ret;
}

} // End namespace ALEMBIC_VERSION_NS
} // End namespace AbcCoreHDF5
} // End namespace Alembic
