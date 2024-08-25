// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/CodeGenerator_FirstPass.h"

#include "MuT/CodeGenerator.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentEditPrivate.h"
#include "MuT/NodeComponentNewPrivate.h"
#include "MuT/NodeLODPrivate.h"
#include "MuT/NodeModifierMeshClipDeformPrivate.h"
#include "MuT/NodeModifierMeshClipMorphPlanePrivate.h"
#include "MuT/NodeModifierMeshClipWithMeshPrivate.h"
#include "MuT/NodeModifierMeshClipWithUVMaskPrivate.h"
#include "MuT/NodeObject.h"
#include "MuT/NodeObjectGroupPrivate.h"
#include "MuT/NodeObjectNewPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeSurfaceEditPrivate.h"
#include "MuT/NodeSurfaceNewPrivate.h"
#include "MuT/NodeSurfaceVariationPrivate.h"
#include "MuT/NodeSurfaceSwitchPrivate.h"
#include "MuT/NodeScalarEnumParameterPrivate.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	FirstPassGenerator::FirstPassGenerator()
	{
		// Default conditions when there is no restriction accumulated.
		FConditionContext noCondition;
        m_currentCondition.Add(noCondition);
        m_currentStateCondition.Add(StateCondition());
	}


	//---------------------------------------------------------------------------------------------
    void FirstPassGenerator::Generate( Ptr<ErrorLog> InErrorLog,
                                       const Node* Root,
                                       bool bIgnoreStates,
									   CodeGenerator* InGenerator )
	{
		MUTABLE_CPUPROFILER_SCOPE(FirstPassGenerate);

		Generator = InGenerator;
		m_pErrorLog = InErrorLog;
        m_ignoreStates = bIgnoreStates;

		// Step 1: collect all objects, surfaces and object conditions
        if (Root)
		{
 			Generate_Generic(Root);
 		}

		// Step 2: Collect all tags and a list of the surfaces that activate them
		for (int32 s=0; s<surfaces.Num(); ++s)
		{
            // \todo: edit surfaces should also be able to activate tags. T1245
            for (int32 t=0; t<surfaces[s].node->GetPrivate()->m_tags.Num(); ++t)
			{
				int tag = -1;
                const FString& tagStr = surfaces[s].node->GetPrivate()->m_tags[t];
                for (std::size_t i = 0; i<m_tags.Num() && tag<0; ++i)
				{
                    if (m_tags[i].tag == tagStr)
					{
						tag = (int)i;
					}
				}

				// New tag?
				if (tag < 0)
				{
                    tag = (int)m_tags.Num();
					FTag newTag;
                    newTag.tag = tagStr;
                    m_tags.Add(newTag);
				}

                if (m_tags[tag].surfaces.Find(s)==INDEX_NONE)
				{
                    m_tags[tag].surfaces.Add((int)s);
				}
			}

            // Collect the tags in edit surfaces
            for (std::size_t e=0; e<surfaces[s].edits.Num(); ++e)
            {
                const auto& edit = surfaces[s].edits[e];
                for (int32 t=0; t<edit.node->m_tags.Num(); ++t)
                {
                    int tag = -1;
                    auto tagStr = edit.node->m_tags[t];

                    for (int32 i = 0; i<m_tags.Num() && tag<0; ++i)
                    {
                        if (m_tags[i].tag == tagStr)
                        {
                            tag = (int)i;
                        }
                    }

                    // New tag?
                    if (tag < 0)
                    {
                        tag = (int)m_tags.Num();
						FTag newTag;
                        newTag.tag = tagStr;
                        m_tags.Add(newTag);
                    }

                    if (m_tags[tag].edits.Find({int(s),int(e)}) == INDEX_NONE)
                    {
                        m_tags[tag].edits.Add({ int(s),int(e) });
                    }
                }
            }

		}

        // Step 3: Create default state if necessary
        if ( bIgnoreStates )
        {
            m_states.Empty();
        }

        if ( m_states.IsEmpty() )
        {
            FObjectState data;
            data.m_name = "Default";
            m_states.Emplace( data, Root->GetBasePrivate() );
        }
	}


	void FirstPassGenerator::Generate_Generic(const Node* Root)
	{
		if (Root->GetType()==NodeSurfaceNew::GetStaticType())
		{
			Generate_SurfaceNew(static_cast<const NodeSurfaceNew*>(Root));
		}
		else if (Root->GetType() == NodeSurfaceEdit::GetStaticType())
		{
			Generate_SurfaceEdit(static_cast<const NodeSurfaceEdit*>(Root));
		}
		else if (Root->GetType() == NodeSurfaceVariation::GetStaticType())
		{
			Generate_SurfaceVariation(static_cast<const NodeSurfaceVariation*>(Root));
		}
		else if (Root->GetType() == NodeSurfaceSwitch::GetStaticType())
		{
			Generate_SurfaceSwitch(static_cast<const NodeSurfaceSwitch*>(Root));
		}
		else if (Root->GetType() == NodeComponentNew::GetStaticType())
		{
			Generate_ComponentNew(static_cast<const NodeComponentNew*>(Root));
		}
		else if (Root->GetType() == NodeComponentEdit::GetStaticType())
		{
			Generate_ComponentEdit(static_cast<const NodeComponentEdit*>(Root));
		}
		else if (Root->GetType() == NodeObjectNew::GetStaticType())
		{
			Generate_ObjectNew(static_cast<const NodeObjectNew*>(Root));
		}
		else if (Root->GetType() == NodeObjectGroup::GetStaticType())
		{
			Generate_ObjectGroup(static_cast<const NodeObjectGroup*>(Root));
		}
		else if (Root->GetType() == NodeLOD::GetStaticType())
		{
			Generate_LOD(static_cast<const NodeLOD*>(Root));
		}
		else if (Root->GetType() == NodeModifier::GetStaticType())
		{
			Generate_Modifier(static_cast<const NodeModifier*>(Root));
		}
		else
		{
			check(false);
		}
	}

	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_Modifier(const NodeModifier* InNode)
	{
		// Add the data about this modifier
		FModifier thisData;
		thisData.node = static_cast<const NodeModifier::Private*>(InNode->GetBasePrivate());
		thisData.objectCondition = m_currentCondition.Last().objectCondition;
		thisData.stateCondition = m_currentStateCondition.Last();
		thisData.lod = m_currentLOD;
		thisData.positiveTags = m_currentPositiveTags;
		thisData.negativeTags = m_currentNegativeTags;
		modifiers.Add(thisData);
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceNew(const NodeSurfaceNew* InNode)
	{
		// Add the data about this surface
		FSurface thisData;
		thisData.node = InNode;
		thisData.component = m_currentComponent;
		thisData.objectCondition = m_currentCondition.Last().objectCondition;
		thisData.stateCondition = m_currentStateCondition.Last();
		thisData.positiveTags = m_currentPositiveTags;
		thisData.negativeTags = m_currentNegativeTags;
		surfaces.Add(thisData);
	}


	//---------------------------------------------------------------------------------------------
    void FirstPassGenerator::Generate_SurfaceEdit(const NodeSurfaceEdit* InNode)
	{
		const NodeSurfaceEdit::Private* Private = InNode->GetPrivate();

		// Store a reference to this node in the surface data for the surface that this node is
		// editing.
		FSurface* Surface = surfaces.FindByPredicate([&Private](const FSurface& s)
        {
            // Are we editing the main surface node of this surface?
            if (s.node.get() == Private->m_pParent.get()) return true;

            // Are we editing an edit node modifying this surface?
            for (const auto& e: s.edits)
            {
                if (Private->m_pParent && e.node==Private->m_pParent->GetBasePrivate())
                {
                    return true;
                }
            }

            return false;
        });
		
		// The surface could be missing if the parent is not in the hierarchy. This could happen
		// with wrong input or in case of partial models for preview.
		if (Surface)
		{
			FSurface::FEdit edit;
			edit.PositiveTags = m_currentPositiveTags;
			edit.NegativeTags = m_currentNegativeTags;
            edit.node = Private;
            edit.condition = m_currentCondition.Last().objectCondition;
			Surface->edits.Add(edit);
		}
		else
		{
			m_pErrorLog->GetPrivate()->Add("Missing parent object for edit node.", ELMT_WARNING, Private->m_errorContext);
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceVariation(const NodeSurfaceVariation* InNode)
	{
		const NodeSurfaceVariation::Private* Private = InNode->GetPrivate();

        switch(Private->m_type)
        {

        case NodeSurfaceVariation::VariationType::Tag:
        {
            // Any of the tags in the variations would prevent the default surface
            auto oldNegativeTags = m_currentNegativeTags;
            for (int32 v=0; v< Private->m_variations.Num(); ++v)
            {
                m_currentNegativeTags.Add(Private->m_variations[v].m_tag);
            }

            for(const auto& n: Private->m_defaultSurfaces)
            {
				Generate_Generic(n.get());
			}
            for(const auto& n: Private->m_defaultModifiers)
            {
				Generate_Modifier(n.get());
            }

            m_currentNegativeTags = oldNegativeTags;

            for (int32 v=0; v< Private->m_variations.Num(); ++v)
            {
                m_currentPositiveTags.Add(Private->m_variations[v].m_tag);
                for (const auto& s : Private->m_variations[v].m_surfaces)
                {
					Generate_Generic(s.get());
                }

                for (const auto& s : Private->m_variations[v].m_modifiers)
                {
					Generate_Modifier(s.get());
				}

                m_currentPositiveTags.Pop();

                // Tags have an order in a variation node: the current tag should prevent any following
                // variation surface
                m_currentNegativeTags.Add(Private->m_variations[v].m_tag);
            }

            m_currentNegativeTags = oldNegativeTags;

            break;
        }


        case NodeSurfaceVariation::VariationType::State:
        {
            size_t stateCount = m_states.Num();

            // Default
            {
                // Store the states for the default branch here
                StateCondition defaultStates;
                {
					StateCondition AllTrue;
					AllTrue.Init(true,stateCount);
                    defaultStates = m_currentStateCondition.Last().IsEmpty()
                            ? AllTrue
                            : m_currentStateCondition.Last();

                    for (const auto& v: Private->m_variations)
                    {
                        for( size_t s=0; s<stateCount; ++s )
                        {
                            if (m_states[s].Key.m_name==v.m_tag)
                            {
                                // Remove this state from the default options, since it has its own variation
                                defaultStates[s] = false;
                            }
                        }
                    }
                }

                m_currentStateCondition.Add(defaultStates);

                for (const auto& n : Private->m_defaultSurfaces)
                {
					Generate_Generic(n.get());
				}
                for (const auto& n : Private->m_defaultModifiers)
                {
					Generate_Modifier(n.get());
				}

                m_currentStateCondition.Pop();
            }

            // Variation branches
            for (const auto& v: Private->m_variations)
            {
                // Store the states for this variation here
				StateCondition variationStates;
				variationStates.Init(false,stateCount);

                for( size_t s=0; s<stateCount; ++s )
                {
                    if (m_states[s].Key.m_name==v.m_tag)
                    {
                        variationStates[s] = true;
                    }
                }

                m_currentStateCondition.Add(variationStates);

                for (const auto& n : v.m_surfaces)
                {
					Generate_Generic(n.get());
				}
                for (const auto& n : v.m_modifiers)
                {
					Generate_Modifier(n.get());
				}

                m_currentStateCondition.Pop();
            }

            break;
        }

        default:
            // Case not implemented.
            check(false);
            break;
        }
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceSwitch(const NodeSurfaceSwitch* InNode)
	{
		const NodeSurfaceSwitch::Private* Private = InNode->GetPrivate();

		if (Private->Options.Num() == 0)
		{
			// No options in the switch!
			return;
		}

		// Prepare the enumeration parameter
		CodeGenerator::FGenericGenerationOptions Options;
		CodeGenerator::FScalarGenerationResult ScalarResult;
		if (Private->Parameter)
		{
			Generator->GenerateScalar( ScalarResult, Options, Private->Parameter );
		}
		else
		{
			// This argument is required
			ScalarResult.op = Generator->GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, Private->m_errorContext);
		}

		// Parse the options
		for (int32 t = 0; t < Private->Options.Num(); ++t)
		{
			// Create a comparison operation as the boolean parameter for the child
			Ptr<ASTOpFixed> ParamOp = new ASTOpFixed();
			ParamOp->op.type = OP_TYPE::BO_EQUAL_INT_CONST;
			ParamOp->SetChild(ParamOp->op.args.BoolEqualScalarConst.value, ScalarResult.op);
			ParamOp->op.args.BoolEqualScalarConst.constant = (int16)t;

			// Combine the new condition with previous conditions coming from parent objects
			if (m_currentCondition.Last().objectCondition)
			{
				Ptr<ASTOpFixed> op = new ASTOpFixed();
				op->op.type = OP_TYPE::BO_AND;
				op->SetChild(op->op.args.BoolBinary.a, m_currentCondition.Last().objectCondition);
				op->SetChild(op->op.args.BoolBinary.b, ParamOp);
				ParamOp = op;
			}

			FConditionContext data;
			data.objectCondition = ParamOp;
			m_currentCondition.Push(data);

			if (Private->Options[t])
			{
				Generate_Generic(Private->Options[t].get());
			}

			m_currentCondition.Pop();
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentNew(const NodeComponentNew* InNode)
	{
		const NodeComponentNew::Private* Private = InNode->GetPrivate();

        m_currentComponent = Private->GetParentComponentNew();

		for (const Ptr<NodeSurface>& c : Private->m_surfaces)
		{
			if (c)
			{
				Generate_Generic(c.get());
			}
		}

		m_currentComponent = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentEdit(const NodeComponentEdit* InNode)
	{
		const NodeComponentEdit::Private* Private = InNode->GetPrivate();

		m_currentComponent = Private->GetParentComponentNew();

		for (const auto& c : Private->m_surfaces)
		{
			if (c)
			{
				Generate_Generic(c.get());
			}
		}

		m_currentComponent = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_LOD(const NodeLOD* InNode)
	{
		const NodeLOD::Private* Private = InNode->GetPrivate();

		for (const auto& c : Private->m_components)
		{
			if (c)
			{
				Generate_Generic(c.get());
			}
		}
		for (const auto& c : Private->m_modifiers)
		{
			if (c)
			{
				Generate_Modifier(c.get());
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ObjectNew(const NodeObjectNew* InNode)
	{
		const NodeObjectNew::Private* Private = InNode->GetPrivate();

		// Add the data about this object
		FObject thisData;
		thisData.node = Private;
        thisData.condition = m_currentCondition.Last().objectCondition;
		objects.Add(thisData);

        // Accumulate the model states
        for ( const auto& s: Private->m_states )
        {
            m_states.Emplace( s, Private );

            if ( s.m_runtimeParams.Num() > MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE )
            {
                FString Msg = FString::Printf( TEXT("State [%s] has more than %d runtime parameters. Their update may fail."), 
					*s.m_name,
                    MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE);
                m_pErrorLog->GetPrivate()->Add(Msg, ELMT_ERROR, Private->m_errorContext );
            }
        }

		// Process the lods
		int i = 0;
		for (const auto& l : Private->m_lods)
		{
			if (l)
			{
                m_currentLOD = i++;
				Generate_Generic(l.get());
			}
		}

		m_currentLOD = -1;

		// Process the children
		for (const auto& c : Private->m_children)
		{
			if (c)
			{
				Generate_Generic(c.get());
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ObjectGroup(const NodeObjectGroup* InNode)
	{
		const NodeObjectGroup::Private* Private = InNode->GetPrivate();

		// Prepare the enumeration parameter if necessary
        Ptr<ASTOpParameter> enumOp;
        if (Private->m_type==NodeObjectGroup::CS_ALWAYS_ONE ||
			Private->m_type==NodeObjectGroup::CS_ONE_OR_NONE )
        {
            Ptr<ASTOpParameter> op = new ASTOpParameter();
            op->type = OP_TYPE::NU_PARAMETER;

            op->parameter.m_name = Private->Name;
			const TCHAR* CStr = ToCStr(Private->Uid);
			op->parameter.m_uid.ImportTextItem(CStr, 0, nullptr, nullptr);
            op->parameter.m_type = PARAMETER_TYPE::T_INT;
            op->parameter.m_defaultValue.Set<ParamIntType>(Private->DefaultValue);

            if (Private->m_type==NodeObjectGroup::CS_ONE_OR_NONE )
            {
                FParameterDesc::FIntValueDesc nullValue;
                nullValue.m_value = -1;
                nullValue.m_name = "None";
                op->parameter.m_possibleValues.Add( nullValue );
            }

			ParameterNodes.Add(InNode, op);

            enumOp = op;
        }


        // Parse the child objects
		for ( int32 t=0; t< Private->m_children.Num(); ++t )
        {
            if ( mu::Ptr<const NodeObject> pChildNode = Private->m_children[t] )
            {
                // Overwrite the implicit condition
                Ptr<ASTOp> paramOp = 0;
                switch (Private->m_type )
                {
                    case NodeObjectGroup::CS_TOGGLE_EACH:
                    {
                        // Create a new boolean parameter
                        Ptr<ASTOpParameter> op = new ASTOpParameter();
                        op->type = OP_TYPE::BO_PARAMETER;

                        op->parameter.m_name = pChildNode->GetName();
                   		const TCHAR* CStr = ToCStr(pChildNode->GetUid());
                   		op->parameter.m_uid.ImportTextItem(CStr, 0, nullptr, nullptr);
                        op->parameter.m_type = PARAMETER_TYPE::T_BOOL;
                        op->parameter.m_defaultValue.Set<ParamBoolType>(false);

						ParameterNodes.Add(pChildNode, op);

                        paramOp = op;
                        break;
                    }

                    case NodeObjectGroup::CS_ALWAYS_ALL:
                    {
                        // Create a constant true boolean that the optimiser will remove later.
                        Ptr<ASTOpConstantBool> op = new ASTOpConstantBool();
                        op->value = true;

                        paramOp = op;
                        break;
                    }

                    case NodeObjectGroup::CS_ONE_OR_NONE:
                    case NodeObjectGroup::CS_ALWAYS_ONE:
                    {
                        // Add the option to the enumeration parameter
                        FParameterDesc::FIntValueDesc value;
                        value.m_value = (int16)t;
                        value.m_name = pChildNode->GetName();
                        enumOp->parameter.m_possibleValues.Add( value );

                        check(enumOp);

                        // Create a comparison operation as the boolean parameter for the child
                        Ptr<ASTOpFixed> op = new ASTOpFixed();
                        op->op.type = OP_TYPE::BO_EQUAL_INT_CONST;
                        op->SetChild( op->op.args.BoolEqualScalarConst.value, enumOp);
                        op->op.args.BoolEqualScalarConst.constant = (int16_t)t;

                        paramOp = op;
                        break;
                    }

                    default:
                        check( false );
                }

                // Combine the new condition with previous conditions coming from parent objects
                if (m_currentCondition.Last().objectCondition)
                {
                    Ptr<ASTOpFixed> op = new ASTOpFixed();
                    op->op.type = OP_TYPE::BO_AND;
                    op->SetChild( op->op.args.BoolBinary.a,m_currentCondition.Last().objectCondition);
                    op->SetChild( op->op.args.BoolBinary.b,paramOp);
                    paramOp = op;
                }

				FConditionContext data;
                data.objectCondition = paramOp;
                m_currentCondition.Add( data );

				Generate_Generic(pChildNode.get());

                m_currentCondition.Pop();
            }
        }
 	}

}

