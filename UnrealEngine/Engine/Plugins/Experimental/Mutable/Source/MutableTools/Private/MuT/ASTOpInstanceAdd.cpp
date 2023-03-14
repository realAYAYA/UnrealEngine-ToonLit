// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpInstanceAdd.h"

#include "Misc/AssertionMacros.h"
#include "MuR/ModelPrivate.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/CodeOptimiser.h"
#include "MuT/StreamsPrivate.h"

#include <algorithm>
#include <memory>
#include <utility>


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	ASTOpInstanceAdd::ASTOpInstanceAdd()
		: instance(this)
		, value(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpInstanceAdd::~ASTOpInstanceAdd()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpInstanceAdd::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpInstanceAdd*>(&otherUntyped))
		{
			return type == other->type &&
				instance == other->instance &&
				value == other->value &&
				id == other->id &&
				externalId == other->externalId &&
				name == other->name;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpInstanceAdd::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpInstanceAdd> n = new ASTOpInstanceAdd();
		n->type = type;
		n->instance = mapChild(instance.child());
		n->value = mapChild(value.child());
		n->id = id;
		n->externalId = externalId;
		n->name = name;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpInstanceAdd::Hash() const
	{
		uint64 res = std::hash<size_t>()(size_t(type));
		hash_combine(res, instance.child().get());
		hash_combine(res, value.child().get());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpInstanceAdd::Assert()
	{
		switch (type)
		{
		case OP_TYPE::IN_ADDMESH:
		case OP_TYPE::IN_ADDIMAGE:
		case OP_TYPE::IN_ADDVECTOR:
		case OP_TYPE::IN_ADDSCALAR:
		case OP_TYPE::IN_ADDSTRING:
		case OP_TYPE::IN_ADDSURFACE:
		case OP_TYPE::IN_ADDCOMPONENT:
		case OP_TYPE::IN_ADDLOD:
			break;
		default:
			// Unexpected type
			check(false);
			break;
		}

		ASTOp::Assert();
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpInstanceAdd::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(instance);
		f(value);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpInstanceAdd::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::InstanceAddArgs args;
			memset(&args, 0, sizeof(args));
			args.id = id;
			args.externalId = externalId;

			args.name = program.AddConstant(name);

			if (instance) args.instance = instance->linkedAddress;
			if (value) args.value = value->linkedAddress;

			if (type == OP_TYPE::IN_ADDIMAGE ||
				type == OP_TYPE::IN_ADDMESH)
			{
				// Find out relevant parameters. \todo: this may be optimised by reusing partial
				// values in a LINK_CONTEXT or similar
				SubtreeRelevantParametersVisitorAST visitor;
				visitor.Run(value.child());

				TArray<uint16> params;
				for (const string& paramName : visitor.m_params)
				{
					for (int32 i = 0; i < program.m_parameters.Num(); ++i)
					{
						const auto& param = program.m_parameters[i];
						if (param.m_name == paramName)
						{
							params.Add(uint16(i));
							break;
						}
					}
				}

				params.Sort();

				auto it = program.m_parameterLists.Find(params);

				if (it != INDEX_NONE)
				{
					args.relevantParametersListIndex = it;
				}
				else
				{
					args.relevantParametersListIndex = uint32_t(program.m_parameterLists.Num());
					program.m_parameterLists.Add(params);
				}
			}

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, type);
			AppendCode(program.m_byteCode, args);
		}

	}

}
