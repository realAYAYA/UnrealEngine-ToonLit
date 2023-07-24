//-*****************************************************************************
//
// Copyright (c) 2013,
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
// Industrial Light & Magic, nor the names of their contributors may be used
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

#include <Alembic/Ogawa/All.h>
#include <Alembic/AbcCoreAbstract/Tests/Assert.h>

void test(bool iUseMMap)
{
    {
        Alembic::Ogawa::OArchive oa("archiveTest.ogawa");
        TESTING_ASSERT(oa.isValid());
        Alembic::Ogawa::IArchive ia("archiveTest.ogawa", 1, iUseMMap);
        TESTING_ASSERT(ia.isValid());
        TESTING_ASSERT(!ia.isFrozen());
        TESTING_ASSERT(ia.getVersion() == 1);
        TESTING_ASSERT(ia.getGroup()->getNumChildren() == 0);
    }

    Alembic::Ogawa::IArchive ia("archiveTest.ogawa");
    TESTING_ASSERT(ia.isValid());
    TESTING_ASSERT(ia.isFrozen());
    TESTING_ASSERT(ia.getVersion() == 1);
    TESTING_ASSERT(ia.getGroup()->getNumChildren() == 0);

}

void stringStreamTest()
{

    std::stringstream strm;
    strm << "potato!";
    {
        Alembic::Ogawa::OArchive oa(&strm);
        TESTING_ASSERT(oa.isValid());
    }

    strm.seekg(7);
    std::vector< std::istream * > streams;
    streams.push_back(&strm);
    Alembic::Ogawa::IArchive ia(streams);
    TESTING_ASSERT(ia.isValid());
    TESTING_ASSERT(ia.isFrozen());
    TESTING_ASSERT(ia.getVersion() == 1);
    TESTING_ASSERT(ia.getGroup()->getNumChildren() == 0);
}


int main ( int argc, char *argv[] )
{
    test(true);     // Use mmap
    test(false);    // Use streams

    stringStreamTest();
    return 0;
}