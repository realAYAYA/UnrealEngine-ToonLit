// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConstantProjector.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CodeGenerator.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeProjectorPrivate.h"
#include "MuT/NodeRange.h"
#include "map"

#include <memory>
#include <utility>


namespace mu
{

	//-------------------------------------------------------------------------------------------------
	FProjector ProjectorFromNode(const NodeProjectorConstant::Private& node)
	{
		FProjector p;
		p.type = node.m_type;
		p.position[0] = node.m_position[0];
		p.position[1] = node.m_position[1];
		p.position[2] = node.m_position[2];
		p.direction[0] = node.m_direction[0];
		p.direction[1] = node.m_direction[1];
		p.direction[2] = node.m_direction[2];
		p.up[0] = node.m_up[0];
		p.up[1] = node.m_up[1];
		p.up[2] = node.m_up[2];
		p.scale[0] = node.m_scale[0];
		p.scale[1] = node.m_scale[1];
		p.scale[2] = node.m_scale[2];
		p.projectionAngle = node.m_projectionAngle;

		return p;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateProjector(FProjectorGenerationResult& result, const NodeProjectorPtrConst& untyped)
	{
		if (!untyped)
		{
			result = FProjectorGenerationResult();
			return;
		}

		// See if it was already generated
		FVisitedKeyMap key = GetCurrentCacheKey(untyped);
		GeneratedProjectorsMap::ValueType* it = m_generatedProjectors.Find(key);
		if (it)
		{
			result = *it;
			return;
		}

		// Generate for each different type of node
		if (auto Constant = dynamic_cast<const NodeProjectorConstant*>(untyped.get()))
		{
			GenerateProjector_Constant(result, Constant);
		}
		else if (auto Param = dynamic_cast<const NodeProjectorParameter*>(untyped.get()))
		{
			GenerateProjector_Parameter(result, Param);
		}
		else
		{
			check(false);
		}


		// Cache the result
		m_generatedProjectors.Add(key, result);
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateProjector_Constant(FProjectorGenerationResult& result,
		const Ptr<const NodeProjectorConstant>& constant)
	{
		const NodeProjectorConstant::Private& node = *constant->GetPrivate();

		Ptr<ASTOpConstantProjector> op = new ASTOpConstantProjector();
		op->value = ProjectorFromNode(node);

		result.op = op;
		result.type = op->value.type;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateProjector_Parameter(FProjectorGenerationResult& result,
		const Ptr<const NodeProjectorParameter>& paramn)
	{
		const NodeProjectorParameter::Private& node = *paramn->GetPrivate();

		Ptr<ASTOpParameter> op;

		auto it = m_nodeVariables.find(node.m_pNode);
		if (it == m_nodeVariables.end())
		{
			FParameterDesc param;
			param.m_name = node.m_name;
			param.m_uid = node.m_uid;
			param.m_type = PARAMETER_TYPE::T_PROJECTOR;

			FProjector p = ProjectorFromNode(node);
			param.m_defaultValue.Set<ParamProjectorType>(p);

			op = new ASTOpParameter();
			op->type = OP_TYPE::PR_PARAMETER;
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

		result.type = op->parameter.m_defaultValue.Get<ParamProjectorType>().type;
		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMissingProjectorCode(FProjectorGenerationResult& result,
		const void* errorContext)
	{
		// Log an error message
		m_pErrorLog->GetPrivate()->Add("Required projector connection not found.",
			ELMT_ERROR, errorContext);

		FProjector p;
		p.type = PROJECTOR_TYPE::PLANAR;

		p.direction[0] = 1;
		p.direction[1] = 0;
		p.direction[2] = 0;

		p.up[0] = 0;
		p.up[1] = 0;
		p.up[2] = 1;

		p.position[0] = 0.0f;
		p.position[1] = 0.0f;
		p.position[2] = 0.0f;

		p.scale[0] = 1.0f;
		p.scale[1] = 1.0f;

		Ptr<ASTOpConstantProjector> op = new ASTOpConstantProjector();
		op->value = p;

		result.op = op;
		result.type = p.type;
	}

}
