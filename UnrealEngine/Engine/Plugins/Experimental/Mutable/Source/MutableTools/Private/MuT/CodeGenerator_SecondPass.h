// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformCrt.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/MutableMemory.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/AST.h"
#include "MuT/CodeGenerator_FirstPass.h"
#include "MuT/Compiler.h"
#include "MuT/Node.h"
#include "map"
#include "set"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//! Second pass of the code generation process.
    //! Solves surface and modifier conditions from tags and variations
	//---------------------------------------------------------------------------------------------
    class SecondPassGenerator : public Base
	{
	public:

		SecondPassGenerator( FirstPassGenerator* firstPass,
            const CompilerOptions::Private* options  );

		// Return true on success.
        bool Generate( ErrorLogPtr pErrorLog,
                       const Node::Private* root );

	private:

        FirstPassGenerator* m_pFirstPass = nullptr;
        const CompilerOptions::Private *m_pCompilerOptions = nullptr;


        struct CONDITION_CONTEXT
        {
            Ptr<ASTOp> surfaceCondition;
            FirstPassGenerator::StateCondition stateCondition;
        };
        vector< CONDITION_CONTEXT > m_currentCondition;


        //!
        ErrorLogPtr m_pErrorLog;

        //!
        struct CONDITION_GENERATION_KEY
        {
            size_t tagOrSurfIndex = 0;
            set<size_t> posSurf;
            set<size_t> negSurf;
            set<size_t> posTag;
            set<size_t> negTag;

            inline bool operator<(const CONDITION_GENERATION_KEY& o) const
            {
                if (tagOrSurfIndex<o.tagOrSurfIndex) return true;
                if (tagOrSurfIndex>o.tagOrSurfIndex) return false;
                if (posSurf<o.posSurf) return true;
                if (posSurf>o.posSurf) return false;
                if (negSurf<o.negSurf) return true;
                if (negSurf>o.negSurf) return false;
                if (posTag<o.posTag) return true;
                if (posTag>o.posTag) return false;
                if (negTag<o.negTag) return true;
                if (negTag>o.negTag) return false;
                return false;
            }
        };

        // List of surfaces that activate or deactivate every tag, or another surface that activates a tag in this set.
		vector< set<size_t> > m_surfacesPerTag;
		vector< set<size_t> > m_tagsPerTag;

        std::map<CONDITION_GENERATION_KEY,Ptr<ASTOp>> m_tagConditionGenerationCache;

        UniqueOpPool m_opPool;

        //!
        Ptr<ASTOp> GenerateTagCondition( size_t tagIndex,
                                         const set<size_t>& posSurf,
                                         const set<size_t>& negSurf,
                                         const set<size_t>& posTag,
                                         const set<size_t>& negTag );

        //!
        Ptr<ASTOp> GenerateSurfaceCondition( size_t surfIndex,
                                             const set<size_t>& posSurf,
                                             const set<size_t>& negSurf,
                                             const set<size_t>& posTag,
                                             const set<size_t>& negTag );

        //!
        Ptr<ASTOp> GenerateModifierCondition( size_t modIndex );
    };

}

