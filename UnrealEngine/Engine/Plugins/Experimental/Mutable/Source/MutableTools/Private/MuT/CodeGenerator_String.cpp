// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConstantString.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CodeGenerator.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeString.h"
#include "MuT/NodeStringConstant.h"
#include "MuT/NodeStringConstantPrivate.h"
#include "MuT/NodeStringParameter.h"
#include "MuT/NodeStringParameterPrivate.h"
#include "map"

#include <memory>
#include <utility>


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateString(FStringGenerationResult& result, const NodeStringPtrConst& untyped)
	{
		if (!untyped)
		{
			result = FStringGenerationResult();
			return;
		}

		// See if it was already generated
		FVisitedKeyMap key = GetCurrentCacheKey(untyped);
		GeneratedStringsMap::ValueType* it = m_generatedStrings.Find(key);
		if (it)
		{
			result = *it;
			return;
		}

		// Generate for each different type of node
		if (auto Constant = dynamic_cast<const NodeStringConstant*>(untyped.get()))
		{
			GenerateString_Constant(result, Constant);
		}
		else if (auto Param = dynamic_cast<const NodeStringParameter*>(untyped.get()))
		{
			GenerateString_Parameter(result, Param);
		}
		else
		{
			check(false);
		}

		// Cache the result
		m_generatedStrings.Add(key, result);
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateString_Constant(FStringGenerationResult& result, const Ptr<const NodeStringConstant>& Typed)
	{
		const NodeStringConstant::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpConstantString> op = new ASTOpConstantString();
		op->value = node.m_value;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateString_Parameter(FStringGenerationResult& result, const Ptr<const NodeStringParameter>& Typed)
	{
		const NodeStringParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		auto it = m_nodeVariables.find(node.m_pNode);
		if (it == m_nodeVariables.end())
		{
			FParameterDesc param;
			param.m_name = node.m_name;
			param.m_uid = node.m_uid;
			param.m_type = PARAMETER_TYPE::T_FLOAT;
			param.m_detailedType = node.m_detailedType;
			param.m_defaultValue.Set<ParamStringType>(node.m_defaultValue);

			op = new ASTOpParameter();
			op->type = OP_TYPE::ST_PARAMETER;
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


}
