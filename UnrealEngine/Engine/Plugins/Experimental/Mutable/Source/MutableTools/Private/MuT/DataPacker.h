// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Operations.h"
#include "MuT/AST.h"

namespace mu { struct FProgram; }


#define MUTABLE_HASH_SEED					((uint32)0xcadababa)


namespace mu
{
	class CompilerOptions;

    /** Convert constant data to different formats, based on their usage. */
    extern void DataOptimise( const CompilerOptions*, ASTOpList& roots );

    //---------------------------------------------------------------------------------------------
    //! Find the given constant in a subtree
    //---------------------------------------------------------------------------------------------
    class SubtreeSearchConstantVisitor
    {
    public:

        SubtreeSearchConstantVisitor( FProgram& program, OP::ADDRESS constant, OP_TYPE optype );

        bool Run( OP::ADDRESS root );


    private:

        FProgram& m_program;
        OP::ADDRESS m_constant;
        OP_TYPE m_opType;

        //! 0: not visited
        //! 1: visited and not found
        //! 2: visited and found
        TArray<uint8> m_visited;
    };

}
