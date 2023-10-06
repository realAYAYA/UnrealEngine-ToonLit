// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef _MSC_VER
__int64
#else
long long
#endif
evaluate_integer_constant_expression(char* p, int* syntax_error);
extern int evaluate_integer_constant_expression_as_condition(char* p, int* syntax_error);
extern void test_integer_constant_expression(void);
