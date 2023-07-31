// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Compiler.h"

#include "MuT/AST.h"
#include "MuT/ErrorLogPrivate.h"

#include "MuR/Operations.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeObjectPrivate.h"


namespace mu
{

    //!
    struct STATE_OPTIMIZATION_OPTIONS
    {
        STATE_OPTIMIZATION_OPTIONS()
        {
			m_firstLOD = 0;
            m_onlyFirstLOD = false;
            m_avoidRuntimeCompression = false;
        }

		uint8 m_firstLOD;
		bool m_onlyFirstLOD;
        bool m_avoidRuntimeCompression;

        void Serialise( OutputArchive& arch ) const
        {
            const int32_t ver = 2;
            arch << ver;

			arch << m_firstLOD;
			arch << m_onlyFirstLOD;
            arch << m_avoidRuntimeCompression;
        }


        void Unserialise( InputArchive& arch )
        {
            int32_t ver = 0;
            arch >> ver;
			check(ver <= 2);

			if (ver >= 2)
			{
				arch >> m_firstLOD;
			}
			else
			{
				m_firstLOD = 0;
			}

            arch >> m_onlyFirstLOD;
            arch >> m_avoidRuntimeCompression;
        }
    };


    //!
    class CompilerOptions::Private : public Base
    {
    public:

        //! Detailed optimization options
        MODEL_OPTIMIZATION_OPTIONS m_optimisationOptions;

        bool m_enablePartialOptimise = false;
		bool m_enableConcurrency = false;
        bool m_ignoreStates = false;
        CompilerOptions::TextureLayoutStrategy m_textureLayoutStrategy = CompilerOptions::TextureLayoutStrategy::Pack;

		int MinRomSize = 3;
		int MinTextureResidentMipCount = 3;

        int m_imageCompressionQuality = 0;

        bool m_log = false;

    };


    //! Information about an object state in the source data
    struct OBJECT_STATE
    {
        //! Name used to identify the state from the code and user interface.
        string m_name;

        //! GPU Optimisation options
        STATE_OPTIMIZATION_OPTIONS m_optimisation;

        //! List of names of the runtime parameters in this state
        TArray<string> m_runtimeParams;

        void Serialise( OutputArchive& arch ) const
        {
            const int32_t ver = 5;
            arch << ver;

            arch << m_name;
            arch << m_optimisation;
            arch << m_runtimeParams;
        }


        void Unserialise( InputArchive& arch )
        {
            int32_t ver = 0;
            arch >> ver;
            check( ver==5 );

            arch >> m_name;
            arch >> m_optimisation;
            arch >> m_runtimeParams;
        }
    };


    //!
    struct STATE_COMPILATION_DATA
    {
        OBJECT_STATE nodeState;
        Ptr<ASTOp> root;
        PROGRAM::STATE state;
        STATE_OPTIMIZATION_OPTIONS optimisationFlags;

        //! List of instructions that need to be cached to efficiently update this state
        TArray<Ptr<ASTOp>> m_updateCache;

        //! List of root instructions for the dynamic resources that depend on the runtime
        //! parameters of this state.
		TArray< TPair<Ptr<ASTOp>, TArray<string> > > m_dynamicResources;
    };


    //!
    class Compiler::Private : public Base
    {
    public:

        Private()
        {
            m_pErrorLog = new ErrorLog();
        }

        ErrorLogPtr m_pErrorLog;

        //! Detailed options
        CompilerOptionsPtr m_options;


		//!
		void GenerateRoms(Model* p, int32 MinRomSize);

    };

}
