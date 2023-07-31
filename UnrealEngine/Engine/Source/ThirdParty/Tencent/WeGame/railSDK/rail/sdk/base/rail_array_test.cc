// Copyright (c) 2016, Entropy Game Global Limited.
// All rights reserved.

#include <stdint.h>
#include "rail/sdk/base/rail_array.h"
#include "thirdparty/gtest/gtest.h"

struct Item {
    int32_t id;
    int32_t value;
};

TEST(RailArray, size) {
    rail::RailArray<struct Item> array;
    EXPECT_EQ(0U, array.size());
    array.resize(100);
    EXPECT_EQ(100U, array.size());
    array.resize(0);
    EXPECT_EQ(0U, array.size());

    struct Item item;
    item.id = 1;
    item.value = 100;
    array.push_back(item);
    EXPECT_EQ(1U, array.size());
    EXPECT_EQ(100, array[0].value);
    array[0].value = 101;
    EXPECT_EQ(101, array[0].value);
    const struct Item& it = array[0];
    EXPECT_EQ(101, it.value);

    struct Item item2;
    item2.id = 2;
    item2.value = 200;
    array.push_back(item2);
    EXPECT_EQ(2U, array.size());

    rail::RailArray<char> str("Hello", 5);
    EXPECT_EQ(5U, str.size());
}

TEST(RailArray, assign) {
    rail::RailArray<struct Item> array;
    Item items[5];
    items[1].value = 100;
    array.assign(items, sizeof(items) / sizeof(Item));
    EXPECT_EQ(5U, array.size());
    EXPECT_EQ(100, array[1].value);
}

TEST(RailArray, buf) {
    rail::RailArray<int64_t> array;
    EXPECT_EQ(NULL, array.buf());
}
