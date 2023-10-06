// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#include <cstdint>

#include "raw_pdb/Foundation/PDB_Warnings.h"
#include "raw_pdb/Foundation/PDB_DisableWarningsPush.h"
#include "raw_pdb/PDB.h"
#include "raw_pdb/PDB_RawFile.h"
#include "raw_pdb/PDB_InfoStream.h"
#include "raw_pdb/PDB_DBIStream.h"
#include "raw_pdb/PDB_TPIStream.h"

#include "raw_pdb/Foundation/PDB_DisableWarningsPop.h"

#endif //PCH_H
