// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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
#include "MuT/NodeModifierMeshClipWithUVMask.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/NodeSurfaceSwitch.h"
#include "MuT/ASTOpParameter.h"

namespace mu
{
	class Layout;
	struct FObjectState;

	/** First pass of the code generation process.
	 * It collects data about the object hierarchy, the conditions for each object and the global modifiers.
	 */
	class FirstPassGenerator
	{
	public:

		FirstPassGenerator();

        void Generate(Ptr<ErrorLog>, const Node* Root, bool bIgnoreStates, class CodeGenerator*);

	private:

		void Generate_Generic(const Node*);
		void Generate_Modifier(const NodeModifier*);
		void Generate_SurfaceNew(const NodeSurfaceNew*);
		void Generate_SurfaceEdit(const NodeSurfaceEdit*);
		void Generate_SurfaceSwitch(const NodeSurfaceSwitch*);
		void Generate_SurfaceVariation(const NodeSurfaceVariation*);
		void Generate_ComponentNew(const NodeComponentNew*);
		void Generate_ComponentEdit(const NodeComponentEdit*);
		void Generate_LOD(const NodeLOD*);
		void Generate_ObjectNew(const NodeObjectNew*);
		void Generate_ObjectGroup(const NodeObjectGroup*);

	public:

		// Results
		//-------------------------

		//! Store the conditions that will enable or disable every object
		struct FObject
		{
			const NodeObjectNew::Private* node;
            Ptr<ASTOp> condition;
		};
		TArray<FObject> objects;

        //! Type used to represent the activation conditions regarding states
        //! This is the state mask for the states in which this surface must be added. If it
        //! is empty it means the surface is valid for all states. Otherwise it is only valid
        //! for the states whose index is true.
        using StateCondition = TArray<uint8>;

		//! Store information about every surface including
		//! - the component it may be added to
		//! - the conditions that will enable or disable it
		//! - all edit operators
        //! A surface may have different versions depending on the different parents and conditions
        //! it is reached with.
		struct FSurface
		{
            NodeSurfaceNewPtrConst node;

			// Parent component where this surface will be added. It may be different from the 
			// component that defined it (if it was an edit component).
            const NodeComponentNew::Private* component = nullptr;

            // List of tags that are required for the presence of this surface
			TArray<FString> positiveTags;

            // List of tags that block the presence of this surface
			TArray<FString> negativeTags;

			// This conditions is the condition of the object defining this surface which may not
			// be the parent object where this surface will be added.
            Ptr<ASTOp> objectCondition;

            // This is filled in the first pass.
            StateCondition stateCondition;

            // Condition for this surface to be enabled when all the object conditions are met.
            // This is filled in CodeGenerator_SecondPass.
            Ptr<ASTOp> surfaceCondition;

			// All surface editing nodes that edit this surface
            struct FEdit
            {
            	// List of tags that are required for the presence of this surface
            	TArray<FString> PositiveTags;

            	// List of tags that block the presence of this surface
            	TArray<FString> NegativeTags;
            	
                //! Condition that enables the effects of this edit node on the surface
                Ptr<ASTOp> condition;

                //! Weak reference to the edit node, used during compilation.
                const NodeSurfaceEdit::Private* node = nullptr;
            };
			TArray<FEdit> edits;

            // This is filled in the final code generation pass
            Ptr<ASTOp> resultSurfaceOp;
            Ptr<ASTOp> resultMeshOp;
        };
		TArray<FSurface> surfaces;

		//! Store the conditions that enable every modifier.
		struct FModifier
		{
            const NodeModifier::Private* node = nullptr;

            // List of tags that are required to apply this modifier
			TArray<FString> positiveTags;

            // List of tags that block the activation of this modifier
			TArray<FString> negativeTags;

            // This conditions is the condition of the object defining this modifier which may not
            // be the parent object where this surface will be added.
            Ptr<ASTOp> objectCondition;

            // This conditions is the condition for this modifier to be enabled when all the object conditions are met.
            // This is filled in CodeGenerator_SecondPass.
            Ptr<ASTOp> surfaceCondition;

            // This is filled in CodeGenerator_SecondPass.
            StateCondition stateCondition;

            //
            int32 lod = 0;
        };
		TArray<FModifier> modifiers;

		//! Info about all found tags.
		struct FTag
		{
			FString tag;

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
        TArray<FTag> m_tags;

        //! Accumulate the model states found while generating code, with their generated root
        //! nodes.
        typedef TArray< TPair<FObjectState, const Node::Private*> > StateList;
        StateList m_states;

		/** Parameters added for every node. */
		TMap< Ptr<const Node>, Ptr<ASTOpParameter> > ParameterNodes;

	private:

        struct FConditionContext
        {
            Ptr<ASTOp> objectCondition;
        };
		TArray< FConditionContext > m_currentCondition;

        //!
		TArray< StateCondition > m_currentStateCondition;

		//! When processing surfaces, this is the parent component the surfaces may be added to
        const NodeComponentNew::Private* m_currentComponent = nullptr;

        //! Current relevant tags so far. Used during traversal.
		TArray<FString> m_currentPositiveTags;
		TArray<FString> m_currentNegativeTags;

		//! Index of the LOD we are processing
        int m_currentLOD = -1;

		/** Non-owned reference to main code generator. */
		CodeGenerator* Generator = nullptr;

        //!
        ErrorLogPtr m_pErrorLog;

        //!
        bool m_ignoreStates = false;
	};

}
