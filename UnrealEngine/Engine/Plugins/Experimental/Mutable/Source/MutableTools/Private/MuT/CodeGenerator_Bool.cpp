// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CodeGenerator.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeBoolPrivate.h"
#include "MuT/NodeRange.h"
#include "map"

#include <memory>
#include <utility>


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateBool(FBoolGenerationResult& result, const NodeBoolPtrConst& untyped)
	{
		if (!untyped)
		{
			result = FBoolGenerationResult();
			return;
		}

		// See if it was already generated
		FVisitedKeyMap key = GetCurrentCacheKey(untyped);
		GeneratedBoolsMap::ValueType* it = m_generatedBools.Find(key);
		if (it)
		{
			result = *it;
			return;
		}

		// Generate for each different type of node
		if (auto Constant = dynamic_cast<const NodeBoolConstant*>(untyped.get()))
		{
			GenerateBool_Constant(result, Constant);
		}
		else if (auto Param = dynamic_cast<const NodeBoolParameter*>(untyped.get()))
		{
			GenerateBool_Parameter(result, Param);
		}
		else if (auto Switch = dynamic_cast<const NodeBoolIsNull*>(untyped.get()))
		{
			GenerateBool_IsNull(result, Switch);
		}
		else if (auto Sample = dynamic_cast<const NodeBoolNot*>(untyped.get()))
		{
			GenerateBool_Not(result, Sample);
		}
		else if (auto From = dynamic_cast<const NodeBoolAnd*>(untyped.get()))
		{
			GenerateBool_And(result, From);
		}
		else
		{
			check(false);
		}

		// Cache the result
		m_generatedBools.Add(key, result);
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateBool_Constant(FBoolGenerationResult& result, const Ptr<const NodeBoolConstant>& Typed)
	{
		const NodeBoolConstant::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpConstantBool> op = new ASTOpConstantBool();
		op->value = node.m_value;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateBool_Parameter(FBoolGenerationResult& result, const Ptr<const NodeBoolParameter>& Typed)
	{
		const NodeBoolParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		auto it = m_nodeVariables.find(node.m_pNode);
		if (it == m_nodeVariables.end())
		{
			FParameterDesc param;
			param.m_name = node.m_name;
			param.m_uid = node.m_uid;
			param.m_type = PARAMETER_TYPE::T_BOOL;
			param.m_defaultValue.Set<ParamBoolType>(node.m_defaultValue);

			op = new ASTOpParameter();
			op->type = OP_TYPE::BO_PARAMETER;
			op->parameter = param;

			// Generate the code for the ranges
			for (int32 a = 0; a < node.m_ranges.Num(); ++a)
			{
				FRangeGenerationResult rangeResult;
				GenerateRange(rangeResult, node.m_ranges[a]);
				op->ranges.Emplace(op.get(), rangeResult.sizeOp, rangeResult.rangeName, rangeResult.rangeUID);
			}

			m_nodeVariables[node.m_pNode] = op;
		}
		else
		{
			op = it->second;
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateBool_IsNull(FBoolGenerationResult& result, const Ptr<const NodeBoolIsNull>& Typed)
	{
		const NodeBoolIsNull::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpConstantBool> op = new ASTOpConstantBool();
		Ptr<ASTOp> source = Generate(node.m_pSource);
		op->value = !source;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateBool_Not(FBoolGenerationResult& result, const Ptr<const NodeBoolNot>& Typed)
	{
		const NodeBoolNot::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::BO_NOT;
		op->SetChild(op->op.args.BoolNot.source, Generate(node.m_pSource));

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateBool_And(FBoolGenerationResult& result, const Ptr<const NodeBoolAnd>& Typed)
	{
		const NodeBoolAnd::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::BO_AND;
		op->SetChild(op->op.args.BoolBinary.a, Generate(node.m_pA));
		op->SetChild(op->op.args.BoolBinary.b, Generate(node.m_pB));

		result.op = op;
	}


}
