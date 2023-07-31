// Copyright Epic Games, Inc. All Rights Reserved.

#include "tersetests/Defs.h"
#include "tersetests/archives/binary/Common.h"
#include "tersetests/archives/binary/FakeStream.h"

#include "terse/archives/binary/OutputArchive.h"

#include <pma/TypeDefs.h>
#include <pma/resources/AlignedMemoryResource.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstdint>
#include <string>
#include <vector>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

TEST(BinaryOutputArchiveTest, StreamInsertionOperator) {
    std::int32_t source1 = 1234;
    std::int32_t source2 = 5678;

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive << source1 << source2;

    unsigned char expected[] = {
        0x00, 0x00, 0x04, 0xd2,  // 1234
        0x00, 0x00, 0x16, 0x2e  // 5678
    };
    unsigned char bytes[8ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), sizeof(std::int32_t) * 2ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 8ul);
}

TEST(BinaryOutputArchiveTest, ArchiveOffset) {
    OffsetUtilizer source{};
    source.a = 1234;
    source.b = 'x';
    source.c = "abcd";
    source.firstInt = 256;
    source.secondInt = 1024;

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive << source;

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x19,  // layerStart offset
        0x00, 0x00, 0x00, 0x19,  // first offset
        0x00, 0x00, 0x00, 0x1d,  // second offset
        0x00, 0x00, 0x04, 0xd2,  // a = 1234
        0x78,  // b = 'x'
        0x00, 0x00, 0x00, 0x04,  // c size = 4
        0x61, 0x62, 0x63, 0x64,  // c = 'abcd'
        0x00, 0x00, 0x01, 0x00,  // firstInt = 256
        0x00, 0x00, 0x04, 0x00  // secondInt = 1024

    };
    unsigned char bytes[33ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    ASSERT_ELEMENTS_EQ(bytes, expected, sizeof(bytes));
}

TEST(BinaryOutputArchiveTest, PrimitiveTypeSerialization) {
    std::int32_t source = 1234;

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {0x00, 0x00, 0x04, 0xd2};  // 1234
    unsigned char bytes[4ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), sizeof(std::int32_t));

    ASSERT_ELEMENTS_EQ(bytes, expected, 4ul);
}

TEST(BinaryOutputArchiveTest, ComplexTypeSerialization) {
    ComplexType source{127, 255, 32767, 65535, 2147483647, 4294967295};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x7f,  // 127
        0xff,  // 255
        0x7f, 0xff,  // 32767
        0xff, 0xff,  // 65535
        0x7f, 0xff, 0xff, 0xff,  // 2147483647
        0xff, 0xff, 0xff, 0xff,  // 4294967295
    };
    unsigned char bytes[14ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 14ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 14ul);
}

TEST(BinaryOutputArchiveTest, FreeSerialize) {
    SerializableByFreeSerialize source{4294967295u, 65535u};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0xff, 0xff, 0xff, 0xff,  // 4294967295
        0xff, 0xff  // 65535
    };
    unsigned char bytes[sizeof(expected)];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), sizeof(bytes));

    ASSERT_ELEMENTS_EQ(bytes, expected, sizeof(expected));
}

TEST(BinaryOutputArchiveTest, FreeSave) {
    SerializableByFreeLoadSave source{4294967295u, 65535u};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0xff, 0xff, 0xff, 0xff,  // 4294967295
        0xff, 0xff  // 65535
    };
    unsigned char bytes[sizeof(expected)];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), sizeof(bytes));

    ASSERT_ELEMENTS_EQ(bytes, expected, sizeof(expected));
}

TEST(BinaryOutputArchiveTest, StringSerialization) {
    std::string source{"abcdefgh"};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x08,  // 8
        0x61,  // 'a'
        0x62,  // 'b'
        0x63,  // 'c'
        0x64,  // 'd'
        0x65,  // 'e'
        0x66,  // 'f'
        0x67,  // 'g'
        0x68  // 'h'
    };
    unsigned char bytes[12ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 12ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 12ul);
}

TEST(BinaryOutputArchiveTest, VectorOfIntegerTypeSerialization) {
    std::vector<std::int32_t> source{16, 256, 4096, 65536};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x04,  // 4
        0x00, 0x00, 0x00, 0x10,  // 16
        0x00, 0x00, 0x01, 0x00,  // 256
        0x00, 0x00, 0x10, 0x00,  // 4096
        0x00, 0x01, 0x00, 0x00  // 65536
    };
    unsigned char bytes[20ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 20ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 20ul);
}

TEST(BinaryOutputArchiveTest, VectorOfFloatTypeSerialization) {
    std::vector<float> source{1.2f, 3.14f, 0.05f, 0.74f};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x04,  // 4
        0x3f, 0x99, 0x99, 0x9a,  // 1.2f
        0x40, 0x48, 0xf5, 0xc3,  // 3.14f
        0x3d, 0x4c, 0xcc, 0xcd,  // 0.05f
        0x3f, 0x3d, 0x70, 0xa4  // 0.74f
    };
    unsigned char bytes[20ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 20ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 20ul);
}

