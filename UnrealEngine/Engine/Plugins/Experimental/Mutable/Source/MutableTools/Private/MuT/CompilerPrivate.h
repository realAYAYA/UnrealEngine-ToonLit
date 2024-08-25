// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Compiler.h"

#include "MuT/AST.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/NodeObjectPrivate.h"
#include "MuT/StreamsPrivate.h"
#include "MuR/Operations.h"


namespace mu
{
		
    //!
    class CompilerOptions::Private
    {
    public:

        //! Detailed optimization options
        FModelOptimizationOptions OptimisationOptions;
		FProxyFileContext DiskCacheContext;

		uint64 EmbeddedDataBytesLimit = 1024;
		uint64 PackagedDataBytesLimit = 1024*1024*64;

		// \TODO: Unused?
		int32 MinTextureResidentMipCount = 3;

        int32 ImageCompressionQuality = 0;
		int32 ImageTiling = 0 ;

		/** If this flag is enabled, the compiler can use concurrency to reduce compile time at the cost of higher CPU and memory usage. */
		bool bUseConcurrency = false;

		bool bIgnoreStates = false;
		bool bLog = false;

		FImageOperator::FImagePixelFormatFunc ImageFormatFunc;
    };


    //!
    struct FStateCompilationData
    {
        FObjectState nodeState;
        Ptr<ASTOp> root;
        FProgram::FState state;
		//FStateOptimizationOptions optimisationFlags;

        //! List of instructions that need to be cached to efficiently update this state
        TArray<Ptr<ASTOp>> m_updateCache;

        //! List of root instructions for the dynamic resources that depend on the runtime
        //! parameters of this state.
		TArray< TPair<Ptr<ASTOp>, TArray<FString> > > m_dynamicResources;
    };


    //!
    class Compiler::Private
    {
    public:

        Private()
        {
            m_pErrorLog = new ErrorLog();
        }

        Ptr<ErrorLog> m_pErrorLog;

        //! Detailed options
        Ptr<CompilerOptions> m_options;


		//!
		void GenerateRoms(Model* p, int32 MinRomSize);

    };

}
