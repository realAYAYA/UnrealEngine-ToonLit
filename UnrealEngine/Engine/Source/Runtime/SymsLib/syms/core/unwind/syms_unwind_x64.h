// Copyright Epic Games, Inc. All Rights Reserved.
/* date = November 4th 2021 1:24 pm */

#ifndef SYMS_UNWIND_X64_H
#define SYMS_UNWIND_X64_H

typedef struct SYMS_UnwindStateX64{
  SYMS_U64 stack_pointer;
  SYMS_U64 instruction_pointer;
  
  SYMS_U64 rax;
  SYMS_U64 rcx;
  SYMS_U64 rdx;
  SYMS_U64 rbx;
  SYMS_U64 rsp;
  SYMS_U64 rbp;
  SYMS_U64 rsi;
  SYMS_U64 rdi;
  SYMS_U64 r8;
  SYMS_U64 r9;
  SYMS_U64 r10;
  SYMS_U64 r11;
  SYMS_U64 r12;
  SYMS_U64 r13;
  SYMS_U64 r14;
  SYMS_U64 r15;
  
  // TODO(allen): xmm
  // TODO(allen): flags
} SYMS_UnwindStateX64;

#endif //SYMS_UNWIND_X64_H
