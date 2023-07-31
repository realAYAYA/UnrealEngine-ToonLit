// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/CodeGenerator_FirstPass.h"

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
#include "MuT/NodeObject.h"
#include "MuT/NodeObjectGroupPrivate.h"
#include "MuT/NodeObjectNewPrivate.h"
#include "MuT/NodeObjectStatePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeSurfaceEditPrivate.h"
#include "MuT/NodeSurfaceNewPrivate.h"
#include "MuT/NodeSurfaceVariationPrivate.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	FirstPassGenerator::FirstPassGenerator()
	{
		// Default conditions when there is no restriction accumulated.
		CONDITION_CONTEXT noCondition;
        m_currentCondition.Add(noCondition);
        m_currentStateCondition.Add(StateCondition());
	}


	//---------------------------------------------------------------------------------------------
    void FirstPassGenerator::Generate( ErrorLogPtr pErrorLog,
                                       const Node::Private* root,
                                       bool ignoreStates )
	{
		MUTABLE_CPUPROFILER_SCOPE(FirstPassGenerate);

		m_pErrorLog = pErrorLog;
        m_ignoreStates = ignoreStates;

		// Step 1: collect all objects, surfaces and object conditions
        if ( root )
		{
            bool isFirstPassNode =
                    dynamic_cast<const NodeSurface*>(root->m_pNode)
                    ||
                    dynamic_cast<const NodeComponent*>(root->m_pNode)
                    ||
                    dynamic_cast<const NodeObject*>(root->m_pNode)
                    ||
                    dynamic_cast<const NodeLOD*>(root->m_pNode)
                    ||
                    dynamic_cast<const NodePatchMesh*>(root->m_pNode)
                    ||
                    dynamic_cast<const NodeModifier*>(root->m_pNode);

            if (isFirstPassNode)
            {
                root->Accept(*this);
            }
		}

		// Step 2: Collect all tags and a list of the surfaces that activate them
		for (int32 s=0; s<surfaces.Num(); ++s)
		{
            // \todo: edit surfaces should also be able to activate tags. T1245
            for (int32 t=0; t<surfaces[s].node->GetPrivate()->m_tags.Num(); ++t)
			{
				int tag = -1;
                auto tagStr = surfaces[s].node->GetPrivate()->m_tags[t];
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
					TAG newTag;
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
                        TAG newTag;
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
        if ( ignoreStates )
        {
            m_states.Empty();
        }

        if ( m_states.IsEmpty() )
        {
            OBJECT_STATE data;
            data.m_name = "Default";
            m_states.Emplace( data, root );
        }
	}


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeModifierMeshClipMorphPlane::Private& node)
	{
        // Add the data about this modifier
        MODIFIER thisData;
        thisData.node = &node;
        thisData.objectCondition = m_currentCondition.Last().objectCondition;
        thisData.stateCondition = m_currentStateCondition.Last();
        thisData.lod = m_currentLOD;
        thisData.positiveTags = m_currentPositiveTags;
        thisData.negativeTags = m_currentNegativeTags;
        modifiers.Add(thisData);

        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeModifierMeshClipWithMesh::Private& node)
	{
        // Add the data about this modifier
		MODIFIER thisData;
		thisData.node = &node;
        thisData.objectCondition = m_currentCondition.Last().objectCondition;
        thisData.stateCondition = m_currentStateCondition.Last();
        thisData.lod = m_currentLOD;
        thisData.positiveTags = m_currentPositiveTags;
        thisData.negativeTags = m_currentNegativeTags;
        modifiers.Add(thisData);

        return nullptr;
	}

	
	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeModifierMeshClipDeform::Private& node)
	{
        // Add the data about this modifier
		MODIFIER thisData;
		thisData.node = &node;
        thisData.objectCondition = m_currentCondition.Last().objectCondition;
        thisData.stateCondition = m_currentStateCondition.Last();
        thisData.lod = m_currentLOD;
        thisData.positiveTags = m_currentPositiveTags;
        thisData.negativeTags = m_currentNegativeTags;
        modifiers.Add(thisData);

        return nullptr;
	}

	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeSurfaceNew::Private& node)
	{
		// Add the data about this surface
		SURFACE thisData;
		thisData.node = dynamic_cast<const NodeSurfaceNew*>(node.m_pNode);
		thisData.component = m_currentComponent;
        thisData.objectCondition = m_currentCondition.Last().objectCondition;
        thisData.stateCondition = m_currentStateCondition.Last();
        thisData.positiveTags = m_currentPositiveTags;
        thisData.negativeTags = m_currentNegativeTags;
        surfaces.Add(thisData);

        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeSurfaceEdit::Private& node)
	{
		// Store a reference to this node in the surface data for the surface that this node is
		// editing.
		auto its = surfaces.FindByPredicate([&node](const FirstPassGenerator::SURFACE& s)
        {
            // Are we editing the main surface node of this surface?
            if (s.node.get() == node.m_pParent.get()) return true;

            // Are we editing an edit node modifying this surface?
            for (const auto& e: s.edits)
            {
                if (node.m_pParent && e.node==node.m_pParent->GetBasePrivate())
                {
                    return true;
                }
            }

            return false;
        });
		
		// The surface could be missing if the parent is not in the hierarchy. This could happen
		// with wrong input or in case of partial models for preview.
		if (its)
		{
			SURFACE& surface = *its;

            SURFACE::EDIT edit;
            edit.node = &node;
            edit.condition = m_currentCondition.Last().objectCondition;
            surface.edits.Add(edit);
		}
		else
		{
			m_pErrorLog->GetPrivate()->Add("Missing parent object for edit node.",
				ELMT_WARNING, node.m_errorContext);
		}

        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeSurfaceVariation::Private& node)
	{
        switch(node.m_type)
        {

        case NodeSurfaceVariation::VariationType::Tag:
        {
            // Any of the tags in the variations would prevent the default surface
            auto oldNegativeTags = m_currentNegativeTags;
            for (int32 v=0; v<node.m_variations.Num(); ++v)
            {
                m_currentNegativeTags.Add(node.m_variations[v].m_tag);
            }

            for(const auto& n:node.m_defaultSurfaces)
            {
                n->GetBasePrivate()->Accept(*this);
            }
            for(const auto& n:node.m_defaultModifiers)
            {
                n->GetBasePrivate()->Accept(*this);
            }

            m_currentNegativeTags = oldNegativeTags;

            for (int32 v=0; v<node.m_variations.Num(); ++v)
            {
                m_currentPositiveTags.Add(node.m_variations[v].m_tag);
                for (const auto& s : node.m_variations[v].m_surfaces)
                {
                    s->GetBasePrivate()->Accept(*this);
                }

                for (const auto& s : node.m_variations[v].m_modifiers)
                {
                    s->GetBasePrivate()->Accept(*this);
                }

                m_currentPositiveTags.Pop();

                // Tags have an order in a variation node: the current tag should prevent any following
                // variation surface
                m_currentNegativeTags.Add(node.m_variations[v].m_tag);
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

                    for (const auto& v:node.m_variations)
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

                for (const auto& n : node.m_defaultSurfaces)
                {
                    n->GetBasePrivate()->Accept(*this);
                }
                for (const auto& n : node.m_defaultModifiers)
                {
                    n->GetBasePrivate()->Accept(*this);
                }

                m_currentStateCondition.Pop();
            }


            // Variation branches
            for (const auto& v:node.m_variations)
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
                    n->GetBasePrivate()->Accept(*this);
                }
                for (const auto& n : v.m_modifiers)
                {
                    n->GetBasePrivate()->Accept(*this);
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

        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeComponentNew::Private& node)
	{
        m_currentComponent = node.GetParentComponentNew();

		for (const auto& c : node.m_surfaces)
		{
			if (c)
			{
				c->GetBasePrivate()->Accept(*this);
			}
		}

		m_currentComponent = nullptr;

        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeComponentEdit::Private& node)
	{
		m_currentComponent = node.GetParentComponentNew();

		for (const auto& c : node.m_surfaces)
		{
			if (c)
			{
                c->GetBasePrivate()->Accept(*this);
			}
		}

		m_currentComponent = nullptr;

        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeLOD::Private& node)
	{
		for (const auto& c : node.m_components)
		{
			if (c)
			{
				c->GetBasePrivate()->Accept(*this);
			}
		}
		for (const auto& c : node.m_modifiers)
		{
			if (c)
			{
				c->GetBasePrivate()->Accept(*this);
			}
		}

        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeObjectNew::Private& node)
	{
		// Add the data about this object
		OBJECT thisData;
		thisData.node = &node;
        thisData.condition = m_currentCondition.Last().objectCondition;
		objects.Add(thisData);

        // Accumulate the model states
        for ( const auto& s: node.m_states )
        {
            m_states.Emplace( s, &node );

            // \todo move this 32 constant to a macro or constexpr with a meaningful name and location
            if ( s.m_runtimeParams.Num() > 32 )
            {
                char temp[256];
                mutable_snprintf( temp, 256,
                                  "State [%s] has more than 32 runtime parameters. Their update may fail.",
                                  s.m_name.c_str() );
                m_pErrorLog->GetPrivate()->Add( temp, ELMT_ERROR, node.m_errorContext );
            }
        }

		// Process the lods
		int i = 0;
		for (const auto& l : node.m_lods)
		{
			if (l)
			{
                m_currentLOD = i++;
				l->GetBasePrivate()->Accept(*this);
			}
		}

		m_currentLOD = -1;

		// Process the children
		for (const auto& c : node.m_children)
		{
			if (c)
			{
				c->GetBasePrivate()->Accept(*this);
			}
		}

        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodeObjectGroup::Private& node)
	{
       // Prepare the enumeration parameter if necessary
        Ptr<ASTOpParameter> enumOp;
        if ( node.m_type==NodeObjectGroup::CS_ALWAYS_ONE ||
             node.m_type==NodeObjectGroup::CS_ONE_OR_NONE )
        {
            Ptr<ASTOpParameter> op = new ASTOpParameter();
            op->type = OP_TYPE::NU_PARAMETER;

            op->parameter.m_name = node.m_name;
            op->parameter.m_uid = node.m_uid;
            op->parameter.m_type = PARAMETER_TYPE::T_INT;
            op->parameter.m_defaultValue.m_int = -1;

            if ( node.m_type==NodeObjectGroup::CS_ONE_OR_NONE )
            {
                PARAMETER_DESC::INT_VALUE_DESC nullValue;
                nullValue.m_value = -1;
                nullValue.m_name = "None";
                op->parameter.m_possibleValues.Add( nullValue );
                op->parameter.m_defaultValue.m_int = nullValue.m_value;
            }

            enumOp = op;
        }


        // Parse the child objects
		for ( int32 t=0; t<node.m_children.Num(); ++t )
        {
            if ( const NodeObject* pChildNode = node.m_children[t].get() )
            {
                // Overwrite the implicit condition
                Ptr<ASTOp> paramOp = 0;
                switch ( node.m_type )
                {
                    case NodeObjectGroup::CS_TOGGLE_EACH:
                    {
                        // Create a new boolean parameter
                        Ptr<ASTOpParameter> op = new ASTOpParameter();
                        op->type = OP_TYPE::BO_PARAMETER;

                        op->parameter.m_name = pChildNode->GetName();
                        op->parameter.m_uid = pChildNode->GetUid();
                        op->parameter.m_type = PARAMETER_TYPE::T_BOOL;
                        op->parameter.m_defaultValue.m_bool = false;

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
                        PARAMETER_DESC::INT_VALUE_DESC value;
                        value.m_value = (int16_t)t;
                        value.m_name = pChildNode->GetName();
                        enumOp->parameter.m_possibleValues.Add( value );

                        // Set as default if none has been set.
                        if ( enumOp->parameter.m_defaultValue.m_int == -1 )
                        {
                            enumOp->parameter.m_defaultValue.m_int = (int)t;
                        }

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

				CONDITION_CONTEXT data;
                data.objectCondition = paramOp;
                m_currentCondition.Add( data );

				pChildNode->GetBasePrivate()->Accept(*this);

                m_currentCondition.Pop();
            }
        }

        return nullptr;
 	}


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit( const NodeObjectState::Private& node )
    {
        // Generate the source object
        Ptr<ASTOp> source;

        if (node.m_pSource)
        {
            // Process the source
            node.m_pSource->GetBasePrivate()->Accept(*this);

            if (node.m_pRoot)
            {
                // Remember the new state with the new root.
                m_states.Emplace( node.m_state, node.m_pRoot->GetBasePrivate() );
            }
        }

        return source;
    }


	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> FirstPassGenerator::Visit(const NodePatchMesh::Private&)
	{
        return nullptr;
	}

}

