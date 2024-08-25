// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Operations.h"
#include "MuR/CodeVisitor.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/AST.h"

namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Code optimiser
    //---------------------------------------------------------------------------------------------
    class CodeOptimiser
    {
    public:
        CodeOptimiser( Ptr<CompilerOptions> options, TArray<FStateCompilationData>& states );

        //! Optimise the virtual machine code, using several transforms.
        void OptimiseAST();

    private:

		Ptr<CompilerOptions> m_options;

        //!
		TArray<FStateCompilationData>& m_states;

        //! The max number of optimize iterations is shared across several stages now.
        //! This is how many are left
        int m_optimizeIterationsMax=0;
        int m_optimizeIterationsLeft=0;

        //! Full optimisation pass
        void FullOptimiseAST( ASTOpList& roots, int32 Pass);

        //! Optimise the code of a model for a specific state, generating new instructions and
        //! state information.
        void OptimiseStatesAST();

    };


    //---------------------------------------------------------------------------------------------
    //! Scan a subtree and return true if any op of a type in the provided subset is found.
    //! Intermediate data is used between calls to apply, so don't remove program code or directly
    //! change the instructions. Adding new instructions is ok.
    //---------------------------------------------------------------------------------------------
    class FindOpTypeVisitor
    {
    public:

        FindOpTypeVisitor(const TArray<OP_TYPE>& types)
            : m_typesToFind(types)
        {
        }

        bool Apply( FProgram& program, OP::ADDRESS at );


    private:

        TArray<OP_TYPE> m_typesToFind;

		TArray< TPair<bool,OP::ADDRESS> > m_pending;

        // 0 not visited
        // 1 children pending
        // 2 visited and not found
        // 3 visited and found
		TArray<uint8> m_visited;
    };


    //---------------------------------------------------------------------------------------------
    //! Scan a subtree and return true if the expression is constant.
    //! It has the same restrictions that FindOpTypeVisitor
    //---------------------------------------------------------------------------------------------
    class IsConstantVisitor
    {
    public:

        IsConstantVisitor();

        bool Apply( FProgram& program, OP::ADDRESS at );

    private:

        TUniquePtr<FindOpTypeVisitor> m_findOpTypeVisitor;
    };


    //---------------------------------------------------------------------------------------------
    //! ConstantGenerator replaces constant subtrees of operations with an equivalent single
	//! constant value operation. 
    //---------------------------------------------------------------------------------------------
    extern bool ConstantGeneratorAST( const CompilerOptions::Private* options, Ptr<ASTOp>& root, int32 Pass );

    //---------------------------------------------------------------------------------------------
    //! \TODO: shapes, projectors, others? but not switches (they must be unique)
    //---------------------------------------------------------------------------------------------
    extern void DuplicatedDataRemoverAST( ASTOpList& roots );


    //---------------------------------------------------------------------------------------------
    //! Mark all the duplicated code instructions to point at the same operation, leaving the
    //! copies unreachable.
    //---------------------------------------------------------------------------------------------
    extern void DuplicatedCodeRemoverAST( ASTOpList& roots );


    //---------------------------------------------------------------------------------------------
    //! All kinds of optimisations that depend on the meaning of each operation
    //---------------------------------------------------------------------------------------------
    extern bool SemanticOptimiserAST( ASTOpList& roots, const FModelOptimizationOptions& optimisationOptions, int32 Pass);

    //---------------------------------------------------------------------------------------------
    //! Semantic operator that reorders instructions moving expensive ones down to the
    //! leaves of the expressions trying to turn them into constants.
    //---------------------------------------------------------------------------------------------
    extern bool SinkOptimiserAST( ASTOpList& roots, const FModelOptimizationOptions& );

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    extern bool SizeOptimiserAST( ASTOpList& roots );

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    extern bool LocalLogicOptimiserAST(ASTOpList& roots);

    //---------------------------------------------------------------------------------------------
    //! Discard all LODs beyond the given lod count.
    //---------------------------------------------------------------------------------------------
    class LODCountReducerAST : public Visitor_TopDown_Unique_Cloning
    {
    public:

        LODCountReducerAST( Ptr<ASTOp>& root, int lodCount );

    protected:

        Ptr<ASTOp> Visit( Ptr<ASTOp> at, bool& processChildren ) override;

        int m_lodCount;
    };


    //---------------------------------------------------------------------------------------------
    //! Scan the code in the given subtree and return true if a state runtime parameters is
    //! found.
    //! Intermediate data is used betwen calls to apply, so don't remove program code or directly
    //! change the instructions. Adding new instructions is ok.
    //---------------------------------------------------------------------------------------------
    class RuntimeParameterVisitorAST
    {
    public:

        RuntimeParameterVisitorAST(const FStateCompilationData* pState);

        bool HasAny( const Ptr<ASTOp>& root );

    private:

        const FStateCompilationData* m_pState;

        //!
        struct PENDING_ITEM
        {
            //! 0: indicate subtree pending
            //! 1: indicate children finished
            uint8_t itemType;

            //! 0: everything is relevant
            //! 1: only layouts are relevant
            uint8_t onlyLayoutsRelevant;

            //! Address to visit
            Ptr<ASTOp> at;
        };

        //!
        vector< PENDING_ITEM > m_pending;

        //! Possible op state
        enum class OP_STATE : uint8_t
        {
            NOT_VISITED = 0,
            CHILDREN_PENDING_FULL,
            CHILDREN_PENDING_PARTIAL,
            VISITED_HASRUNTIME,
            VISITED_FULL_DOESNTHAVERUNTIME,
            VISITED_PARTIAL_DOESNTHAVERUNTIME
        };

        std::unordered_map<Ptr<ASTOp>,OP_STATE> m_visited;

        //!
        void AddIfNeeded( const PENDING_ITEM& item );

    };


    //---------------------------------------------------------------------------------------------
    //! Remove all texture compression operations that would happen for runtime parameter changes.
    //---------------------------------------------------------------------------------------------
    class RuntimeTextureCompressionRemoverAST : public Visitor_TopDown_Unique_Cloning
    {
    public:

        RuntimeTextureCompressionRemoverAST(FStateCompilationData* state, bool bInAlwaysUncompress );

    protected:

        Ptr<ASTOp> Visit( Ptr<ASTOp> at, bool& processChildren ) override;

    private:

        RuntimeParameterVisitorAST m_hasRuntimeParamVisitor;
		bool bAlwaysUncompress = false;

    };

    //---------------------------------------------------------------------------------------------
    //! Restructure the code to move operations involving runtime parameters as high as possible
    //! in the code tree.
    //---------------------------------------------------------------------------------------------
    class ParameterOptimiserAST : public Visitor_TopDown_Unique_Cloning
    {
    private:

		FStateCompilationData& m_stateProps;
        // unused const GPU_PLATFORM_PROPS& m_gpuPlatformProps;

    public:

        ParameterOptimiserAST(FStateCompilationData&s,
                               const FModelOptimizationOptions& optimisationOptions );

        bool Apply();


    private:

        Ptr<ASTOp> Visit( Ptr<ASTOp> at, bool& processChildren ) override;

        bool m_modified;

        FModelOptimizationOptions OptimisationOptions;

        RuntimeParameterVisitorAST m_hasRuntimeParamVisitor;

    };


    //---------------------------------------------------------------------------------------------
    //! Some masks are optional. If they are null, replace them by a white plain image of the right
    //! size.
    //---------------------------------------------------------------------------------------------
    extern Ptr<ASTOp> EnsureValidMask( Ptr<ASTOp> mask, Ptr<ASTOp> base );


    //---------------------------------------------------------------------------------------------
    //! Calculate all the parameters found relevant under a particular operation. This may not
    //! incldue all the parameters in the subtree (if because of the operations they are not
    //! relevant)
    //! It has an internal cache, so don't reuse if the program changes.
    //---------------------------------------------------------------------------------------------
    class SubtreeRelevantParametersVisitorAST
    {
    public:

        void Run( Ptr<ASTOp> root );

        //! After Run, list of relevant parameters.
        TSet< FString > m_params;

    private:

        struct FState
        {
            Ptr<ASTOp> op;
            bool bOnlyLayoutIsRelevant=false;

			FState( Ptr<ASTOp> o=nullptr, bool l=false) : op(o), bOnlyLayoutIsRelevant(l) {}

            bool operator==(const FState& o) const
            {
                return  op == o.op &&
					bOnlyLayoutIsRelevant == o.bOnlyLayoutIsRelevant;
            }

			friend FORCEINLINE uint32 GetTypeHash(const FState& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.op.get()));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.bOnlyLayoutIsRelevant));
				return KeyHash;
			}
		};


        // Result cache
        // \todo optimise by storing unique lists separately and an index here.
        TMap< FState, TSet<FString> > m_resultCache;
    };

}
