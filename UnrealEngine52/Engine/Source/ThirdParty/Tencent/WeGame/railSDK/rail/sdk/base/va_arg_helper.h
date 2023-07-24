// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_BASE_VAARG_HELPER_H
#define RAIL_BASE_VAARG_HELPER_H

#if _MSC_VER == 1400
#define RAIL_EXPAND(args) args
#define RAIL_COMBINE(a, b) RAIL_COMBINE_(a, b)
#define RAIL_COMBINE_(a, b) a##b
#define RAIL_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...)  N
#define RAIL_ARG_N_(...) RAIL_EXPAND(RAIL_ARG_N(__VA_ARGS__))
#define RAIL_IS_COMMA(...) RAIL_ARG_N_(__VA_ARGS__, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,\
    COMMA)
#define RAIL_ARG_COMMA ,
#define RAIL_ARG_NONE
#define RAIL_VA_ARGS(...) RAIL_COMBINE(RAIL_ARG_, RAIL_IS_COMMA(__VA_ARGS__)) __VA_ARGS__
#else
#define RAIL_VA_ARGS(...) __VA_ARGS__
#endif

#endif  // RAIL_BASE_VAARG_HELPER_H
