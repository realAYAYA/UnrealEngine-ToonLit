// Copyright (c) 2006, Google Inc.
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

// Windows utility to dump the line number data from a pdb file to
// a text-based format that we can use from the minidump processor.

#include <io.h>
#include <stdio.h>
#include <wchar.h>

#include <string>
#include <fstream>

#include "common/linux/dump_symbols.h"
#include "common/windows/pdb_source_line_writer.h"

using std::wstring;
using google_breakpad::PDBSourceLineWriter;

int usage(const wchar_t* self) {
    fprintf(stderr, "Usage for PDB: %ws <file.[pdb|exe|dll]>\n", self);
#ifdef DUMP_SYMS_WITH_EPIC_EXTENSIONS
    fprintf(stderr, "Usage for ELF: %ws [Options] <Input> <Output>\n", self);
    fprintf(stderr, "Options for ELF:\n");
    fprintf(stderr, "  -m    Keep the C++ Mangled names\n");
    fprintf(stderr, "  -v    Print all warnings to stderr\n");
#endif
    return 1;
}

int wmain(int argc, wchar_t **argv) {
  if (argc < 2) {
    return usage(argv[0]);
  }

#ifdef DUMP_SYMS_WITH_EPIC_EXTENSIONS
  int arg_index = 1;
  bool log_to_stderr = false;

  while (arg_index < argc && wcslen(argv[arg_index]) > 0 &&
         argv[arg_index][0] == '-') {
    if (wcscmp(L"-m", argv[arg_index]) == 0) {
      extern bool g_mangle_names;
      g_mangle_names = true;
    } else if (wcscmp(L"-v", argv[arg_index]) == 0) {
      log_to_stderr = true;
    }
    ++arg_index;
  }

  if (arg_index == argc) {
    return usage(argv[0]);
  }

  FILE* saved_stderr = _fdopen(_dup(_fileno(stderr)), "w");
  if (!log_to_stderr) {
    if (freopen("nul", "w", stderr)) {
      // If it fails, not a lot we can (or should) do.
    }
  }
#endif

  PDBSourceLineWriter writer;
  // Disable CFI
  bool cfi = false;
  bool handle_inter_cu_refs = true;
  if (!writer.Open(wstring(argv[arg_index]), PDBSourceLineWriter::ANY_FILE)) {
    std::vector<string> debug_dirs;
    std::wstring s(argv[arg_index]);

    std::streambuf* os = std::cout.rdbuf();
    std::ofstream ofs;

    // Make sure we still have input and output args
    if (arg_index + 1 < argc)
    {
      std::wstring output(argv[arg_index + 1]);
      ofs.open(output, std::ofstream::out);
      if (ofs.is_open()) {
        os = ofs.rdbuf();
      }
    }

    std::ostream out(os);
    SymbolData symbol_data = cfi ? ALL_SYMBOL_DATA : NO_CFI;
    google_breakpad::DumpOptions options(symbol_data, handle_inter_cu_refs);
    if (!WriteSymbolFile(std::string(s.begin(), s.end()), debug_dirs, options, out)) {
      fprintf(saved_stderr, "Failed to write symbol file.\n");
      ofs.close();
      return 1;
    }

    ofs.close();
    return 0;
  }

  if (!writer.WriteMap(stdout)) {
    fprintf(saved_stderr, "WriteMap failed\n");
    return 1;
  }

  writer.Close();
  return 0;
}
