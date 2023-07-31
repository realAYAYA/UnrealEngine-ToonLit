// Copyright (c) 2016, Entropy Game Global Limited.
// All rights reserved.

#include <stdint.h>
#include <string.h>

#include "thirdparty/gtest/gtest.h"

// rail_string.h中有使用min函数，而在windows上面，min是一个宏，为了在linux上面编译ut，定义该宏
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#include "rail/sdk/base/rail_string.h"

TEST(RailString, size) {
    const rail::RailString s("Hello");
    EXPECT_EQ(5U, s.size());
    EXPECT_EQ('e', s[1]);
    EXPECT_STREQ("Hello", s.c_str());

    rail::RailString s1 = "Hi";
    EXPECT_STREQ("Hi", s1.c_str());
    EXPECT_EQ(2U, s1.size());
    EXPECT_EQ('i', s1[1]);

    rail::RailString s2;
    EXPECT_EQ(0U, s2.size());
    s2 = "Hello";
    EXPECT_STREQ("Hello", s2.c_str());
    EXPECT_EQ(5U, s2.size());
}

TEST(RailString, content) {
    const char buff[] = "\0\1\2\3\4\5";
    rail::RailString s;
    size_t elements = sizeof(buff) / sizeof(char) - 1;  // NOLINT, not include last '\0'
    s.assign(buff, elements);
    EXPECT_EQ(elements, s.size());
    EXPECT_EQ(0, memcmp(buff, s.c_str(), s.size()));
}

TEST(RailString, assignment) {
    struct Data {
        int a;
        char b;
        rail::RailString s;
        Data() {
            a = 100;
            b = 'a';
            s = "Hi";
        }
    };

    Data data1;
    data1.a = 101;
    data1.s = "ok!";
    Data data2;
    data2.s = "hello";
    EXPECT_STREQ("hello", data2.s.data());

    data2 = data1;
    EXPECT_STREQ("ok!", data2.s.c_str());

    // test assign
    data1.s.assign("1\0\045", 5);
    data2 = data1;
    EXPECT_EQ(5U, data1.s.size());
    EXPECT_EQ(data2.s.size(), data1.s.size());
    EXPECT_EQ(0, memcmp(data2.s.c_str(), data1.s.c_str(), data1.s.size()));
    EXPECT_EQ(0, memcmp(data2.s.data(), data1.s.data(), data1.s.size()));
}

TEST(RailString, operator_overload) {
    rail::RailString s1("hello");
    rail::RailString s2("hello");

    EXPECT_EQ(s1, s2);

    rail::RailString s3("hello2");
    EXPECT_TRUE(s1 < s3);
    EXPECT_TRUE(s3 > s1);
    EXPECT_TRUE(s1 == s2);
    EXPECT_TRUE(s1 != s3);
}