TEST(BinaryOutputArchiveTest, VectorOfComplexTypeSerialization) {
    std::vector<ComplexType> source;
    source.push_back({127, 255, 32767, 65535, 2147483647, 4294967295});
    source.push_back({127, 255, 32767, 65535, 2147483647, 4294967295});

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x02,  // 2
        0x7f,  // 127
        0xff,  // 255
        0x7f, 0xff,  // 32767
        0xff, 0xff,  // 65535
        0x7f, 0xff, 0xff, 0xff,  // 2147483647
        0xff, 0xff, 0xff, 0xff,  // 4294967295
        0x7f,  // 127
        0xff,  // 255
        0x7f, 0xff,  // 32767
        0xff, 0xff,  // 65535
        0x7f, 0xff, 0xff, 0xff,  // 2147483647
        0xff, 0xff, 0xff, 0xff  // 4294967295
    };
    unsigned char bytes[32ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 32ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 32ul);
}

TEST(BinaryOutputArchiveTest, VectorOfStringSerialization) {
    std::vector<std::string> source{"abcdefgh", "abcdefgh"};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x02,  // 2
        0x00, 0x00, 0x00, 0x08,  // 8
        0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,  // "abcdefgh"
        0x00, 0x00, 0x00, 0x08,  // 8
        0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68  // "abcdefgh"
    };
    unsigned char bytes[28ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 28ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 28ul);
}

TEST(BinaryOutputArchiveTest, VectorOfPairOfStringsSerialization) {
    using KeyValue = std::pair<std::string, std::string>;
    std::vector<KeyValue> source{{"abcd", "efgh"}, {"abcd", "efgh"}};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x02,  // 2
        0x00, 0x00, 0x00, 0x04,  // 4
        0x61, 0x62, 0x63, 0x64,  // "abcd"
        0x00, 0x00, 0x00, 0x04,  // 4
        0x65, 0x66, 0x67, 0x68,  // "efgh"
        0x00, 0x00, 0x00, 0x04,  // 4
        0x61, 0x62, 0x63, 0x64,  // "abcd"
        0x00, 0x00, 0x00, 0x04,  // 4
        0x65, 0x66, 0x67, 0x68,  // "efgh"
    };
    unsigned char bytes[36ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 36ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 36ul);
}

TEST(BinaryOutputArchiveTest, VectorOfVectorOfPrimitiveTypeSerialization) {
    pma::Vector<std::uint16_t> values{4, 16, 256, 4096};
    std::vector<pma::Vector<std::uint16_t> > source{values, values};

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x02,  // 2
        0x00, 0x00, 0x00, 0x04,  // 4
        0x00, 0x04, 0x00, 0x10, 0x01, 0x00, 0x10, 0x00,  // 4, 16, 256, 4096
        0x00, 0x00, 0x00, 0x04,  // 4
        0x00, 0x04, 0x00, 0x10, 0x01, 0x00, 0x10, 0x00  // 4, 16, 256, 4096
    };
    unsigned char bytes[28ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 28ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 28ul);
}

TEST(BinaryOutputArchiveTest, MixedNestedTypes) {
    pma::AlignedMemoryResource memRes;

    Root root{&memRes};
    root.name = "root";

    Child child1{&memRes};
    child1.integers.push_back(1);
    child1.integers.push_back(2);
    child1.integers.push_back(3);
    child1.floats.push_back(0.01f);
    child1.floats.push_back(0.99f);
    child1.floats.push_back(3.14f);
    Child child2{&memRes};
    child2.integers.push_back(1);
    child2.integers.push_back(2);
    child2.integers.push_back(3);
    child2.floats.push_back(0.01f);
    child2.floats.push_back(0.99f);
    child2.floats.push_back(3.14f);

    root.children.push_back(std::move(child1));
    root.children.push_back(std::move(child2));

    pma::Vector<Root> source{&memRes};
    source.push_back(std::move(root));

    tersetests::FakeStream stream;
    terse::BinaryOutputArchive<tersetests::FakeStream> archive(&stream);
    archive(source);

    unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x01,  // 1
        0x00, 0x00, 0x00, 0x04,  // 4
        0x72, 0x6f, 0x6f, 0x74,  // "root"
        0x00, 0x00, 0x00, 0x02,  // 2
        0x00, 0x00, 0x00, 0x03,  // 3
        0x00, 0x01, 0x00, 0x02, 0x00, 0x03,  // 1, 2, 3
        0x00, 0x00, 0x00, 0x03,  // 3
        0x3c, 0x23, 0xd7, 0x0a, 0x3f, 0x7d, 0x70, 0xa4, 0x40, 0x48, 0xf5, 0xc3,  // 0.01f, 0.99f, 3.14f
        0x00, 0x00, 0x00, 0x03,  // 3
        0x00, 0x01, 0x00, 0x02, 0x00, 0x03,  // 1, 2, 3
        0x00, 0x00, 0x00, 0x03,  // 3
        0x3c, 0x23, 0xd7, 0x0a, 0x3f, 0x7d, 0x70, 0xa4, 0x40, 0x48, 0xf5, 0xc3  // 0.01f, 0.99f, 3.14f
    };
    unsigned char bytes[68ul];
    stream.seek(0ul);
    stream.read(reinterpret_cast<char*>(bytes), 68ul);

    ASSERT_ELEMENTS_EQ(bytes, expected, 68ul);
}
