// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshBindShape : public ASTOp
	{
	public:

		ASTChild Mesh;
		ASTChild Shape;

		TArray<string> m_bonesToDeform;
		TArray<string> m_physicsToDeform;

		bool m_reshapeSkeleton = false;
		bool m_discardInvalidBindings = true;
		bool m_enableRigidParts = false;
		bool m_deformAllBones = false;
		bool m_deformAllPhysics = false;
		bool m_reshapePhysicsVolumes = false;
		bool m_reshapeVertices = true;

		uint32 m_bindingMethod = 0;


	public:

		ASTOpMeshBindShape();
		ASTOpMeshBindShape(const ASTOpMeshBindShape&) = delete;
		~ASTOpMeshBindShape();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_BINDSHAPE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS&, OPTIMIZE_SINK_CONTEXT&) const override;

	};

}

