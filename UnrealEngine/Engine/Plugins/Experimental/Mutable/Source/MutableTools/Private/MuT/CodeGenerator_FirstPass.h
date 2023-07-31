// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MemoryPrivate.h"
#include "MuR/MutableMemory.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/AST.h"
#include "MuT/Compiler.h"
#include "MuT/Node.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeModifier.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeObjectState.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/Visitor.h"

#include <stdint.h>
#include <memory>
#include <utility>

namespace mu
{
	class Layout;
	struct OBJECT_STATE;

	//---------------------------------------------------------------------------------------------
	//! First pass of the code generation process.
	//! It collects data about the object hierarchy, the conditions for each object and the global
	//! modifiers.
	//---------------------------------------------------------------------------------------------
	class FirstPassGenerator :
		public Base,
		public BaseVisitor,

        public Visitor<NodeSurfaceNew::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeSurfaceEdit::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeSurfaceVariation::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeComponentNew::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeComponentEdit::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeLOD::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeObjectNew::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeObjectGroup::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeObjectState::Private, Ptr<ASTOp>, true>,
        public Visitor<NodePatchMesh::Private, Ptr<ASTOp>, true>,

        public Visitor<NodeModifierMeshClipMorphPlane::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeModifierMeshClipWithMesh::Private, Ptr<ASTOp>, true>,
        public Visitor<NodeModifierMeshClipDeform::Private, Ptr<ASTOp>, true>
	{
	public:

		FirstPassGenerator();

        void Generate(ErrorLogPtr pErrorLog,
                      const Node::Private* root,
                      bool ignoreStates);

	public:

		// Results
		//-------------------------

		//! Store the conditions that will enable or disable every object
		struct OBJECT
		{
			const NodeObjectNew::Private* node;
            Ptr<ASTOp> condition;
		};
		TArray<OBJECT> objects;

        //! Type used to represent the activation conditions regarding states
        //! This is the state mask for the states in which this surface must be added. If it
        //! is empty it means the surface is valid for all states. Otherwise it is only valid
        //! for the states whose index is true.
        using StateCondition = TArray<uint8_t>;

		//! Store information about every surface including
		//! - the component it may be added to
		//! - the conditions that will enable or disable it
		//! - all edit operators
        //! A surface may have different versions depending on the different parents and conditions
        //! it is reached with.
		struct SURFACE
		{
            NodeSurfaceNewPtrConst node;

			// Parent component where this surface will be added. It may be different from the 
			// component that defined it (if it was an edit component).
            const NodeComponentNew::Private* component = nullptr;

            // List of tags that are required for the presence of this surface
			TArray<string> positiveTags;

            // List of tags that block the presence of this surface
			TArray<string> negativeTags;

			// This conditions is the condition of the object defining this surface which may not
			// be the parent object where this surface will be added.
            Ptr<ASTOp> objectCondition;

            // This is filled in the first pass.
            StateCondition stateCondition;

            // Condition for this surface to be enabled when all the object conditions are met.
            // This is filled in CodeGenerator_SecondPass.
            Ptr<ASTOp> surfaceCondition;

			// All surface editing nodes that edit this surface
            struct EDIT
            {
                //! Condition that enables the effects of this edit node on the surface
                Ptr<ASTOp> condition;

                //! Weak reference to the edit node, used during compilation.
                const NodeSurfaceEdit::Private* node = nullptr;
            };
			TArray<EDIT> edits;

            // This is filled in the final code generation pass
            Ptr<ASTOp> resultSurfaceOp;
            Ptr<ASTOp> resultMeshOp;
        };
		TArray<SURFACE> surfaces;

		//! Store the conditions that enable every modifier.
		struct MODIFIER
		{
            const NodeModifier::Private* node = nullptr;

            // List of tags that are required for the presence of this surface
			TArray<string> positiveTags;

            // List of tags that block the presence of this surface
			TArray<string> negativeTags;

            // This conditions is the condition of the object defining this modifier which may not
            // be the parent object where this surface will be added.
            Ptr<ASTOp> objectCondition;

            // This conditions is the condition for this modifier to be enabled when all the object
            // conditions are met.
            // This is filled in CodeGenerator_SecondPass.
            Ptr<ASTOp> surfaceCondition;

            // This is filled in CodeGenerator_SecondPass.
            StateCondition stateCondition;

            //
            int lod = 0;
        };
		TArray<MODIFIER> modifiers;

		//! Info about all found tags.
		struct TAG
		{
			string tag;

            // Surfaces that activate the tag. These are indices to the FirstPassGenerator::surfaces
            // vector.
			TArray<int> surfaces;

            // Edit Surfaces that activate the tag. These first element of the pair are indices to
            // the FirstPassGenerator::surfaces vector. The second element are indices to the
            // "edits" in the specific surface.
			TArray<TPair<int,int>> edits;

            // This conditions is the condition for this tag to be enabled considering no other
            // condition. This is filled in CodeGenerator_SecondPass.
            Ptr<ASTOp> genericCondition;
        };
        TArray<TAG> m_tags;

        //! Accumulate the model states found while generating code, with their generated root
        //! nodes.
        typedef TArray< TPair<OBJECT_STATE, const Node::Private*> > StateList;
        StateList m_states;

	public:

        Ptr<ASTOp> Visit(const NodeSurfaceNew::Private&) override;
        Ptr<ASTOp> Visit(const NodeSurfaceEdit::Private&) override;
        Ptr<ASTOp> Visit(const NodeSurfaceVariation::Private&) override;
        Ptr<ASTOp> Visit(const NodeComponentNew::Private&) override;
        Ptr<ASTOp> Visit(const NodeComponentEdit::Private&) override;
        Ptr<ASTOp> Visit(const NodeLOD::Private&) override;
        Ptr<ASTOp> Visit(const NodeObjectNew::Private& node) override;
        Ptr<ASTOp> Visit(const NodeObjectGroup::Private&) override;
        Ptr<ASTOp> Visit(const NodeObjectState::Private&) override;
        Ptr<ASTOp> Visit(const NodeModifierMeshClipMorphPlane::Private&) override;
        Ptr<ASTOp> Visit(const NodeModifierMeshClipWithMesh::Private&) override;
        Ptr<ASTOp> Visit(const NodeModifierMeshClipDeform::Private&) override;
        Ptr<ASTOp> Visit(const NodePatchMesh::Private&) override;

	private:

        struct CONDITION_CONTEXT
        {
            Ptr<ASTOp> objectCondition;
        };
		TArray< CONDITION_CONTEXT > m_currentCondition;

        //!
		TArray< StateCondition > m_currentStateCondition;

		//! When processing surfaces, this is the parent component the surfaces may be added to
        const NodeComponentNew::Private* m_currentComponent = nullptr;

        //! Current relevant tags so far. Used during traversal.
		TArray<string> m_currentPositiveTags;
		TArray<string> m_currentNegativeTags;

		//! Index of the LOD we are processing
        int m_currentLOD = -1;

        //!
        ErrorLogPtr m_pErrorLog;

        //!
        bool m_ignoreStates = false;
	};

}
