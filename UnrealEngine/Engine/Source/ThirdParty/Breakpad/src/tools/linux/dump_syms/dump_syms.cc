// Copyright (c) 2011, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

#include <paths.h>
#include <stdio.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <jemalloc.h>

#include "common/linux/dump_symbols.h"

using google_breakpad::WriteSymbolFile;
using google_breakpad::WriteSymbolFileHeader;

/* EG BEGIN */
#ifdef DUMP_SYMS_WITH_EPIC_EXTENSIONS

// Only supports Linux atm
#if defined(__linux__)
// We are using je_malloc so we'll override these functions to our own definitions
// https://www.gnu.org/software/libc/manual/html_node/Replacing-malloc.html
void* malloc(size_t size)
{
  return je_malloc(size);
}

void free(void* ptr)
{
  je_free(ptr);
}

void* calloc(size_t nmemb, size_t size)
{
  return je_calloc(nmemb, size);
}

void* realloc(void* ptr, size_t size)
{
  return je_realloc(ptr, size);
}

void* aligned_alloc(size_t alignment, size_t size)
{
  return je_aligned_alloc(alignment, size);
}

size_t malloc_usable_size(void* ptr)
{
  return je_malloc_usable_size(ptr);
}

void* memalign(size_t alignment, size_t size)
{
  return je_memalign(alignment, size);
}

int posix_memalign(void** memptr, size_t alignment, size_t size)
{
  return je_posix_memalign(memptr, alignment, size);
}

void* valloc(size_t size)
{
  return je_valloc(size);
}
#endif /* defined (__linux__) */
#endif /* DUMP_SYMS_WITH_EPIC_EXTENSIONS */
/* EG END */

int usage(const char* self) {
  fprintf(stderr, "Usage: %s [OPTION] <binary-with-debugging-info> "
          "[directories-for-debug-file]\n\n", self);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -i:   Output module header information only.\n");
  fprintf(stderr, "  -c    Do not generate CFI section\n");
  fprintf(stderr, "  -r    Do not handle inter-compilation unit references\n");
  fprintf(stderr, "  -m    Keep the C++ Mangled names\n");
  fprintf(stderr, "  -o    File output path\n");
  fprintf(stderr, "  -v    Print all warnings to stderr\n");
  return 1;
}

int main(int argc, char **argv) {
  if (argc < 2)
    return usage(argv[0]);
  bool header_only = false;
  bool cfi = true;
  bool handle_inter_cu_refs = true;
  bool log_to_stderr = false;
  int arg_index = 1;
  std::streambuf* os = std::cout.rdbuf();
  std::ofstream ofs;

  while (arg_index < argc && strlen(argv[arg_index]) > 0 &&
         argv[arg_index][0] == '-') {
    if (strcmp("-i", argv[arg_index]) == 0) {
      header_only = true;
    } else if (strcmp("-c", argv[arg_index]) == 0) {
      cfi = false;
    } else if (strcmp("-r", argv[arg_index]) == 0) {
      handle_inter_cu_refs = false;
    } else if (strcmp("-v", argv[arg_index]) == 0) {
      log_to_stderr = true;
    } else if (strcmp("-m", argv[arg_index]) == 0) {
      extern bool g_mangle_names;
      g_mangle_names = true;
    } else if (strcmp("-o", argv[arg_index]) == 0) {
      arg_index++;
      ofs.open(argv[arg_index], std::ofstream::out);
      if (ofs.is_open()) {
        os = ofs.rdbuf();
      }
      else {
        fprintf(stderr, "Failed to open file: %s\n", argv[arg_index]);
      }
    } else {
      printf("2.4 %s\n", argv[arg_index]);
      return usage(argv[0]);
    }
    ++arg_index;
  }
  if (arg_index == argc)
    return usage(argv[0]);
  // Save stderr so it can be used below.
  FILE* saved_stderr = fdopen(dup(fileno(stderr)), "w");
  if (!log_to_stderr) {
    if (freopen(_PATH_DEVNULL, "w", stderr)) {
      // If it fails, not a lot we can (or should) do.
      // Add this brace section to silence gcc warnings.
    }
  }
  const char* binary;
  std::vector<string> debug_dirs;
  binary = argv[arg_index];
  for (int debug_dir_index = arg_index + 1;
       debug_dir_index < argc;
       ++debug_dir_index) {
    debug_dirs.push_back(argv[debug_dir_index]);
  }

  std::ostream out(os);
  if (header_only) {
    if (!WriteSymbolFileHeader(binary, out)) {
      fprintf(saved_stderr, "Failed to process file.\n");
      ofs.close();
      return 1;
    }
  } else {
    SymbolData symbol_data = cfi ? ALL_SYMBOL_DATA : NO_CFI;
    google_breakpad::DumpOptions options(symbol_data, handle_inter_cu_refs);
    if (!WriteSymbolFile(binary, debug_dirs, options, out)) {
      fprintf(saved_stderr, "Failed to write symbol file.\n");
      ofs.close();
      return 1;
    }
  }

  return 0;
}
