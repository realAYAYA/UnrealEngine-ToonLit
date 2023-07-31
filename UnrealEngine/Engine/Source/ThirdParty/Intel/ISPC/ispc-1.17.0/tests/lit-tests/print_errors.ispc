// This test checks that compiler does not crash and produces correct error output
// for different print arguments for CPU.

// RUN: not %{ispc} %s -o %t.o -DPRINT_1 --nowrap --target=host 2>&1 | FileCheck %s -check-prefix=CHECK_ERROR_1

// RUN: %{ispc} %s -o %t.o -DPRINT_2 --nowrap --target=host

// RUN: not %{ispc} %s -o %t.o -DPRINT_3 --nowrap --target=host 2>&1 | FileCheck %s -check-prefix=CHECK_ERROR_3

// RUN: not %{ispc} %s -o %t.o -DPRINT_4 --nowrap --target=host 2>&1 | FileCheck %s -check-prefix=CHECK_ERROR_4

// RUN: not %{ispc} %s -o %t.o -DPRINT_5 --nowrap --target=host 2>&1 | FileCheck %s -check-prefix=CHECK_ERROR_5

// RUN: not %{ispc} %s -o %t.o -DPRINT_6 --nowrap --target=host 2>&1 | FileCheck %s -check-prefix=CHECK_ERROR_6

// CHECK_ERROR_1: Error: syntax error, unexpected ')', expecting TOKEN_STRING_LITERAL
// CHECK_ERROR_1-NOT: FATAL ERROR

// CHECK_ERROR_3: Error: Not enough arguments are provided in print call
// CHECK_ERROR_3-NOT: FATAL ERROR

// CHECK_ERROR_4: Error: Too much arguments are provided in print call
// CHECK_ERROR_4-NOT: FATAL ERROR

// CHECK_ERROR_5: Error: Not enough arguments are provided in print call
// CHECK_ERROR_5-NOT: FATAL ERROR

// CHECK_ERROR_6: Error: Too much arguments are provided in print call
// CHECK_ERROR_6-NOT: FATAL ERROR

#ifdef PRINT_1
task void test() {
    print();
}
#endif
#ifdef PRINT_2
task void test() {
    print("");
}
#endif
#ifdef PRINT_3
task void test() {
    print("%");
}
#endif
#ifdef PRINT_4
task void test() {
    print("", 5);
}
#endif
#ifdef PRINT_5
task void test() {
    print("% %", 5);
}
#endif
#ifdef PRINT_6
task void test() {
    print("%", 5, 5);
}
#endif