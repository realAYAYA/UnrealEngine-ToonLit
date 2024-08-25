// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;
	template <class SCALAR> class vec4;


	//---------------------------------------------------------------------------------------------
	//! A reference to an engine image (or other resources in the future)
	//---------------------------------------------------------------------------------------------
	class ASTOpReferenceResource final : public ASTOp
	{
	public:

		//! Type of switch
		OP_TYPE type = OP_TYPE::NONE;

		//!
		uint32 ID = 0;
		
		/** */
		bool bForceLoad = false;

		FImageDesc ImageDesc;

	public:

		// Own interface

		// ASTOp interface
		OP_TYPE GetOpType() const override { return type; }
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		uint64 Hash() const override;
		void Link(FProgram& program, FLinkerOptions*) override;
		FImageDesc GetImageDesc(bool, class FGetImageDescContext*) const override;
		void GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache) override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		bool GetNonBlackRect(FImageRect& maskUsage) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
		Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const override;
	};


}

