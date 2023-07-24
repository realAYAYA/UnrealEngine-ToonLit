// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/CodeRunner.h"

#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuR/Instance.h"
#include "MuR/InstancePrivate.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableString.h"
#include "MuR/MutableTrace.h"
#include "MuR/OpEvalCurve.h"
#include "MuR/OpImageApplyComposite.h"
#include "MuR/OpImageBinarise.h"
#include "MuR/OpImageBlend.h"
#include "MuR/OpImageColourMap.h"
#include "MuR/OpImageCompose.h"
#include "MuR/OpImageCrop.h"
#include "MuR/OpImageDifference.h"
#include "MuR/OpImageDisplace.h"
#include "MuR/OpImageGradient.h"
#include "MuR/OpImageInterpolate.h"
#include "MuR/OpImageInvert.h"
#include "MuR/OpImageLuminance.h"
#include "MuR/OpImageMipmap.h"
#include "MuR/OpImageNormalCombine.h"
#include "MuR/OpImagePixelFormat.h"
#include "MuR/OpImageProject.h"
#include "MuR/OpImageRasterMesh.h"
#include "MuR/OpImageResize.h"
#include "MuR/OpImageSaturate.h"
#include "MuR/OpImageSelectColour.h"
#include "MuR/OpImageTransform.h"
#include "MuR/OpLayoutPack.h"
#include "MuR/OpLayoutRemoveBlocks.h"
#include "MuR/OpMeshApplyLayout.h"
#include "MuR/OpMeshApplyPose.h"
#include "MuR/OpMeshBind.h"
#include "MuR/OpMeshClipDeform.h"
#include "MuR/OpMeshClipMorphPlane.h"
#include "MuR/OpMeshClipWithMesh.h"
#include "MuR/OpMeshDifference.h"
#include "MuR/OpMeshExtractLayoutBlock.h"
#include "MuR/OpMeshFormat.h"
#include "MuR/OpMeshGeometryOperation.h"
#include "MuR/OpMeshMerge.h"
#include "MuR/OpMeshMorph.h"
#include "MuR/OpMeshRemapIndices.h"
#include "MuR/OpMeshRemoveChart.h"
#include "MuR/OpMeshReshape.h"
#include "MuR/OpMeshSubtract.h"
#include "MuR/OpMeshTransform.h"
#include "MuR/OpMeshOptimizeSkinning.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/PhysicsBody.h"
#include "MuR/Platform.h"
#include "MuR/SettingsPrivate.h"
#include "MuR/Skeleton.h"
#include "MuR/SystemPrivate.h"
#include "Templates/Tuple.h"
#include "Trace/Detail/Channel.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    CodeRunner::CodeRunner(const SettingsPtrConst& pSettings,
		class System::Private* s,
		const TSharedPtr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at,
		uint32 lodMask, uint8 executionOptions, FScheduledOp::EType Type )
		: m_pSettings(pSettings), m_pSystem(s), m_pModel(pModel), m_pParams(pParams), m_lodMask(lodMask)
	{
		ScheduledStagePerOp.resize(m_pModel->GetPrivate()->m_program.m_opAddress.Num());

		// We will read this in the end, so make sure we keep it.
		m_pSystem->m_memory->IncreaseHitCount(FCacheAddress(at, 0, executionOptions));

		FProgram& program = m_pModel->GetPrivate()->m_program;
		m_romPendingOps.SetNum(program.m_roms.Num());

		// Push the root operation
		FScheduledOp rootOp;
		rootOp.At = at;
		rootOp.ExecutionOptions = executionOptions;
		rootOp.Type = Type;
		AddOp(rootOp);
	}


    //---------------------------------------------------------------------------------------------
	FProgramCache& CodeRunner::GetMemory()
    {
		return *m_pSystem->m_memory;
	}


    //---------------------------------------------------------------------------------------------
	TTuple<FGraphEventRef, TFunction<void()>> CodeRunner::LoadExternalImageAsync(EXTERNAL_IMAGE_ID Id, uint8 MipmapsToSkip, TFunction<void(Ptr<Image>)>& ResultCallback)
    {
		MUTABLE_CPUPROFILER_SCOPE(LoadExternalImageAsync);

		check(m_pSystem);

		if (m_pSystem->m_pImageParameterGenerator)
		{
			return m_pSystem->m_pImageParameterGenerator->GetImageAsync(Id, MipmapsToSkip, ResultCallback);

			// Don't cache for now. Need to figure out how to invalidate them.
			// \TODO: Like constants? attached to a cache level?
			//m_pSystem->m_externalImages.push_back( pair<EXTERNAL_IMAGE_ID,ImagePtr>(id,pResult) );
		}
		else
		{
			// Not found and there is no generator!
			check(false);
		}

		// Not needed as it should never reach this point, but added for correctness.
		FGraphEventRef CompletionEvent = FGraphEvent::CreateGraphEvent();
		CompletionEvent->DispatchSubsequents();

		return MakeTuple(CompletionEvent, []() -> void {});
	}

	
    //---------------------------------------------------------------------------------------------
	FImageDesc CodeRunner::GetExternalImageDesc(EXTERNAL_IMAGE_ID Id, uint8 MipmapsToSkip)
	{
		MUTABLE_CPUPROFILER_SCOPE(GetExternalImageDesc);

		check(m_pSystem);

		if (m_pSystem->m_pImageParameterGenerator)
		{
			return m_pSystem->m_pImageParameterGenerator->GetImageDesc(Id, MipmapsToSkip);
		}
		else
		{
			// Not found and there is no generator!
			check(false);
		}

		return FImageDesc();
	}

	
    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Conditional( FScheduledOp& item,
                                          const Model* pModel
                                          )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Conditional);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
		OP::ConditionalArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ConditionalArgs>(item.At);

        // Conditionals have the following execution stages:
        // 0: we need to run the condition
        // 1: we need to run the branch
        // 2: we need to fetch the result and store it in this op
        switch( item.Stage )
        {
        case 0:
        {
            AddOp( FScheduledOp( item.At,item,1 ),
                   FScheduledOp( args.condition, item ) );
            break;
        }

        case 1:
        {
            // Get the condition result

            // If there is no expression, we'll assume true.
            bool value = true;
            value = GetMemory().GetBool( FCacheAddress(args.condition, item.ExecutionIndex, item.ExecutionOptions) );

            OP::ADDRESS resultAt = value ? args.yes : args.no;

            // Schedule the end of this instruction if necessary
            AddOp( FScheduledOp( item.At, item, 2, (uint32)value),
				FScheduledOp( resultAt, item) );

            break;
        }

        case 2:
        {
            OP::ADDRESS resultAt = item.CustomState ? args.yes : args.no;

            // Store the final result
            FCacheAddress cat( item );
            FCacheAddress rat( resultAt, item );
            switch (GetOpDataType(type))
            {
            case DT_BOOL:       GetMemory().SetBool( cat, GetMemory().GetBool(rat) ); break;
            case DT_INT:        GetMemory().SetInt( cat, GetMemory().GetInt(rat) ); break;
            case DT_SCALAR:     GetMemory().SetScalar( cat, GetMemory().GetScalar(rat) ); break;
			case DT_STRING:		GetMemory().SetString( cat, GetMemory().GetString( rat ) ); break;
            case DT_COLOUR:		GetMemory().SetColour( cat, GetMemory().GetColour( rat ) ); break;
            case DT_PROJECTOR:  GetMemory().SetProjector( cat, GetMemory().GetProjector(rat) ); break;
            case DT_MESH:       GetMemory().SetMesh( cat, GetMemory().GetMesh(rat) ); break;
            case DT_IMAGE:      GetMemory().SetImage( cat, GetMemory().GetImage(rat) ); break;
            case DT_LAYOUT:     GetMemory().SetLayout( cat, GetMemory().GetLayout(rat) ); break;
            case DT_INSTANCE:   GetMemory().SetInstance( cat, GetMemory().GetInstance(rat) ); break;
            default:
                // Not implemented
                check( false );
            }

            break;
        }

        default:
            check(false);
        }
    }


	//---------------------------------------------------------------------------------------------
	void CodeRunner::RunCode_Switch(FScheduledOp& item,
		const Model* pModel
	)
	{
		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);

		const uint8* data = pModel->GetPrivate()->m_program.GetOpArgsPointer(item.At);

		OP::ADDRESS VarAddress;
		FMemory::Memcpy(&VarAddress, data, sizeof(OP::ADDRESS));
		data += sizeof(OP::ADDRESS);

		OP::ADDRESS DefAddress;
		FMemory::Memcpy(&DefAddress, data, sizeof(OP::ADDRESS));
		data += sizeof(OP::ADDRESS);

		uint32 CaseCount;
		FMemory::Memcpy(&CaseCount, data, sizeof(uint32));
		data += sizeof(uint32);

		switch (item.Stage)
		{
		case 0:
		{
			if (VarAddress)
			{
				AddOp(FScheduledOp(item.At, item, 1),
					FScheduledOp(VarAddress, item));
			}
			else
			{
				switch (GetOpDataType(type))
				{
				case DT_BOOL:       GetMemory().SetBool(item, false); break;
				case DT_INT:        GetMemory().SetInt(item, 0); break;
				case DT_SCALAR:		GetMemory().SetScalar(item, 0.0f); break;
				case DT_STRING:		GetMemory().SetString(item, nullptr); break;
				case DT_COLOUR:		GetMemory().SetColour(item, FVector4f()); break;
				case DT_PROJECTOR:  GetMemory().SetProjector(item, nullptr); break;
				case DT_MESH:       GetMemory().SetMesh(item, nullptr); break;
				case DT_IMAGE:      GetMemory().SetImage(item, nullptr); break;
				case DT_LAYOUT:     GetMemory().SetLayout(item, nullptr); break;
				case DT_INSTANCE:   GetMemory().SetInstance(item, nullptr); break;
				default:
					// Not implemented
					check(false);
				}
			}
			break;
		}

		case 1:
		{
			// Get the variable result
			int var = GetMemory().GetInt(FCacheAddress(VarAddress, item));

			OP::ADDRESS valueAt = DefAddress;
			for (uint32 C = 0; C < CaseCount; ++C)
			{
				int32 Condition;
				FMemory::Memcpy(&Condition, data, sizeof(int32));
				data += sizeof(int32);

				OP::ADDRESS At;
				FMemory::Memcpy(&At, data, sizeof(OP::ADDRESS));
				data += sizeof(OP::ADDRESS);

				if (At && var == (int)Condition)
				{
					valueAt = At;
					break; 
				}
			}

            // Schedule the end of this instruction if necessary
            AddOp( FScheduledOp( item.At, item, 2, valueAt ),
				   FScheduledOp( valueAt, item ) );

            break;
        }

        case 2:
        {
			OP::ADDRESS resultAt = OP::ADDRESS(item.CustomState);

            // Store the final result
            FCacheAddress cat( item );
            FCacheAddress rat( resultAt, item );
            switch (GetOpDataType(type))
            {
            case DT_BOOL:       GetMemory().SetBool( cat, GetMemory().GetBool(rat) ); break;
            case DT_INT:        GetMemory().SetInt( cat, GetMemory().GetInt(rat) ); break;
            case DT_SCALAR:     GetMemory().SetScalar( cat, GetMemory().GetScalar(rat) ); break;
            case DT_STRING:		GetMemory().SetString( cat, GetMemory().GetString( rat ) ); break;
            case DT_COLOUR:		GetMemory().SetColour( cat, GetMemory().GetColour( rat ) ); break;
            case DT_PROJECTOR:  GetMemory().SetProjector( cat, GetMemory().GetProjector(rat) ); break;
            case DT_MESH:       GetMemory().SetMesh( cat, GetMemory().GetMesh(rat) ); break;
            case DT_IMAGE:      GetMemory().SetImage( cat, GetMemory().GetImage(rat) ); break;
            case DT_LAYOUT:     GetMemory().SetLayout( cat, GetMemory().GetLayout(rat) ); break;
            case DT_INSTANCE:   GetMemory().SetInstance( cat, GetMemory().GetInstance(rat) ); break;
            default:
                // Not implemented
                check( false );
            }

            break;
        }

        default:
            check(false);
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Instance( FScheduledOp& item,
                                  const Model* pModel,
                                  uint32 lodMask
                                  )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Instance);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::IN_ADDVECTOR:
        {
			OP::InstanceAddArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>(item.At);

            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.instance, item),
                           FScheduledOp( args.value, item) );
                break;

            case 1:
            {
                InstancePtrConst pBase = GetMemory().GetInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = CloneOrTakeOver<Instance>(pBase.get());
                }

                if ( args.value )
                {
					FVector4f value = GetMemory().GetColour( FCacheAddress(args.value,item) );

                    OP::ADDRESS nameAd = args.name;
                    check(  nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName = pModel->GetPrivate()->m_program.m_constantStrings[ nameAd ].c_str();

                    pResult->GetPrivate()->AddVector( 0, 0, 0, value, strName );
                }
                GetMemory().SetInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IN_ADDSCALAR:
        {
			OP::InstanceAddArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.instance, item),
                           FScheduledOp( args.value, item) );
                break;

            case 1:
            {
                InstancePtrConst pBase = GetMemory().GetInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
                    pResult = CloneOrTakeOver<Instance>(pBase.get());
                }

                if ( args.value )
                {
                    float value = GetMemory().GetScalar( FCacheAddress(args.value,item) );

                    OP::ADDRESS nameAd = args.name;
                    check(  nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName = pModel->GetPrivate()->m_program.m_constantStrings[ nameAd ].c_str();

                    pResult->GetPrivate()->AddScalar( 0, 0, 0, value, strName );
                }
                GetMemory().SetInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IN_ADDSTRING:
        {
			OP::InstanceAddArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>( item.At );
            switch ( item.Stage )
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1 ), FScheduledOp( args.instance, item ),
                           FScheduledOp( args.value, item ) );
                break;

            case 1:
            {
                InstancePtrConst pBase =
                    GetMemory().GetInstance( FCacheAddress( args.instance, item ) );
                InstancePtr pResult;
                if ( !pBase )
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = CloneOrTakeOver<Instance>(pBase.get());
				}

                if ( args.value )
                {
                    Ptr<const String> value =
                        GetMemory().GetString( FCacheAddress( args.value, item ) );

                    OP::ADDRESS nameAd = args.name;
                    check( nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName =
                        pModel->GetPrivate()->m_program.m_constantStrings[nameAd].c_str();

                    pResult->GetPrivate()->AddString( 0, 0, 0, value->GetValue(), strName );
                }
                GetMemory().SetInstance( item, pResult );
                break;
            }

            default:
                check( false );
            }

            break;
        }

        case OP_TYPE::IN_ADDCOMPONENT:
        {
			OP::InstanceAddArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.instance, item),
                           FScheduledOp( args.value, item) );
                break;

            case 1:
            {
                InstancePtrConst pBase = GetMemory().GetInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = CloneOrTakeOver<Instance>(pBase.get());
				}

                if ( args.value )
                {
                    InstancePtrConst pComp = GetMemory().GetInstance( FCacheAddress(args.value,item) );

                    int cindex = pResult->GetPrivate()->AddComponent( 0 );

                    if ( !pComp->GetPrivate()->m_lods.IsEmpty()
                         &&
                         !pResult->GetPrivate()->m_lods.IsEmpty()
                         &&
                         !pComp->GetPrivate()->m_lods[0].m_components.IsEmpty() )
                    {
                        pResult->GetPrivate()->m_lods[0].m_components[cindex] =
                                pComp->GetPrivate()->m_lods[0].m_components[0];

                    	pResult->GetPrivate()->m_lods[0].m_components[cindex].m_id = args.id;
                    	
                        // Name
                        OP::ADDRESS nameAd = args.name;
                        check( nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                        const char* strName = pModel->GetPrivate()->m_program.m_constantStrings[ nameAd ].c_str();
                        pResult->GetPrivate()->SetComponentName( 0, cindex, strName );
                    }
                }
                GetMemory().SetInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IN_ADDSURFACE:
        {
			OP::InstanceAddArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.instance, item),
                           FScheduledOp( args.value, item) );
                break;

            case 1:
            {
                InstancePtrConst pBase = GetMemory().GetInstance( FCacheAddress(args.instance,item) );

                InstancePtr pResult;
				if (pBase)
				{
					pResult = CloneOrTakeOver<Instance>(pBase.get());
				}
				else
				{
					pResult = new Instance();
				}

                // Empty surfaces are ok, they still need to be created, because they may contain
                // additional information like internal or external IDs
                //if ( args.value )
                {
                    InstancePtrConst pSurf = GetMemory().GetInstance( FCacheAddress(args.value,item) );

                    int sindex = pResult->GetPrivate()->AddSurface( 0, 0 );

                    // Surface data
                    if (pSurf
                            &&
                            pSurf->GetPrivate()->m_lods.Num()
                            &&
                            pSurf->GetPrivate()->m_lods[0].m_components.Num()
                            &&
                            pSurf->GetPrivate()->m_lods[0].m_components[0].m_surfaces.Num())
                    {
                        pResult->GetPrivate()->m_lods[0].m_components[0].m_surfaces[sindex] =
                            pSurf->GetPrivate()->m_lods[0].m_components[0].m_surfaces[0];

                        // Meshes must be added later.
                        check(!pSurf->GetPrivate()->m_lods[0].m_components[0].m_meshes.Num());
                    }

                    // Name
                    OP::ADDRESS nameAd = args.name;
                    check( nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName = pModel->GetPrivate()->m_program.m_constantStrings[ nameAd ].c_str();
                    pResult->GetPrivate()->SetSurfaceName( 0, 0, sindex, strName );

                    // IDs
                    pResult->GetPrivate()->m_lods[0].m_components[0].m_surfaces[sindex].m_internalID = args.id;
                    pResult->GetPrivate()->m_lods[0].m_components[0].m_surfaces[sindex].m_customID = args.externalId;
                }
                GetMemory().SetInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IN_ADDLOD:
        {
			OP::InstanceAddLODArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddLODArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
            {                
                TArray<FScheduledOp> deps;
                for ( int i=0; i<MUTABLE_OP_MAX_ADD_COUNT; ++i )
                {
                    if ( args.lod[i] )
                    {
                        bool selectedLod = ( (1<<i) & lodMask ) != 0;

                        if ( selectedLod )
                        {
                            deps.Emplace(args.lod[i], item);
                        }
                    }
                }

                AddOp( FScheduledOp( item.At,item, 1), deps );

                break;
            }

            case 1:
            {
                // Assemble result
                InstancePtr pResult = new Instance();

                for ( int i=0; i<MUTABLE_OP_MAX_ADD_COUNT; ++i )
                {
                    if ( args.lod[i] )
                    {
                        bool selectedLod = ( (1<<i) & lodMask ) != 0;

                        if ( selectedLod )
                        {
                            InstancePtrConst pLOD = GetMemory().GetInstance( FCacheAddress(args.lod[i],item) );

                            int LODIndex = pResult->GetPrivate()->AddLOD();

                            // In a degenerated case, the returned pLOD may not have an LOD inside
                            if ( pLOD && !pLOD->GetPrivate()->m_lods.IsEmpty() )
                            {
                                pResult->GetPrivate()->m_lods[LODIndex] = pLOD->GetPrivate()->m_lods[0];
                            }
                        }
                        else
                        {
                            // LOD not selected. Add an empty one
                            pResult->GetPrivate()->AddLOD();
                        }
                    }
                }

                GetMemory().SetInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        default:
                check(false);
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_InstanceAddResource( FScheduledOp& item,
                                  const Model* pModel,
                                  const Parameters* pParams
                                  )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_InstanceAddResource);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::IN_ADDMESH:
        {
			OP::InstanceAddArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.instance, item) );

                // We don't build the resources when building instance: just store ids for them.
                //PushIfNotVisited(args.value, item);
                break;

            case 1:
            {
                InstancePtrConst pBase = GetMemory().GetInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = CloneOrTakeOver<Instance>(pBase.get());
				}

                if ( args.value )
                {
                    RESOURCE_ID meshId = pModel->GetPrivate()->GetResourceKey(
                                args.relevantParametersListIndex,
                                args.value,
                                pParams);
                    OP::ADDRESS nameAd = args.name;
                    check(  nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName = pModel->GetPrivate()->m_program.m_constantStrings[ nameAd ].c_str();
                    pResult->GetPrivate()->AddMesh( 0, 0, meshId, strName );
                }
                GetMemory().SetInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case OP_TYPE::IN_ADDIMAGE:
        {
			OP::InstanceAddArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
				// We don't build the resources when building instance: just store ids for them.
				AddOp( FScheduledOp( item.At, item, 1), FScheduledOp( args.instance, item) );
                break;

            case 1:
            {
                InstancePtrConst pBase = GetMemory().GetInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = CloneOrTakeOver<Instance>(pBase.get());
				}

                if ( args.value )
                {
                    RESOURCE_ID imageId = pModel->GetPrivate()->GetResourceKey(
                                args.relevantParametersListIndex,
                                args.value,
                                pParams);
                    OP::ADDRESS nameAd = args.name;
                    check(  nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName = pModel->GetPrivate()->m_program.m_constantStrings[ nameAd ].c_str();
                    pResult->GetPrivate()->AddImage( 0, 0, 0, imageId, strName );
                }
                GetMemory().SetInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        default:
			check(false);
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_ConstantResource( FScheduledOp& item, const Model* pModel )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Constant);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::ME_CONSTANT:
        {
			OP::MeshConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshConstantArgs>(item.At);

            OP::ADDRESS cat = args.value;

            FProgram& program = pModel->GetPrivate()->m_program;

            // Assume the ROM has been loaded previously
            check(program.m_constantMeshes[cat].Value )

            MeshPtrConst pSourceConst;
            program.GetConstant( cat, pSourceConst );
            MeshPtr pSource = pSourceConst->Clone();

            // Set the separate skeleton if necessary
            if (args.skeleton>=0)
            {
                check( program.m_constantSkeletons.Num()>size_t(args.skeleton)  );
                Ptr<const Skeleton> pSkeleton = program.m_constantSkeletons[args.skeleton];
                pSource->SetSkeleton(pSkeleton);
            }

			if (args.physicsBody >= 0)
			{
                check( program.m_constantPhysicsBodies.Num()>size_t(args.physicsBody)  );
                Ptr<const PhysicsBody> pPhysicsBody = program.m_constantPhysicsBodies[args.physicsBody];
                pSource->SetPhysicsBody(pPhysicsBody);
			}

            GetMemory().SetMesh( item, pSource );
			//UE_LOG(LogMutableCore, Log, TEXT("Set mesh constant %d."), item.At);
            break;
        }

        case OP_TYPE::IM_CONSTANT:
        {
			OP::ResourceConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ResourceConstantArgs>(item.At);
            OP::ADDRESS cat = args.value;

            const FProgram& program = pModel->GetPrivate()->m_program;

			int32 MipsToSkip = item.ExecutionOptions;
            Ptr<const Image> pSource;
            program.GetConstant( cat, pSource, MipsToSkip);

			// Assume the ROM has been loaded previously in a task generated at IssueOp
			check(pSource);

            GetMemory().SetImage( item, pSource );
			//UE_LOG(LogMutableCore, Log, TEXT("Set image constant %d."), item.At);
			break;
        }

        default:
            if (type!=OP_TYPE::NONE)
            {
                // Operation not implemented
                check( false );
            }
            break;
        }
    }

    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Mesh( FScheduledOp& item, const Model* pModel )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Mesh);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::ME_APPLYLAYOUT:
        {
			OP::MeshApplyLayoutArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshApplyLayoutArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.mesh, item),
                           FScheduledOp( args.layout, item) );
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_APPLYLAYOUT)
            		
                Ptr<const Mesh> pBase = GetMemory().GetMesh( FCacheAddress(args.mesh,item) );

                MeshPtr pApplied;
                if (pBase)
                {
                    pApplied = pBase->Clone();

                    Ptr<const Layout> pLayout = GetMemory().GetLayout( FCacheAddress(args.layout,item) );
                    int texCoordsSet = args.channel;
                    MeshApplyLayout( pApplied.get(), pLayout.get(), texCoordsSet );
                }

                GetMemory().SetMesh( item, pApplied );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_DIFFERENCE:
        {
			const uint8* data = pModel->GetPrivate()->m_program.GetOpArgsPointer(item.At);

			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);

            switch (item.Stage)
            {
            case 0:
                if (BaseAt && TargetAt)
                {
                    AddOp( FScheduledOp( item.At, item, 1),
                            FScheduledOp( BaseAt, item),
                            FScheduledOp( TargetAt, item) );
                }
                else
                {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
       	        MUTABLE_CPUPROFILER_SCOPE(ME_DIFFERENCE)

                Ptr<const Mesh> pBase = GetMemory().GetMesh(FCacheAddress(BaseAt,item));
                Ptr<const Mesh> pTarget = GetMemory().GetMesh(FCacheAddress(TargetAt,item));

				TArray<MESH_BUFFER_SEMANTIC, TInlineAllocator<8>> Semantics;
				TArray<int32, TInlineAllocator<8>> SemanticIndices;

				uint8 bIgnoreTextureCoords = 0;
				FMemory::Memcpy(&bIgnoreTextureCoords, data, sizeof(uint8)); data += sizeof(uint8);

				uint8 NumChannels = 0;
				FMemory::Memcpy(&NumChannels, data, sizeof(uint8)); data += sizeof(uint8);

                for ( uint8 i=0; i< NumChannels; ++i )
                {
					uint8 Semantic = 0;
					FMemory::Memcpy(&Semantic, data, sizeof(uint8)); data += sizeof(uint8);
					uint8 SemanticIndex = 0;
					FMemory::Memcpy(&SemanticIndex, data, sizeof(uint8)); data += sizeof(uint8);

					Semantics.Add(MESH_BUFFER_SEMANTIC(Semantic));
					SemanticIndices.Add(SemanticIndex);
                }

                MeshPtr pResult = MeshDifference( pBase.get(), pTarget.get(),
                                          NumChannels, Semantics.GetData(), SemanticIndices.GetData(),
                                          bIgnoreTextureCoords!=0 );

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_MORPH2:
        {
			const uint8* data = pModel->GetPrivate()->m_program.GetOpArgsPointer(item.At);

			OP::ADDRESS FactorAt = 0;
			FMemory::Memcpy(&FactorAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			
			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);

			uint8 NumTargets = 0;
			FMemory::Memcpy(&NumTargets, data, sizeof(uint8)); data += sizeof(uint8);

			TArray<MESH_BUFFER_SEMANTIC, TInlineAllocator<8>> Targets;
			Targets.SetNum(NumTargets);
			for (uint8 T = 0; T < NumTargets; ++T)
			{
				FMemory::Memcpy(&Targets[T], data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			}

			switch (item.Stage)
            {
            case 0:
                if (BaseAt)
                {
                    AddOp( FScheduledOp(item.At, item, 1),
                           FScheduledOp(BaseAt, item),
                           FScheduledOp(FactorAt, item) );
                }
                else
                {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(ME_MORPH2_1)

                bool baseValid = GetMemory().IsValid( FCacheAddress(BaseAt,item) );
                float factor = GetMemory().GetScalar( FCacheAddress(FactorAt,item) );

                // Factor goes from -1 to 1 across all targets. [0 - 1] represents positive morphs, while [-1, 0) represent negative morphs.
				factor = FMath::Clamp(factor, -1.0f, 1.0f); // Is the factor not in range [-1, 1], it will index a non existing morph.

				float absFactor = FMath::Abs(factor);
                float delta = 1.0f/(NumTargets -1);
				int min = (int)FMath::FloorToFloat(absFactor/delta);
				int max = (int)FMath::CeilToFloat(absFactor/delta);

                // Factor from 0 to 1 between the two targets
				float bifactor = absFactor/delta - min;

				// From [0,1] to [-1,0] for negative factors
				if (factor < 0.0f)
				{
					float threshold = -1.0f + UE_SMALL_NUMBER;

					if (factor <= threshold)
					{
						// setting bifactor to -1.0
						bifactor = -1.0f;
					}
					else
					{
						// we calculate the bifactor using positive values but is negative here
						bifactor *= -1.0f;
					}
				}

                if (baseValid)
                {
                    FScheduledOpData HeapData;
					HeapData.Interpolate.Bifactor = bifactor;
					HeapData.Interpolate.Min = FMath::Clamp(min, 0, NumTargets - 1);
					HeapData.Interpolate.Max = FMath::Clamp(max, 0, NumTargets - 1);
					uint32 dataAddress = uint32(m_heapData.Add(HeapData));

                    // Just the first of the targets
					if ( bifactor < UE_SMALL_NUMBER && bifactor > -UE_SMALL_NUMBER )
                    {                        
                        AddOp( FScheduledOp( item.At, item, 2, dataAddress),
                                FScheduledOp(BaseAt, item),
                                FScheduledOp(Targets[min], item) );
                    }
                    // Just the second of the targets
					else if ( bifactor > 1.0f - UE_SMALL_NUMBER || bifactor <= -1.0f + UE_SMALL_NUMBER )
                    {
                        check( max>0 );

                        AddOp( FScheduledOp( item.At, item, 2, dataAddress),
                                FScheduledOp(BaseAt, item),
                                FScheduledOp(Targets[max], item) );
                    }
                    // Mix two targets on the base
                    else
                    {
                        // We will need the base again
                        AddOp( FScheduledOp( item.At, item, 2, dataAddress),
                                FScheduledOp(BaseAt, item),
                                FScheduledOp(Targets[min], item),
                                FScheduledOp(Targets[max], item) );
                    }
                }

                break;
            }

            case 2:
            {
       		    MUTABLE_CPUPROFILER_SCOPE(ME_MORPH2_2)

                Ptr<const Mesh> pBase = GetMemory().GetMesh( FCacheAddress(BaseAt,item) );

                // Factor from 0 to 1 between the two targets
                const FScheduledOpData& HeapData = m_heapData[ (size_t)item.CustomState ];
                float bifactor = HeapData.Interpolate.Bifactor;
                int min = HeapData.Interpolate.Min;
                int max = HeapData.Interpolate.Max;

                MeshPtrConst pResult;

                if (pBase)
                {
                    // Just the first of the targets
					if ( bifactor < UE_SMALL_NUMBER && bifactor > -UE_SMALL_NUMBER )
                    {
                        // Base with one full morph
                        Ptr<const Mesh> pMorph = GetMemory().GetMesh( FCacheAddress(Targets[min],item) );
                        if (pMorph)
                        {
							pResult = MeshMorph(pBase.get(), pMorph.get());
                        }
                    }
                    // Just the second of the targets
                    else if ( bifactor > 1.0f - UE_SMALL_NUMBER )
                    {
                        check( max>0 );
                        Ptr<const Mesh> pMorph = GetMemory().GetMesh( FCacheAddress(Targets[max],item) );
                        if (pMorph)
                        {
							pResult = MeshMorph(pBase.get(), pMorph.get());
                        }
                    }
					// Negative target
					else if (bifactor <= -1.0f + UE_SMALL_NUMBER)
					{
						check( max > 0 );
						Ptr<const Mesh> pMorph = GetMemory().GetMesh(FCacheAddress(Targets[max], item));
						if (pMorph)
						{
							pResult = MeshMorph(pBase.get(), pMorph.get(), bifactor);
						}
					}
                    // Mix two targets on the base
                    else
                    {
                        Ptr<const Mesh> pMin = GetMemory().GetMesh( FCacheAddress(Targets[min],item) );
                        Ptr<const Mesh> pMax = GetMemory().GetMesh( FCacheAddress(Targets[max],item) );
                        if (pMin && pMax)
                        {
                            pResult = MeshMorph2( pBase.get(), pMin.get(), pMax.get(), bifactor );
                        }
                    }
					// Missing branch. With the current system we can never get apply a negative pMin morph, only a negative pMax morph.
					// This is due to not having a bifactor value that represents a negative pMin morph.
					// "-0" should represent a negative pMin morph, but it has the contradiction that 0 also represents positive pMax morph.
					// In other words, we have a discontinuity arround 0.
					// With the current implementation, the only solutions to get a negative pMin morph is to apply a really small negative value (lim x -> -0) (for example: -0.00001).
                }
                GetMemory().SetMesh( item, pResult );

                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_MERGE:
        {
			OP::MeshMergeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshMergeArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.base, item),
                           FScheduledOp( args.added, item) );
                break;

            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_MORPH2_1)

                MeshPtrConst pA = GetMemory().GetMesh( FCacheAddress(args.base,item) );
                MeshPtrConst pB = GetMemory().GetMesh( FCacheAddress(args.added,item) );

                MeshPtr pResult;

                if (pA && pB && pA->GetVertexCount() && pB->GetVertexCount())
                {
                    pResult = MeshMerge( pA.get(), pB.get() );

                    if (!args.newSurfaceID)
                    {
                        // Make it a single surface.
                        pResult->m_surfaces.Empty();
                        pResult->EnsureSurfaceData();
                    }
                    else
                    {
                        check(pB->GetSurfaceCount()==1);
                        pResult->m_surfaces.Last().m_id=args.newSurfaceID;
                    }
                }
                else if (pA && pA->GetVertexCount())
                {
                    pResult = pA->Clone();
                }
                else if (pB && pB->GetVertexCount())
                {
                    pResult = pB->Clone();
                    check(pResult->GetSurfaceCount()==1);
                    if (pResult->GetSurfaceCount()>0 && args.newSurfaceID)
                    {
                        pResult->m_surfaces.Last().m_id=args.newSurfaceID;
                    }
                }
                else
                {
                    pResult = new Mesh();
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_INTERPOLATE:
        {
			OP::MeshInterpolateArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshInterpolateArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if ( args.base )
                {
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.base, item),
                           FScheduledOp( args.factor, item ) );
                }
                else
                {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_INTERPOLATE_1)

                int count = 1;
                for ( int i=0
                    ; i<MUTABLE_OP_MAX_INTERPOLATE_COUNT-1 && args.targets[i]
                    ; ++i )
                {
                    count++;
                }

                // Factor from 0 to 1 across all targets
                float factor = GetMemory().GetScalar( FCacheAddress(args.factor,item) );

                float delta = 1.0f/(count-1);
                int min = (int)floorf( factor/delta );
                int max = (int)ceilf( factor/delta );

                // Factor from 0 to 1 between the two targets
                float bifactor = factor/delta - min;

                FScheduledOpData data;
                data.Interpolate.Bifactor = bifactor;
				data.Interpolate.Min = FMath::Clamp(min, 0, count - 1);
				data.Interpolate.Max = FMath::Clamp(max, 0, count - 1);
				uint32 dataAddress = uint32(m_heapData.Num());

                // Just the first of the targets
                if ( bifactor < UE_SMALL_NUMBER )
                {
                    if( min==0 )
                    {
                        // Just the base
                            Ptr<const Mesh> pBase = GetMemory().GetMesh( FCacheAddress(args.base,item) );
                            MeshPtr pResult;
                            if (pBase)
                            {
                                pResult = pBase->Clone();
                            }
                            GetMemory().SetMesh( item, pResult );
                        }
                    else
                    {
                        // Base with one full morph
                        m_heapData.Add(data);
                            AddOp( FScheduledOp( item.At, item, 2, dataAddress),
                                   FScheduledOp( args.base, item),
                                   FScheduledOp( args.targets[min-1], item) );
                        }
                    }
                // Just the second of the targets
                else if ( bifactor > 1.0f-UE_SMALL_NUMBER )
                {
                    m_heapData.Add(data);
                        AddOp( FScheduledOp( item.At, item, 2, dataAddress),
                               FScheduledOp( args.base, item),
                               FScheduledOp( args.targets[max-1], item) );
                    }
                // Mix the first target on the base
                else if ( min==0 )
                {
                    m_heapData.Add(data);
                        AddOp( FScheduledOp( item.At, item, 2, dataAddress),
                               FScheduledOp( args.base, item),
                               FScheduledOp( args.targets[0], item)
                               );
                    }
                // Mix two targets on the base
                else
                {
                    m_heapData.Add(data);
                        AddOp( FScheduledOp( item.At, item, 2, dataAddress),
                               FScheduledOp( args.base, item),
                               FScheduledOp( args.targets[min-1], item),
                               FScheduledOp( args.targets[max-1], item) );
                    }

                break;
            }

            case 2:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_INTERPOLATE_2)

                int count = 1;
                for ( int i=0
                    ; i<MUTABLE_OP_MAX_INTERPOLATE_COUNT-1 && args.targets[i]
                    ; ++i )
                {
                    count++;
                }

                const FScheduledOpData& data = m_heapData[ (size_t)item.CustomState ];

                // Factor from 0 to 1 between the two targets
                float bifactor = data.Interpolate.Bifactor;
                int min = data.Interpolate.Min;
                int max = data.Interpolate.Max;

                Ptr<const Mesh> pBase = GetMemory().GetMesh( FCacheAddress(args.base, item) );

                MeshPtr pResult;

                if (pBase)
                {
                    // Just the first of the targets
                    if ( bifactor < UE_SMALL_NUMBER )
                    {
                        if( min==0 )
                        {
                            // Just the base. It should have been dealt with in the previous stage.
                            check( false );
                        }
                        else
                        {
                            // Base with one full morph
                            Ptr<const Mesh> pMorph = GetMemory().GetMesh( FCacheAddress(args.targets[min-1],item) );
                            pResult = MeshMorph( pBase.get(), pMorph.get() );
                        }
                    }
                    // Just the second of the targets
                    else if ( bifactor > 1.0f-UE_SMALL_NUMBER )
                    {
                        check( max>0 );
                        Ptr<const Mesh> pMorph = GetMemory().GetMesh( FCacheAddress(args.targets[max-1],item) );

                        if (pMorph)
                        {
                            pResult = MeshMorph( pBase.get(), pMorph.get() );
                        }
                        else
                        {
                            pResult = pBase->Clone();
                        }
                    }
                    // Mix the first target on the base
                    else if ( min==0 )
                    {
                        Ptr<const Mesh> pMorph = GetMemory().GetMesh( FCacheAddress( args.targets[0], item ) );
                        if (pMorph)
                        {
                            pResult = MeshMorph( pBase.get(), pMorph.get(), bifactor );
                        }
                        else
                        {
                            pResult = pBase->Clone();
                        }

                    }
                    // Mix two targets on the base
                    else
                    {
                        Ptr<const Mesh> pMin = GetMemory().GetMesh( FCacheAddress(args.targets[min-1],item) );
                        Ptr<const Mesh> pMax = GetMemory().GetMesh( FCacheAddress(args.targets[max-1],item) );

                        if (pMin && pMax)
                        {
                            pResult = MeshMorph2( pBase.get(), pMin.get(), pMax.get(), bifactor );
                        }
                        else
                        {
                            pResult = pBase->Clone();
                        }
                    }
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_MASKCLIPMESH:
        {
			OP::MeshMaskClipMeshArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshMaskClipMeshArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.source, item),
                           FScheduledOp( args.clip, item) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_MASKCLIPMESH_1)
            		
                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );
                Ptr<const Mesh> pClip = GetMemory().GetMesh( FCacheAddress(args.clip,item) );

                // Only if both are valid.
                MeshPtr pResult;
                if (pSource.get() && pClip.get())
                {
                    pResult = MeshMaskClipMesh(pSource.get(), pClip.get());
                }
                GetMemory().SetMesh( item, pResult );

                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_MASKDIFF:
        {
			OP::MeshMaskDiffArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshMaskDiffArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.source, item),
                           FScheduledOp( args.fragment, item) );
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_MASKDIFF_1)
            		
                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );
                Ptr<const Mesh> pClip = GetMemory().GetMesh( FCacheAddress(args.fragment,item) );

                // Only if both are valid.
                MeshPtr pResult;
                if (pSource.get() && pClip.get())
                {
                    pResult = MeshMaskDiff(pSource.get(), pClip.get());
                }
                GetMemory().SetMesh( item, pResult );

                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_SUBTRACT:
        {
			OP::MeshSubtractArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshSubtractArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.a, item),
                           FScheduledOp( args.b, item) );
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_SUBTRACT_1)
            	
                Ptr<const Mesh> pA = GetMemory().GetMesh( FCacheAddress(args.a,item) );
                Ptr<const Mesh> pB = GetMemory().GetMesh( FCacheAddress(args.b,item) );

                MeshPtr pResult = MeshSubtract( pA.get(), pB.get() );

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_FORMAT:
        {
			OP::MeshFormatArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshFormatArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.source && args.format)
                {
                         AddOp( FScheduledOp( item.At, item, 1),
                                FScheduledOp( args.source, item),
                                FScheduledOp( args.format, item));
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_FORMAT_1)
            		
                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );
                Ptr<const Mesh> pFormat = GetMemory().GetMesh( FCacheAddress(args.format,item) );

                uint8 flags = args.buffers;
                MeshPtr pResult;
                pResult = MeshFormat( pSource.get(),
                                      pFormat.get(),
                                      true,
                                      (flags & OP::MeshFormatArgs::BT_VERTEX) != 0,
                                      (flags & OP::MeshFormatArgs::BT_INDEX) != 0,
                                      (flags & OP::MeshFormatArgs::BT_FACE) != 0,
                                      (flags & OP::MeshFormatArgs::BT_IGNORE_MISSING) != 0
                                      );

                if (flags & OP::MeshFormatArgs::BT_RESETBUFFERINDICES)
                {
                    pResult->ResetBufferIndices();
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_EXTRACTLAYOUTBLOCK:
        {
            const uint8* data = pModel->GetPrivate()->m_program.GetOpArgsPointer(item.At);

            OP::ADDRESS source;
            FMemory::Memcpy( &source, data, sizeof(OP::ADDRESS) );
            data += sizeof(OP::ADDRESS);

            uint16 layout;
			FMemory::Memcpy( &layout, data, sizeof(uint16) );
            data += sizeof(uint16);

            uint16 blockCount;
			FMemory::Memcpy( &blockCount, data, sizeof(uint16) );
            data += sizeof(uint16);

            switch (item.Stage)
            {
            case 0:
                if (source)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( source, item) );
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_EXTRACTLAYOUTBLOCK_1)

                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(source,item) );

                // Access with memcpy necessary for unaligned arm issues.
                uint32 blocks[1024];
				FMemory::Memcpy( blocks, data, sizeof(uint32)*FMath::Min(1024,int(blockCount)) );

                MeshPtr pResult;
                pResult = MeshExtractLayoutBlock( pSource.get(),
                                                  layout,
                                                  blockCount,
                                                  blocks );

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_EXTRACTFACEGROUP:
        {
			OP::MeshExtractFaceGroupArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshExtractFaceGroupArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.source)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.source, item) );
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_EXTRACTFACEGROUP_1)

                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );

                MeshPtr pResult;
                pResult = MeshExtractFaceGroup( pSource.get(),
                                                args.group );

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_TRANSFORM:
        {
			OP::MeshTransformArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshTransformArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.source)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.source, item) );
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_TRANSFORM_1)
            		
                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );

                const mat4f& mat = pModel->GetPrivate()->m_program.
                    m_constantMatrices[args.matrix];

                MeshPtr pResult = MeshTransform(pSource.get(), ToUnreal(mat) );

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_CLIPMORPHPLANE:
        {
			OP::MeshClipMorphPlaneArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshClipMorphPlaneArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.source)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.source, item) );
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_CLIPMORPHPLANE_1)
            		
                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );

                MeshPtr pResult;

                check( args.morphShape < (uint32)pModel->GetPrivate()->m_program.m_constantShapes.Num() );

                // Should be an ellipse
                const FShape& morphShape = pModel->GetPrivate()->m_program.
                    m_constantShapes[args.morphShape];

                const mu::vec3f& origin = morphShape.position;
                const mu::vec3f& normal = morphShape.up;

                if (args.vertexSelectionType == OP::MeshClipMorphPlaneArgs::VS_SHAPE)
                {
                    check( args.vertexSelectionShapeOrBone < (uint32)pModel->GetPrivate()->m_program.m_constantShapes.Num() );

                    // Should be None or an axis aligned box
                    const FShape& selectionShape = pModel->GetPrivate()->m_program.m_constantShapes[args.vertexSelectionShapeOrBone];
                    pResult = MeshClipMorphPlane(pSource.get(), origin, normal, args.dist, args.factor, morphShape.size[0], morphShape.size[1], morphShape.size[2], selectionShape);
                }

                else if (args.vertexSelectionType == OP::MeshClipMorphPlaneArgs::VS_BONE_HIERARCHY)
                {
                    check( args.vertexSelectionShapeOrBone < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );

                    FShape selectionShape;
                    selectionShape.type = (uint8)FShape::Type::None;
                    const string& selectionBone = pModel->GetPrivate()->m_program.m_constantStrings[args.vertexSelectionShapeOrBone];
					pResult = MeshClipMorphPlane(pSource.get(), origin, normal, args.dist, args.factor, morphShape.size[0], morphShape.size[1], morphShape.size[2], selectionShape, selectionBone, args.maxBoneRadius);
                }
                else
                {
                    // No vertex selection
                    FShape selectionShape;
                    selectionShape.type = (uint8)FShape::Type::None;
                    pResult = MeshClipMorphPlane(pSource.get(), origin, normal, args.dist, args.factor, morphShape.size[0], morphShape.size[1], morphShape.size[2], selectionShape);
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }


        case OP_TYPE::ME_CLIPWITHMESH:
        {
			OP::MeshClipWithMeshArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshClipWithMeshArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.source)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.source, item),
                               FScheduledOp( args.clipMesh, item) );
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_CLIPWITHMESH_1)

                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );
                Ptr<const Mesh> pClip = GetMemory().GetMesh( FCacheAddress(args.clipMesh,item) );

                // Only if both are valid.
                MeshPtr pResult;
                if (pSource && pClip)
                {
                    pResult = MeshClipWithMesh(pSource.get(), pClip.get());
                }
                else if (pSource)
                {
                    pResult = pSource->Clone();
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }
		case OP_TYPE::ME_CLIPDEFORM:
		{
			OP::MeshClipDeformArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshClipDeformArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				if (args.mesh)
				{
						AddOp(FScheduledOp(item.At, item, 1),
							FScheduledOp(args.mesh, item),
							FScheduledOp(args.clipShape, item));
					}
					else
					{
					GetMemory().SetMesh(item, nullptr);
				}
				break;
			
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_CLIPDEFORM_1)

				Ptr<const Mesh> BaseMesh = GetMemory().GetMesh(FCacheAddress(args.mesh, item));
				Ptr<const Mesh> ClipShape = GetMemory().GetMesh(FCacheAddress(args.clipShape, item));
				Ptr<Mesh> pResult;

				if ( BaseMesh && ClipShape )
				{
					pResult = MeshClipDeform(BaseMesh.get(), ClipShape.get(), args.clipWeightThreshold);
				}
				else if( BaseMesh )
				{
					pResult = BaseMesh->Clone();
				}

				GetMemory().SetMesh(item, pResult);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case OP_TYPE::ME_REMAPINDICES:
        {
			OP::MeshRemapIndicesArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshRemapIndicesArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.source)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.source, item),
                               FScheduledOp( args.reference, item) );
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
    			MUTABLE_CPUPROFILER_SCOPE(ME_REMAPINDICES_1)

                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );
                Ptr<const Mesh> pReference = GetMemory().GetMesh( FCacheAddress(args.reference,item) );

                // Only if both are valid.
                MeshPtr pResult;
                if (pSource && pReference)
                {
                    pResult = MeshRemapIndices(pSource.get(), pReference.get());
                }
                else if (pSource)
                {
                    pResult = pSource->Clone();
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }


        case OP_TYPE::ME_APPLYPOSE:
        {
			OP::MeshApplyPoseArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshApplyPoseArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.base)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.base, item),
                               FScheduledOp( args.pose, item) );
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
          		MUTABLE_CPUPROFILER_SCOPE(ME_APPLYPOSE_1)

                Ptr<const Mesh> pBase = GetMemory().GetMesh( FCacheAddress(args.base,item) );
                Ptr<const Mesh> pPose = GetMemory().GetMesh( FCacheAddress(args.pose,item) );

                // Only if both are valid.
                MeshPtr pResult;
                if (pBase && pPose)
                {
                    pResult = MeshApplyPose(pBase.get(), pPose.get());
                }
                else if (pBase)
                {
                    pResult = pBase->Clone();
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }


		case OP_TYPE::ME_GEOMETRYOPERATION:
		{
			OP::MeshGeometryOperationArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshGeometryOperationArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				if (args.meshA)
				{
						AddOp(FScheduledOp(item.At, item, 1),
							FScheduledOp(args.meshA, item),
							FScheduledOp(args.meshB, item),
							FScheduledOp(args.scalarA, item),
							FScheduledOp(args.scalarB, item));
					}
					else
					{
					GetMemory().SetMesh(item, nullptr);
				}
				break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_GEOMETRYOPERATION_1)
				
				Ptr<const Mesh> MeshA = GetMemory().GetMesh(FCacheAddress(args.meshA, item));
				Ptr<const Mesh> MeshB = GetMemory().GetMesh(FCacheAddress(args.meshB, item));
				float ScalarA = GetMemory().GetScalar(FCacheAddress(args.scalarA, item));
				float ScalarB = GetMemory().GetScalar(FCacheAddress(args.scalarB, item));

				MeshPtr pResult = MeshGeometryOperation(MeshA.get(),MeshB.get(),ScalarA,ScalarB);

				GetMemory().SetMesh(item, pResult);
				break;
			}

			default:
				check(false);
			}

			break;
		}


		case OP_TYPE::ME_BINDSHAPE:
		{
			OP::MeshBindShapeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshBindShapeArgs>(item.At);
			const uint8* data = pModel->GetPrivate()->m_program.GetOpArgsPointer(item.At);

			switch (item.Stage)
			{
			case 0:
				if (args.mesh)
				{
					AddOp(FScheduledOp(item.At, item, 1),
							FScheduledOp(args.mesh, item),
							FScheduledOp(args.shape, item));
				}
				else
				{
					GetMemory().SetMesh(item, nullptr);
				}
				break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_BINDSHAPE_1)
				
				Ptr<const Mesh> BaseMesh = GetMemory().GetMesh(FCacheAddress(args.mesh, item));
				Ptr<const Mesh> Shape = GetMemory().GetMesh(FCacheAddress(args.shape, item));
				
				EShapeBindingMethod BindingMethod = static_cast<EShapeBindingMethod>(args.bindingMethod); 

				if (BindingMethod == EShapeBindingMethod::ReshapeClosestProject)
				{ 
					// Bones are stored after the args
					data += sizeof(args);

					// Rebuilding array of bone names ----
					int32 NumBones;
					FMemory::Memcpy(&NumBones, data, sizeof(int32)); 
					data += sizeof(int32);
					
					TArray<string> BonesToDeform;
					BonesToDeform.SetNum(NumBones);
					for (int32 b = 0; b < NumBones; ++b)
					{
						BonesToDeform[b] = pModel->GetPrivate()->m_program.m_constantStrings[*data].c_str();
						data += sizeof(int32);
					}

					int32 NumPhysicsBodies;
					FMemory::Memcpy(&NumPhysicsBodies, data, sizeof(int32)); 
					data += sizeof(int32);

					TArray<string> PhysicsToDeform;
					PhysicsToDeform.SetNum(NumPhysicsBodies);
					for (int32 b = 0; b < NumPhysicsBodies; ++b)
					{
						PhysicsToDeform[b] = pModel->GetPrivate()->m_program.m_constantStrings[*data].c_str();
						data += sizeof(int32);
					}

					// -----------
					Ptr<Mesh> pResult = MeshBindShapeReshape( BaseMesh.get(), Shape.get(), BonesToDeform, PhysicsToDeform, 
						    static_cast<EMeshBindShapeFlags>(args.flags));
					
					GetMemory().SetMesh(item, pResult);
				}	
				else
				{
					Ptr<Mesh> ResultPtr = MeshBindShapeClipDeform( BaseMesh.get(), Shape.get(), BindingMethod );
					GetMemory().SetMesh(item, ResultPtr);
				}

				break;
			}

			default:
				check(false);
			}

			break;
		}


		case OP_TYPE::ME_APPLYSHAPE:
		{
			OP::MeshApplyShapeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshApplyShapeArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				if (args.mesh)
				{
						AddOp(FScheduledOp(item.At, item, 1),
							FScheduledOp(args.mesh, item),
							FScheduledOp(args.shape, item));
				}
				else
				{
					GetMemory().SetMesh(item, nullptr);
				}
				break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_APPLYSHAPE_1)
					
				Ptr<const Mesh> BaseMesh = GetMemory().GetMesh(FCacheAddress(args.mesh, item));
				Ptr<const Mesh> Shape = GetMemory().GetMesh(FCacheAddress(args.shape, item));


				Ptr<Mesh> pResult = MeshApplyShape(BaseMesh.get(), Shape.get(), static_cast<EMeshBindShapeFlags>(args.flags));

				GetMemory().SetMesh(item, pResult);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::ME_MORPHRESHAPE:
		{
			OP::MeshMorphReshapeArgs Args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshMorphReshapeArgs>(item.At);
			switch(item.Stage)
			{
			case 0:
			{
				if (Args.Morph)
				{
					AddOp(FScheduledOp(item.At, item, 1), 
						  FScheduledOp(Args.Morph, item), 
						  FScheduledOp(Args.Reshape, item));
				}
				else 
				{
					GetMemory().SetMesh(item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_MORPHRESHAPE_1)
					
				Ptr<const Mesh> MorphedMesh = GetMemory().GetMesh(FCacheAddress(Args.Morph, item));
				Ptr<const Mesh> ReshapeMesh = GetMemory().GetMesh(FCacheAddress(Args.Reshape, item));

				if (ReshapeMesh)
				{
					// Clone without Skeleton, Physics or Poses 
					EMeshCloneFlags CloneFlags = ~(
							EMeshCloneFlags::WithSkeleton    | 
							EMeshCloneFlags::WithPhysicsBody | 
							EMeshCloneFlags::WithPoses);

					Ptr<Mesh> ResultPtr = MorphedMesh->Clone(CloneFlags);
					ResultPtr->SetSkeleton(ReshapeMesh->GetSkeleton().get());
					ResultPtr->SetPhysicsBody(ReshapeMesh->GetPhysicsBody().get());
					ResultPtr->BonePoses = ReshapeMesh->BonePoses;

					GetMemory().SetMesh(item, ResultPtr);
				}
				else
				{
					Ptr<Mesh> ResultPtr = MorphedMesh->Clone();
					GetMemory().SetMesh(item, ResultPtr);
				}

				break;
			}

			default:
				check(false);
			}

			break;
		}

        case OP_TYPE::ME_SETSKELETON:
        {
			OP::MeshSetSkeletonArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshSetSkeletonArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.source)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.source, item),
                               FScheduledOp( args.skeleton, item) );
                    }
                    else
                    {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_SETSKELETON_1)
            		
                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(args.source,item) );
                Ptr<const Mesh> pSkeleton = GetMemory().GetMesh( FCacheAddress(args.skeleton,item) );

                // Only if both are valid.
                MeshPtr pResult;
                if (pSource && pSkeleton)
                {
                    if ( pSource->GetSkeleton()
                         &&
                         !pSource->GetSkeleton()->m_bones.IsEmpty() )
                    {
                        // For some reason we already have bone data, so we can't just overwrite it
                        // or the skinning may break. This may happen because of a problem in the
                        // optimiser that needs investigation.
                        // \TODO Be defensive, for now.
                        UE_LOG(LogMutableCore,Warning, TEXT("Performing a MeshRemapSkeleton, instead of MeshSetSkeletonData because source mesh already has some skeleton."));
                        pResult = MeshRemapSkeleton( pSource.get(), pSkeleton->GetSkeleton().get() );
                        if (!pResult)
                        {
                            pResult = pSource->Clone();
                        }
                    }
                    else
                    {
                        pResult = pSource->Clone();
                        pResult->SetSkeleton(pSkeleton->GetSkeleton().get());
                    }
                    //pResult->GetPrivate()->CheckIntegrity();
                }
                else if (pSource)
                {
                    pResult = pSource->Clone();
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_REMOVEMASK:
        {
       		MUTABLE_CPUPROFILER_SCOPE(ME_REMOVEMASK)
        		
            // Decode op
            // TODO: Partial decode for each stage
            const uint8* data = pModel->GetPrivate()->m_program.GetOpArgsPointer(item.At);

            OP::ADDRESS source;
            FMemory::Memcpy(&source,data,sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);

            TArray<FScheduledOp> conditions;
			TArray<OP::ADDRESS> masks;

            uint16 removes;
			FMemory::Memcpy(&removes,data,sizeof(uint16)); data += sizeof(uint16);

            for( uint16 r=0; r<removes; ++r)
            {
                OP::ADDRESS condition;
				FMemory::Memcpy(&condition,data,sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
                conditions.Emplace( condition, item );

                OP::ADDRESS mask;
				FMemory::Memcpy(&mask,data,sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
                masks.Add( mask );
            }


            // Schedule next stages
            switch (item.Stage)
            {
            case 0:
                if (source)
                {
                    // Request the conditions
                    AddOp( FScheduledOp( item.At, item, 1), conditions );
                }
                else
                {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
        		MUTABLE_CPUPROFILER_SCOPE(ME_REMOVEMASK_1)

                // Request the source and the necessary masks
                // \todo: store condition values in heap?
                TArray<FScheduledOp> deps;
                deps.Emplace( source, item );
                for( size_t r=0; source && r<conditions.Num(); ++r )
                {
                    // If there is no expression, we'll assume true.
                    bool value = true;
                    if (conditions[r].At)
                    {
                        value = GetMemory().GetBool( FCacheAddress(conditions[r].At, item) );
                    }

                    if (value)
                    {
                        deps.Emplace( masks[r], item );
                    }
                }

                if (source)
                {
                        AddOp( FScheduledOp( item.At, item, 2), deps );
                    }
                break;
            }

            case 2:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_REMOVEMASK_2)
            	
                // \todo: single remove operation with all masks?
                Ptr<const Mesh> pSource = GetMemory().GetMesh( FCacheAddress(source,item) );

                MeshPtrConst pResult = pSource;

                for( size_t r=0; pResult && r<conditions.Num(); ++r )
                {
                    // If there is no expression, we'll assume true.
                    bool value = true;
                    if (conditions[r].At)
                    {
                        value = GetMemory().GetBool( FCacheAddress(conditions[r].At, item) );
                    }

                    if (value)
                    {
                        Ptr<const Mesh> pMask = GetMemory().GetMesh( FCacheAddress(masks[r],item) );
                        if (pMask)
                        {
                            pResult = MeshRemoveMask(pResult.get(), pMask.get());
                        }
                    }
                }

                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_PROJECT:
        {
			OP::MeshProjectArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshProjectArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                if (args.mesh)
                {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.mesh, item),
                               FScheduledOp( args.projector, item));
                }
                else
                {
                    GetMemory().SetMesh( item, nullptr );
                }
                break;

            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_PROJECT_1)

                Ptr<const Mesh> pMesh = GetMemory().GetMesh( FCacheAddress(args.mesh,item) );
                Ptr<const Projector> pProjector = GetMemory().GetProjector( FCacheAddress(args.projector,item) );

                // Only if both are valid.
                MeshPtr pResult;
                if (pMesh
                    && pMesh.get()
                    && pMesh.get()->GetVertexBuffers().GetBufferCount() > 0 )
                {
                    if (pProjector)
                    {
                        pResult = MeshProject(pMesh.get(), pProjector->m_value);
//                        if (pResult)
//                        {
//                            pResult->GetPrivate()->CheckIntegrity();
//                        }
                    }
                    else if (pMesh)
                    {
                        pResult = pMesh->Clone();
                    }
                }
                GetMemory().SetMesh( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case OP_TYPE::ME_OPTIMIZESKINNING:
		{
			OP::MeshOptimizeSkinningArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshOptimizeSkinningArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				if (args.source)
				{
					AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.source, item));
				}
				else
				{
					GetMemory().SetMesh(item, nullptr);
				}
				break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_OPTIMIZESKINNING_1)

				Ptr<const Mesh> pSource = GetMemory().GetMesh(FCacheAddress(args.source, item));

				MeshPtrConst pResult = MeshOptimizeSkinning(pSource.get());

				GetMemory().SetMesh(item, pResult ? pResult : pSource);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        default:
            if (type!=OP_TYPE::NONE)
            {
                // Operation not implemented
                check( false );
            }
            break;
        }
    }

    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Image( FScheduledOp& item,
                                    const Parameters* pParams,
                                    const Model* pModel
                                    )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Image);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
		switch (type)
        {

        case OP_TYPE::IM_LAYERCOLOUR:
        {
			OP::ImageLayerColourArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageLayerColourArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.base, item),
                           FScheduledOp::FromOpAndOptions( args.colour, item, 0),
                           FScheduledOp( args.mask, item) );
                break;

            case 1:
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_LAYER:
        {
			OP::ImageLayerArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageLayerArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.base, item),
                           FScheduledOp( args.blended, item),
                           FScheduledOp( args.mask, item) );
                break;

            case 1:
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;
			
            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_MULTILAYER:
        {
			OP::ImageMultiLayerArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageMultiLayerArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1),
                       FScheduledOp( args.rangeSize, item ),
					   FScheduledOp(args.base, item));
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_MULTILAYER_1)
            		
                // We now know the number of iterations
                int32 Iterations = 0;
                if (args.rangeSize)
                {
                    FCacheAddress RangeAddress(args.rangeSize,item);

                    // We support both integers and scalars here, which is not common.
                    // \todo: review if this is necessary or we can enforce it at compile time.
                    DATATYPE RangeSizeType = GetOpDataType( pModel->GetPrivate()->m_program.GetOpType(args.rangeSize) );
                    if (RangeSizeType == DT_INT)
                    {
						Iterations = GetMemory().GetInt(RangeAddress);
                    }
                    else if (RangeSizeType == DT_SCALAR)
                    {
						Iterations = int32( GetMemory().GetScalar(RangeAddress) );
                    }
                }

				Ptr<const Image> Base = GetMemory().GetImage(FCacheAddress(args.base, item));

				if (Iterations <= 0)
				{
					// There are no layers: return the base
					GetMemory().SetImage(item, Base);
				}
				else
				{
					// Store the base
					Ptr<Image> New = mu::CloneOrTakeOver(Base.get());

					// This shouldn't happen in optimised models, but it could happen in editors, etc.
					// \todo: raise a performance warning?
					EImageFormat BaseFormat = GetUncompressedFormat(Base->GetFormat());
					if (Base->GetFormat() != BaseFormat)
					{
						Base = ImagePixelFormat(m_pSettings->GetPrivate()->m_imageCompressionQuality, Base.get(), BaseFormat);
					}

					FScheduledOpData Data;
					Data.Resource = New;
					Data.MultiLayer.Iterations = Iterations;
					Data.MultiLayer.OriginalBaseFormat = Base->GetFormat();
					Data.MultiLayer.bBlendOnlyOneMip = false;
					int32 DataPos = m_heapData.Add(Data);

					// Request the first layer
					int32 CurrentIteration = 0;
					FScheduledOp ItemCopy = item;
					ExecutionIndex Index = GetMemory().GetRageIndex(item.ExecutionIndex);
					Index.SetFromModelRangeIndex(args.rangeId, CurrentIteration);
					ItemCopy.ExecutionIndex = GetMemory().GetRageIndexIndex(Index);
					AddOp(FScheduledOp(item.At, item, 2, DataPos), FScheduledOp(args.base, item), FScheduledOp(args.blended, ItemCopy), FScheduledOp(args.mask, ItemCopy));
				}

                break;
            }

            default:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_MULTILAYER_default)

				FScheduledOpData& Data = m_heapData[item.CustomState];

				int32 Iterations = Data.MultiLayer.Iterations;
				int32 CurrentIteration = item.Stage - 2;
				check(CurrentIteration >= 0 && CurrentIteration < 120);

				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("Layer %d of %d"), CurrentIteration, Iterations));

				// Process the current layer

				Ptr<Image> Base = static_cast<Image*>(Data.Resource.get());
 
                FScheduledOp itemCopy = item;
                ExecutionIndex index = GetMemory().GetRageIndex( item.ExecutionIndex );
				
                {
                    index.SetFromModelRangeIndex( args.rangeId, CurrentIteration);
                    itemCopy.ExecutionIndex = GetMemory().GetRageIndexIndex(index);
					itemCopy.CustomState = 0;

                    Ptr<const Image> pBlended = GetMemory().GetImage( FCacheAddress(args.blended,itemCopy) );

                    // This shouldn't happen in optimised models, but it could happen in editors, etc.
                    // \todo: raise a performance warning?
                    if (pBlended && pBlended->GetFormat()!=Base->GetFormat() )
                    {
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_BlendedReformat);
						pBlended = ImagePixelFormat( m_pSettings->GetPrivate()->m_imageCompressionQuality, pBlended.get(), Base->GetFormat());
                    }

					// TODO: This shouldn't happen, but be defensive.
					FImageSize ResultSize = Base->GetSize();
					if (pBlended && pBlended->GetSize() != ResultSize)
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_BlendedFixForMultilayer);
						pBlended = ImageResizeLinear(0, pBlended.get(), ResultSize);
					}

					if (pBlended->GetLODCount() < Base->GetLODCount())
					{
						Data.MultiLayer.bBlendOnlyOneMip = true;
					}

					bool bApplyColorBlendToAlpha = false;

					bool bDone = false;

					// This becomes true if we need to update the mips of the resulting image
					// This could happen in the base image has mips, but one of the blended one doesn't.
					bool bBlendOnlyOneMip = Data.MultiLayer.bBlendOnlyOneMip;

					if (!args.mask && args.bUseMaskFromBlended
						&&
						args.blendType == uint8(EBlendType::BT_BLEND)
						&&
						args.blendTypeAlpha == uint8(EBlendType::BT_LIGHTEN) )
					{
						// This is a frequent critical-path case because of multilayer projectors.
						bDone = true;
						
						BufferLayerComposite<BlendChannelMasked, LightenChannel, false>(Base.get(), pBlended.get(), bBlendOnlyOneMip);
					}

                    if (!bDone && args.mask)
                    {
                        Ptr<const Image> pMask = GetMemory().GetImage( FCacheAddress(args.mask,itemCopy) );

						// TODO: This shouldn't happen, but be defensive.
						if (pMask && pMask->GetSize() != ResultSize)
						{
							MUTABLE_CPUPROFILER_SCOPE(ImageResize_MaskFixForMultilayer);
							pMask = ImageResizeLinear(0, pMask.get(), ResultSize);
						}

                        switch (EBlendType(args.blendType))
                        {
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
                        case EBlendType::BT_SOFTLIGHT: BufferLayer<SoftLightChannelMasked, SoftLightChannel, false>( Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_HARDLIGHT: BufferLayer<HardLightChannelMasked, HardLightChannel, false>(Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_BURN: BufferLayer<BurnChannelMasked, BurnChannel, false>(Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_DODGE: BufferLayer<DodgeChannelMasked, DodgeChannel, false>(Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_SCREEN: BufferLayer<ScreenChannelMasked, ScreenChannel, false>(Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_OVERLAY: BufferLayer<OverlayChannelMasked, OverlayChannel, false>(Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_LIGHTEN: BufferLayer<LightenChannelMasked, LightenChannel, false>(Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_MULTIPLY: BufferLayer<MultiplyChannelMasked, MultiplyChannel, false>(Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_BLEND: BufferLayer<BlendChannelMasked, BlendChannel, false>(Base->GetData(), Base.get(), pMask.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        default: check(false);
                        }

                    }
					else if (!bDone && args.bUseMaskFromBlended)
					{
						switch (EBlendType(args.blendType))
						{
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
						case EBlendType::BT_SOFTLIGHT: BufferLayerEmbeddedMask<SoftLightChannelMasked, SoftLightChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_HARDLIGHT: BufferLayerEmbeddedMask<HardLightChannelMasked, HardLightChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_BURN: BufferLayerEmbeddedMask<BurnChannelMasked, BurnChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_DODGE: BufferLayerEmbeddedMask<DodgeChannelMasked, DodgeChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_SCREEN: BufferLayerEmbeddedMask<ScreenChannelMasked, ScreenChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_OVERLAY: BufferLayerEmbeddedMask<OverlayChannelMasked, OverlayChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_LIGHTEN: BufferLayerEmbeddedMask<LightenChannelMasked, LightenChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_MULTIPLY: BufferLayerEmbeddedMask<MultiplyChannelMasked, MultiplyChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_BLEND: BufferLayerEmbeddedMask<BlendChannelMasked, BlendChannel, false>(Base->GetData(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						default: check(false);
						}
					}
                    else if (!bDone)
                    {
                        switch (EBlendType(args.blendType))
                        {
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
                        case EBlendType::BT_SOFTLIGHT: BufferLayer<SoftLightChannel, false>(Base.get(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_HARDLIGHT: BufferLayer<HardLightChannel, false>(Base.get(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_BURN: BufferLayer<BurnChannel, false>(Base.get(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_DODGE: BufferLayer<DodgeChannel, false>(Base.get(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_SCREEN: BufferLayer<ScreenChannel, false>(Base.get(), Base.get(),  pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_OVERLAY: BufferLayer<OverlayChannel, false>(Base.get(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_LIGHTEN: BufferLayer<LightenChannel, false>(Base.get(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_MULTIPLY: BufferLayer<MultiplyChannel, false>(Base.get(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_BLEND: BufferLayer<BlendChannel, false>(Base.get(), Base.get(), pBlended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        default: check(false);
                        }
                    }

					// Apply the separate blend operation for alpha
					if (!bDone && !bApplyColorBlendToAlpha && args.blendTypeAlpha != uint8(EBlendType::BT_NONE) )
					{
						// Separate alpha operation ignores the mask.
						switch (EBlendType(args.blendTypeAlpha))
						{
						case EBlendType::BT_SOFTLIGHT: BufferLayerInPlace<SoftLightChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						case EBlendType::BT_HARDLIGHT: BufferLayerInPlace<HardLightChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						case EBlendType::BT_BURN: BufferLayerInPlace<BurnChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						case EBlendType::BT_DODGE: BufferLayerInPlace<DodgeChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						case EBlendType::BT_SCREEN: BufferLayerInPlace<ScreenChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						case EBlendType::BT_OVERLAY: BufferLayerInPlace<OverlayChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						case EBlendType::BT_LIGHTEN: BufferLayerInPlace<LightenChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						case EBlendType::BT_MULTIPLY: BufferLayerInPlace<MultiplyChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						case EBlendType::BT_BLEND: BufferLayerInPlace<BlendChannel, false, 1>(Base.get(), pBlended.get(), bBlendOnlyOneMip, 3, 3); break;
						default: check(false);
						}
					}
				}

				// Are we done?
				if (CurrentIteration + 1 == Iterations)
				{
					if (Data.MultiLayer.bBlendOnlyOneMip)
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageLayer_MipFix);
						FMipmapGenerationSettings DummyMipSettings{};
						ImageMipmapInPlace(m_pSettings->GetPrivate()->m_imageCompressionQuality, Base.get(), DummyMipSettings);
					}

					// TODO: Reconvert to OriginalBaseFormat if necessary?

					GetMemory().SetImage(item, Base);
					Data.Resource = nullptr;
					break;
				}
				else
				{
					// Request a new layer
					++CurrentIteration;
					FScheduledOp ItemCopy = item;
					ExecutionIndex Index = GetMemory().GetRageIndex(item.ExecutionIndex);
					Index.SetFromModelRangeIndex(args.rangeId, CurrentIteration);
					ItemCopy.ExecutionIndex = GetMemory().GetRageIndexIndex(Index);
					AddOp(FScheduledOp(item.At, item, 2+CurrentIteration, item.CustomState), FScheduledOp(args.blended, ItemCopy), FScheduledOp(args.mask, ItemCopy));

				}

                break;
            }

            } // switch stage

            break;
        }

        case OP_TYPE::IM_DIFFERENCE:
        {
			OP::ImageDifferenceArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageDifferenceArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.a, item),
                           FScheduledOp( args.b, item) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_DIFFERENCE_1)
            		
                // TODO: Reuse base if possible
                Ptr<const Image> pA = GetMemory().GetImage( FCacheAddress(args.a,item) );
                Ptr<const Image> pB = GetMemory().GetImage( FCacheAddress(args.b,item) );

                ImagePtr pResult = ImageDifference( pA.get(), pB.get() );

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

			break;
		}

		case OP_TYPE::IM_NORMALCOMPOSITE:
		{
			OP::ImageNormalCompositeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageNormalCompositeArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				if (args.base && args.normal)
				{
						AddOp(FScheduledOp(item.At, item, 1),
							  FScheduledOp(args.base, item),
							  FScheduledOp(args.normal, item));
					}
					else
					{
					GetMemory().SetImage(item, nullptr);
				}
				break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(IM_NORMALCOMPOSITE_1)

				Ptr<const Image> Base = GetMemory().GetImage(FCacheAddress(args.base, item));
				Ptr<const Image> Normal = GetMemory().GetImage(FCacheAddress(args.normal, item));

				if (Normal->GetLODCount() < Base->GetLODCount())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageNormalComposite_EmergencyFix);

					int levelCount = Base->GetLODCount();
					ImagePtr pDest = new Image(Normal->GetSizeX(), Normal->GetSizeY(),
						levelCount,
						Normal->GetFormat());

					SCRATCH_IMAGE_MIPMAP scratch;
					FMipmapGenerationSettings mipSettings{};
					ImageMipmap_PrepareScratch(pDest.get(), Normal.get(), levelCount, &scratch);
					ImageMipmap(m_pSettings->GetPrivate()->m_imageCompressionQuality,
						pDest.get(), Normal.get(), levelCount, &scratch, mipSettings);
					Normal = pDest;
				}


                ImagePtr pResult = new Image( Base->GetSizeX(), Base->GetSizeY(), Base->GetLODCount(), Base->GetFormat() );
				ImageNormalComposite(pResult.get(), Base.get(), Normal.get(), args.mode, args.power);

				GetMemory().SetImage(item, pResult);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case OP_TYPE::IM_PIXELFORMAT:
        {
			OP::ImagePixelFormatArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImagePixelFormatArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.source, item) );
                break;

            case 1:
            {
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_MIPMAP:
        {
			OP::ImageMipmapArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageMipmapArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
				AddOp( FScheduledOp( item.At, item, 1), FScheduledOp( args.source, item) );
                break;

            case 1:
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_RESIZE:
        {
			OP::ImageResizeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageResizeArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1), FScheduledOp( args.source, item) );
                break;

            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(IM_RESIZE_1)
            	
                Ptr<const Image> pBase = GetMemory().GetImage( FCacheAddress(args.source,item) );

				if (!pBase)
				{
					GetMemory().SetImage(item, nullptr);
					break;
				}

                FImageSize destSize = FImageSize
                    (
                        args.size[0],
                        args.size[1]
                    );

                ImagePtr pResult;

                if ( destSize[0]!=pBase->GetSizeX()
                     ||
                     destSize[1]!=pBase->GetSizeY() )
                {
                    //pResult = ImageResize( pBase.get(), destSize );
                    pResult = ImageResizeLinear( m_pSettings->GetPrivate()->m_imageCompressionQuality, pBase.get(), destSize );

                    // If the source image had mips, generate them as well for the resized image.
                    // This shouldn't happen often since "ResizeLike" should be usually optimised out
                    // during model compilation. The mipmap generation below is not very precise with
                    // the number of mips that are needed and will probably generate too many
                    bool sourceHasMips = pBase->GetLODCount()>1;
                    if (sourceHasMips)
                    {
                        int levelCount = Image::GetMipmapCount( pResult->GetSizeX(), pResult->GetSizeY() );
                        ImagePtr pMipmapped = new Image( pResult->GetSizeX(), pResult->GetSizeY(),
                                                         levelCount,
                                                         pResult->GetFormat() );

                        SCRATCH_IMAGE_MIPMAP scratch;
						FMipmapGenerationSettings mipSettings{};

                        ImageMipmap_PrepareScratch( pMipmapped.get(), pResult.get(), levelCount, &scratch );
                        ImageMipmap( m_pSettings->GetPrivate()->m_imageCompressionQuality,
                                     pMipmapped.get(), pResult.get(), levelCount, &scratch, mipSettings );

                        pResult = pMipmapped;
                    }

                }
                else
                {
                    pResult = pBase->Clone();
                }

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_RESIZELIKE:
        {
			OP::ImageResizeLikeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageResizeLikeArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.source, item),
                           FScheduledOp( args.sizeSource, item) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_RESIZELIKE_1)
            	
                Ptr<const Image> pBase = GetMemory().GetImage( FCacheAddress(args.source,item) );
                Ptr<const Image> pSizeBase = GetMemory().GetImage( FCacheAddress(args.sizeSource,item) );
                ImagePtr pResult;

                if ( pBase->GetSizeX()!=pSizeBase->GetSizeX()
                     ||
                     pBase->GetSizeY()!=pSizeBase->GetSizeY() )
                {
                    FImageSize destSize( pSizeBase->GetSizeX(), pSizeBase->GetSizeY() );

                    pResult = ImageResize( pBase.get(), destSize );

                    // If the source image had mips, generate them as well for the resized image.
                    // This shouldn't happen often since "ResizeLike" should be usually optimised out
                    // during model compilation. The mipmap generation below is not very precise with
                    // the number of mips that are needed and will probably generate too many
                    bool sourceHasMips = pBase->GetLODCount()>1;
                    if (sourceHasMips)
                    {
                        int levelCount = Image::GetMipmapCount( pResult->GetSizeX(), pResult->GetSizeY() );
                        ImagePtr pMipmapped = new Image( pResult->GetSizeX(), pResult->GetSizeY(),
                                                         levelCount,
                                                         pResult->GetFormat() );

                        SCRATCH_IMAGE_MIPMAP scratch;
						FMipmapGenerationSettings mipSettings{};

                        ImageMipmap_PrepareScratch( pMipmapped.get(), pResult.get(), levelCount, &scratch );
                        ImageMipmap( m_pSettings->GetPrivate()->m_imageCompressionQuality,
                                     pMipmapped.get(), pResult.get(), levelCount, &scratch, mipSettings );

                        pResult = pMipmapped;
                    }
                }
                else
                {
                    pResult = pBase->Clone();
                }

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_RESIZEREL:
        {
			OP::ImageResizeRelArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageResizeRelArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.source, item) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_RESIZEREL_1)
            	
                Ptr<const Image> pBase = GetMemory().GetImage( FCacheAddress(args.source,item) );

                FImageSize destSize(
                            uint16( FMath::Max(1.0, pBase->GetSizeX()*args.factor[0] + 0.5f ) ),
                            uint16( FMath::Max(1.0, pBase->GetSizeY()*args.factor[1] + 0.5f ) ) );

                //pResult = ImageResize( pBase.get(), destSize );
                ImagePtr pResult = ImageResizeLinear(
                    m_pSettings->GetPrivate()->m_imageCompressionQuality, pBase.get(), destSize );

                // If the source image had mips, generate them as well for the resized image.
                // This shouldn't happen often since "ResizeLike" should be usually optimised out
                // during model compilation. The mipmap generation below is not very precise with
                // the number of mips that are needed and will probably generate too many
                bool sourceHasMips = pBase->GetLODCount()>1;
                if (sourceHasMips)
                {
                    int levelCount = Image::GetMipmapCount( pResult->GetSizeX(), pResult->GetSizeY() );
                    ImagePtr pMipmapped = new Image( pResult->GetSizeX(), pResult->GetSizeY(),
                                                     levelCount,
                                                     pResult->GetFormat() );

                    SCRATCH_IMAGE_MIPMAP scratch;
					FMipmapGenerationSettings mipSettings{};

                    ImageMipmap_PrepareScratch( pMipmapped.get(), pResult.get(), levelCount, &scratch );
                    ImageMipmap( m_pSettings->GetPrivate()->m_imageCompressionQuality,
                                 pMipmapped.get(), pResult.get(), levelCount, &scratch, mipSettings );

                    pResult = pMipmapped;
                }

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }


            break;
        }

        case OP_TYPE::IM_BLANKLAYOUT:
        {
			OP::ImageBlankLayoutArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageBlankLayoutArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp::FromOpAndOptions( args.layout, item, 0) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_BLANKLAYOUT_1)
            		
                Ptr<const Layout> pLayout = GetMemory().GetLayout(FScheduledOp::FromOpAndOptions(args.layout, item, 0));

                FIntPoint SizeInBlocks = pLayout->GetGridSize();

				FIntPoint BlockSizeInPixels(args.blockSize[0], args.blockSize[1]);

				// Image size if we don't skip any mipmap
				FIntPoint FullImageSizeInPixels = SizeInBlocks * BlockSizeInPixels;
				int32 FullImageMipCount = Image::GetMipmapCount(FullImageSizeInPixels.X, FullImageSizeInPixels.Y);

				FIntPoint ImageSizeInPixels = FullImageSizeInPixels;
				int32 MipsToSkip = item.ExecutionOptions;
				MipsToSkip = FMath::Min(MipsToSkip, FullImageMipCount);
				if (MipsToSkip > 0)
				{
					//FIntPoint ReducedBlockSizeInPixels;

					// This method tries to reduce only the block size, but it fails if the image is still too big
					// If we want to generate only a subset of mipmaps, reduce the layout block size accordingly.
					//ReducedBlockSizeInPixels.X = BlockSizeInPixels.X >> MipsToSkip;
					//ReducedBlockSizeInPixels.Y = BlockSizeInPixels.Y >> MipsToSkip;
					//const FImageFormatData& FormatData = GetImageFormatData((EImageFormat)args.format);
					//int MinBlockSize = FMath::Max(FormatData.m_pixelsPerBlockX, FormatData.m_pixelsPerBlockY);
					//ReducedBlockSizeInPixels.X = FMath::Max<int32>(ReducedBlockSizeInPixels.X, FormatData.m_pixelsPerBlockX);
					//ReducedBlockSizeInPixels.Y = FMath::Max<int32>(ReducedBlockSizeInPixels.Y, FormatData.m_pixelsPerBlockY);
					//FIntPoint ReducedImageSizeInPixels = SizeInBlocks * ReducedBlockSizeInPixels;

					// This method simply reduces the size and assumes all the other operations will handle degeenrate cases.
					ImageSizeInPixels = FullImageSizeInPixels / (1 << MipsToSkip);
					
					//if (ReducedImageSizeInPixels!= ImageSizeInPixels)
					//{
					//	check(false);
					//}
				}

                int MipsToGenerate = 1;
                if ( args.generateMipmaps )
                {
                    if ( args.mipmapCount==0 )
                    {
						MipsToGenerate = Image::GetMipmapCount(ImageSizeInPixels.X, ImageSizeInPixels.Y);
                    }
                    else
                    {
						MipsToGenerate = FMath::Max(args.mipmapCount-MipsToSkip,1);
                    }
                }

                ImagePtr pNew = new Image(ImageSizeInPixels.X, ImageSizeInPixels.Y, MipsToGenerate, EImageFormat(args.format) );

                FMemory::Memzero( pNew->GetData(), pNew->GetDataSize() );

                GetMemory().SetImage( item, pNew );
                break;
            }

            default:
                check(false);
            }


            break;
        }

        case OP_TYPE::IM_COMPOSE:
        {
			OP::ImageComposeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageComposeArgs>( item.At );
            switch ( item.Stage )
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1 ), 
					FScheduledOp::FromOpAndOptions( args.layout, item, 0 ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_COMPOSE_1)
            		
                Ptr<const Layout> ComposeLayout = GetMemory().GetLayout( FCacheAddress( args.layout, FScheduledOp::FromOpAndOptions(args.layout, item, 0)) );

                FScheduledOpData data;
                data.Resource = const_cast<Layout*>(ComposeLayout.get());
				int32 dataPos = m_heapData.Add( data );

                int relBlockIndex = ComposeLayout->FindBlock( args.blockIndex );
                if ( relBlockIndex >= 0 )
                {
                    AddOp( FScheduledOp( item.At, item, 2, dataPos ),
                           FScheduledOp( args.base, item ),
                           FScheduledOp( args.blockImage, item ),
                           FScheduledOp( args.mask, item ) );
                }
                else
                {
                    AddOp( FScheduledOp( item.At, item, 2, dataPos ),
                           FScheduledOp( args.base, item ) );
                }
                break;
            }

            case 2:
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
                break;

            default:
                check( false );
            }

            break;
        }

        case OP_TYPE::IM_INTERPOLATE:
        {
			OP::ImageInterpolateArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageInterpolateArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1),
                       FScheduledOp( args.factor, item) );
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_INTERPOLATE_1)
            	
                // Targets must be consecutive
                int count = 0;
                for ( int i=0
                    ; i<MUTABLE_OP_MAX_INTERPOLATE_COUNT && args.targets[i]
                    ; ++i )
                {
                    count++;
                }

                float factor = GetMemory().GetScalar( FCacheAddress(args.factor,item) );

                float delta = 1.0f/(count-1);
                int min = (int)floorf( factor/delta );
                int max = (int)ceilf( factor/delta );

                float bifactor = factor/delta - min;

                FScheduledOpData data;
                data.Interpolate.Bifactor = bifactor;
				data.Interpolate.Min = FMath::Clamp(min, 0, count - 1);
				data.Interpolate.Max = FMath::Clamp(max, 0, count - 1);
				uint32 dataPos = uint32(m_heapData.Add(data));

                if ( bifactor < UE_SMALL_NUMBER )
                {
                        AddOp( FScheduledOp( item.At, item, 2, dataPos),
                               FScheduledOp( args.targets[min], item) );
                    }
                else if ( bifactor > 1.0f-UE_SMALL_NUMBER )
                {
                        AddOp( FScheduledOp( item.At, item, 2, dataPos),
                               FScheduledOp( args.targets[max], item) );
                    }
                    else
                    {
                        AddOp( FScheduledOp( item.At, item, 2, dataPos),
                               FScheduledOp( args.targets[min], item),
                               FScheduledOp( args.targets[max], item) );
                    }
                break;
            }

            case 2:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_INTERPOLATE_2)
            		
                // Targets must be consecutive
                int count = 0;
                for ( int i=0
                    ; i<MUTABLE_OP_MAX_INTERPOLATE_COUNT && args.targets[i]
                    ; ++i )
                {
                    count++;
                }

                // Factor from 0 to 1 between the two targets
                const FScheduledOpData& data = m_heapData[(size_t)item.CustomState];
                float bifactor = data.Interpolate.Bifactor;
                int min = data.Interpolate.Min;
                int max = data.Interpolate.Max;

                ImagePtr pResult;
                if ( bifactor < UE_SMALL_NUMBER )
                {
                    Ptr<const Image> pSource = GetMemory().GetImage( FCacheAddress(args.targets[min],item) );
                    pResult = pSource->Clone();
                }
                else if ( bifactor > 1.0f-UE_SMALL_NUMBER )
                {
                    Ptr<const Image> pSource = GetMemory().GetImage( FCacheAddress(args.targets[max],item) );
                    pResult = pSource->Clone();
                }
                else
                {
                    Ptr<const Image> pMin = GetMemory().GetImage( FCacheAddress(args.targets[min],item) );
                    Ptr<const Image> pMax = GetMemory().GetImage( FCacheAddress(args.targets[max],item) );

                    if (pMin && pMax)
                    {
						int32 LevelCount = FMath::Max(pMin->GetLODCount(), pMax->GetLODCount());
						
						ImagePtr pNew = new Image( pMin->GetSizeX(), pMin->GetSizeY(), LevelCount, pMin->GetFormat() );

						// Be defensive: ensure image sizes match.
						if (pMin->GetSize() != pMax->GetSize())
						{
							MUTABLE_CPUPROFILER_SCOPE(ImageResize_ForInterpolate);
							pMax = ImageResizeLinear(0, pMax.get(), pMin->GetSize());
						}

						if (pMin->GetLODCount() != LevelCount)
						{
							MUTABLE_CPUPROFILER_SCOPE(Mipmap_ForInterpolate);

							ImagePtr pDest = new Image(pMin->GetSizeX(), pMin->GetSizeY(), LevelCount, pMin->GetFormat());

							SCRATCH_IMAGE_MIPMAP scratch;
							FMipmapGenerationSettings settings{};

							ImageMipmap_PrepareScratch(pDest.get(), pMin.get(), LevelCount, &scratch);
							ImageMipmap(m_pSettings->GetPrivate()->m_imageCompressionQuality, pDest.get(), pMin.get(), LevelCount,
								&scratch, settings);

							pMin = pDest;
						}

						if (pMax->GetLODCount() != LevelCount)
						{
							MUTABLE_CPUPROFILER_SCOPE(Mipmap_ForInterpolate);

							ImagePtr pDest = new Image(pMax->GetSizeX(), pMax->GetSizeY(), LevelCount, pMax->GetFormat());

							SCRATCH_IMAGE_MIPMAP scratch;
							FMipmapGenerationSettings settings{};

							ImageMipmap_PrepareScratch(pDest.get(), pMax.get(), LevelCount, &scratch);
							ImageMipmap(m_pSettings->GetPrivate()->m_imageCompressionQuality, pDest.get(), pMax.get(), LevelCount,
								&scratch, settings);

							pMax = pDest;
						}


                        ImageInterpolate( pNew.get(), pMin.get(), pMax.get(), bifactor );

                        pResult = pNew;
                    }
                    else if (pMin)
                    {
                        pResult = pMin->Clone();
                    }
                    else if (pMax)
                    {
                        pResult = pMax->Clone();
                    }
                }

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_INTERPOLATE3:
        {
			OP::ImageInterpolate3Args args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageInterpolate3Args>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp::FromOpAndOptions( args.factor1, item, 0),
                           FScheduledOp::FromOpAndOptions( args.factor2, item, 0),
                           FScheduledOp( args.target0, item),
                           FScheduledOp( args.target1, item),
                           FScheduledOp( args.target2, item) );
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_INTERPOLATE3_1)

                // \TODO Optimise limit cases

                float factor1 = GetMemory().GetScalar(FScheduledOp::FromOpAndOptions(args.factor1, item, 0));
                float factor2 = GetMemory().GetScalar(FScheduledOp::FromOpAndOptions(args.factor2, item, 0));

                Ptr<const Image> pTarget0 = GetMemory().GetImage( FCacheAddress(args.target0,item) );
                Ptr<const Image> pTarget1 = GetMemory().GetImage( FCacheAddress(args.target1,item) );
                Ptr<const Image> pTarget2 = GetMemory().GetImage( FCacheAddress(args.target2,item) );

                ImagePtr pResult = ImageInterpolate( pTarget0.get(), pTarget1.get(), pTarget2.get(),
                                                     factor1, factor2 );

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_SATURATE:
        {
			OP::ImageSaturateArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageSaturateArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.base, item ),
                           FScheduledOp::FromOpAndOptions( args.factor, item, 0 ));
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_SATURATE_1)
           		
                Ptr<const Image> pBase = GetMemory().GetImage( FCacheAddress(args.base,item) );
                float factor = GetMemory().GetScalar(FScheduledOp::FromOpAndOptions(args.factor, item, 0));

                ImagePtr pResult = ImageSaturate( pBase.get(), factor );

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_LUMINANCE:
        {
			OP::ImageLuminanceArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageLuminanceArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1),
                        FScheduledOp( args.base, item ) );
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_LUMINANCE_1)
            		
                Ptr<const Image> pBase = GetMemory().GetImage( FCacheAddress(args.base,item) );

                ImagePtr pResult = ImageLuminance( pBase.get() );
                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_SWIZZLE:
        {
			OP::ImageSwizzleArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageSwizzleArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1),
                        FScheduledOp( args.sources[0], item ),
                        FScheduledOp( args.sources[1], item ),
                        FScheduledOp( args.sources[2], item ),
                        FScheduledOp( args.sources[3], item ) );
                break;

            case 1:
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_SELECTCOLOUR:
        {
			OP::ImageSelectColourArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageSelectColourArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1),
                        FScheduledOp( args.base, item ),
                        FScheduledOp::FromOpAndOptions( args.colour, item, 0 ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_SELECTCOLOUR_1)
            		
                Ptr<const Image> pBase = GetMemory().GetImage( FCacheAddress(args.base,item) );
				FVector4f colour = GetMemory().GetColour(FScheduledOp::FromOpAndOptions(args.colour, item, 0));

				Ptr<Image> pResult = ImageSelectColour( pBase.get(), vec3f(colour) );

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_COLOURMAP:
        {
			OP::ImageColourMapArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageColourMapArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.base, item ),
                           FScheduledOp( args.mask, item ),
                           FScheduledOp( args.map, item ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_COLOURMAP_1)
            		
                Ptr<const Image> pSource = GetMemory().GetImage( FCacheAddress(args.base,item) );
                Ptr<const Image> pMask = GetMemory().GetImage( FCacheAddress(args.mask,item) );
                Ptr<const Image> pMap = GetMemory().GetImage( FCacheAddress(args.map,item) );

				// Be defensive: ensure image sizes match.
				if (pMask->GetSize() != pSource->GetSize())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageResize_ForColourmap);
					pMask = ImageResizeLinear(0, pMask.get(), pSource->GetSize());
				}


                ImagePtr pResult = ImageColourMap( pSource.get(), pMask.get(), pMap.get() );

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_GRADIENT:
        {
			OP::ImageGradientArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageGradientArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp::FromOpAndOptions( args.colour0, item, 0 ),
                           FScheduledOp::FromOpAndOptions( args.colour1, item, 0 ) );
                break;

            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(IM_GRADIENT_1)
            		
				FVector4f colour0 = GetMemory().GetColour(FScheduledOp::FromOpAndOptions(args.colour0, item, 0));
				FVector4f colour1 = GetMemory().GetColour(FScheduledOp::FromOpAndOptions(args.colour1, item, 0));

                ImagePtr pResult = ImageGradient( colour0, colour1,
                                                  args.size[0],
                                                  args.size[1] );

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_BINARISE:
        {
			OP::ImageBinariseArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageBinariseArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.base, item ),
                           FScheduledOp::FromOpAndOptions( args.threshold, item, 0 ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_BINARISE_1)
            		
                Ptr<const Image> pA = GetMemory().GetImage( FCacheAddress(args.base,item) );

                float c = GetMemory().GetScalar(FScheduledOp::FromOpAndOptions(args.threshold, item, 0));

                ImagePtr pResult = ImageBinarise( pA.get(), c );

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case OP_TYPE::IM_INVERT:
		{
			OP::ImageInvertArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageInvertArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.base, item));
				break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(IM_INVERT_1)
					
				Ptr<const Image> pA = GetMemory().GetImage(FCacheAddress(args.base, item));

				ImagePtr pResult;
				if (pA->IsUnique())
				{
					pResult = mu::CloneOrTakeOver<>(pA.get());
					ImageInvertInPlace(pResult.get());
				}
				else
				{
					pResult = ImageInvert(pA.get());
				}

				GetMemory().SetImage(item, pResult);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case OP_TYPE::IM_PLAINCOLOUR:
        {
			OP::ImagePlainColourArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImagePlainColourArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
				AddOp( FScheduledOp( item.At, item, 1),
					FScheduledOp::FromOpAndOptions( args.colour, item, 0 ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_PLAINCOLOUR_1)
            		
				FVector4f c = GetMemory().GetColour(FScheduledOp::FromOpAndOptions(args.colour, item, 0));

				uint16 SizeX = args.size[0];
				uint16 SizeY = args.size[1];
				for (int l=0; l<item.ExecutionOptions; ++l)
				{
					SizeX = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeX, uint16(2)));
					SizeY = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeY, uint16(2)));
				}

                ImagePtr pA = new Image( SizeX,
                                         SizeY,
                                         1,
                                         (EImageFormat)args.format );

                FillPlainColourImage(pA.get(), c);

                GetMemory().SetImage( item, pA );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_GPU:
        {
            check(false);
            break;
        }

        case OP_TYPE::IM_CROP:
        {
			OP::ImageCropArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageCropArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1),
					FScheduledOp( args.source, item ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_CROP_1)
            		
                Ptr<const Image> pA = GetMemory().GetImage( FCacheAddress(args.source,item) );

                box< vec2<int> > rect;
                rect.min[0] = args.minX;
                rect.min[1] = args.minY;
                rect.size[0] = args.sizeX;
                rect.size[1] = args.sizeY;

				// Apply ther mipmap reduction to the crop rectangle.
				int32 MipsToSkip = item.ExecutionOptions;
				while ( MipsToSkip>0 && rect.size[0]>0 && rect.size[1]>0 )
				{
					rect.ShrinkToHalf();
					MipsToSkip--;
				}

				ImagePtr pResult;
				if (!rect.IsEmpty())
				{
					pResult = ImageCrop(m_pSettings->GetPrivate()->m_imageCompressionQuality, pA.get(), rect);
				}

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_PATCH:
        {
			OP::ImagePatchArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImagePatchArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1),
					FScheduledOp( args.base, item ),
					FScheduledOp( args.patch, item ) );
                break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_PATCH_1)

                Ptr<const Image> pA = GetMemory().GetImage( FCacheAddress(args.base,item) );
                Ptr<const Image> pB = GetMemory().GetImage( FCacheAddress(args.patch,item) );

				// Failsafe
				if (!pA || !pB)
				{
					GetMemory().SetImage(item, pA);
					break;
				}

                box<vec2<int>> rect;
                rect.min[0] = args.minX;
                rect.min[1] = args.minY;
                rect.size[0] = pB->GetSizeX();
                rect.size[1] = pB->GetSizeY();

				// Apply ther mipmap reduction to the crop rectangle.
				int32 MipsToSkip = item.ExecutionOptions;
				while (MipsToSkip > 0 && rect.size[0] > 0 && rect.size[1] > 0)
				{
					rect.min/=2;
					MipsToSkip--;
				}

                ImagePtr pResult = pA->Clone();

				bool bApplyPatch = !rect.IsEmpty();
				if (bApplyPatch)
				{
					// Resize image if it doesn't fit in the new block size
					if (pB->GetSizeX() != rect.size[0] ||
						pB->GetSizeY() != rect.size[1])
					{
						MUTABLE_CPUPROFILER_SCOPE(ImagePatchResize_Emergency);

						FImageSize blockSize((uint16)rect.size[0], (uint16)rect.size[1]);
						pB = ImageResizeLinear(m_pSettings->GetPrivate()->m_imageCompressionQuality,pB.get(), blockSize);
					}

					// Change the block image format if it doesn't match the composed image
					// This is usually enforced at object compilation time.
					if (pResult->GetFormat() != pB->GetFormat())
					{
						MUTABLE_CPUPROFILER_SCOPE(ImagPatcheReformat);

						EImageFormat format = GetMostGenericFormat(pResult->GetFormat(), pB->GetFormat());

						const FImageFormatData& finfo = GetImageFormatData(format);
						if (finfo.m_pixelsPerBlockX == 0)
						{
							format = GetUncompressedFormat(format);
						}

						if (pResult->GetFormat() != format)
						{
							pResult = ImagePixelFormat(m_pSettings->GetPrivate()->m_imageCompressionQuality, pResult.get(), format);
						}
						if (pB->GetFormat() != format)
						{
							pB = ImagePixelFormat(m_pSettings->GetPrivate()->m_imageCompressionQuality, pB.get(), format);
						}
					}

					// Don't patch if below the image compression block size.
					const FImageFormatData& finfo = GetImageFormatData(pResult->GetFormat());
					bApplyPatch =
						(rect.min[0] % finfo.m_pixelsPerBlockX == 0) &&
						(rect.min[1] % finfo.m_pixelsPerBlockY == 0) &&
						(rect.size[0] % finfo.m_pixelsPerBlockX == 0) &&
						(rect.size[1] % finfo.m_pixelsPerBlockY == 0) &&
						(rect.min[0] + rect.size[0]) <= pResult->GetSizeX() &&
						(rect.min[1] + rect.size[1]) <= pResult->GetSizeY()
						;
				}

				if (bApplyPatch)
				{
					ImageCompose(pResult.get(), pB.get(), rect);
					pResult->m_flags = 0;
				}
				else
				{
					// This happens very often when skipping mips, and floods the log.
					//UE_LOG( LogMutableCore, Verbose, TEXT("Skipped patch operation for image not fitting the block compression size. Small image? Patch rect is (%d, %d), (%d, %d), base is (%d, %d)"),
					//	rect.min[0], rect.min[1], rect.size[0], rect.size[1], pResult->GetSizeX(), pResult->GetSizeY());
				}

                GetMemory().SetImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_RASTERMESH:
        {
			OP::ImageRasterMeshArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageRasterMeshArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    if (!args.image)
                    {
                        AddOp( FScheduledOp( item.At, item, 1),
                            FScheduledOp::FromOpAndOptions( args.mesh, item, 0 ) );
                    }
                    else
                    {
                        AddOp( FScheduledOp( item.At, item, 1),
							FScheduledOp::FromOpAndOptions( args.mesh, item, 0),
							FScheduledOp( args.image, item ),
							FScheduledOp( args.mask, item ),
							FScheduledOp::FromOpAndOptions( args.angleFadeProperties, item, 0 ),
							FScheduledOp::FromOpAndOptions( args.projector, item, 0 ) );
                    }
                break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_RASTERMESH_1)

                Ptr<const Mesh> pMesh = GetMemory().GetMesh( FScheduledOp::FromOpAndOptions(args.mesh, item, 0) );

                ImagePtr pNew;

				uint16 SizeX = args.sizeX;
				uint16 SizeY = args.sizeY;

				// Drop mips while possible
				int32 MipsToDrop = item.ExecutionOptions;
				while (MipsToDrop && !(SizeX % 2) && !(SizeY % 2))
				{
					SizeX = FMath::Max(uint16(1),FMath::DivideAndRoundUp(SizeX, uint16(2)));
					SizeY = FMath::Max(uint16(1),FMath::DivideAndRoundUp(SizeY, uint16(2)));
					--MipsToDrop;
				}

                if (!pMesh.get())
                {
                    pNew = new Image(SizeX, SizeY, 1, EImageFormat::IF_L_UBYTE);
                    check(false);
                }
                else if (args.image)
                {
                    // Raster with projection
                    Ptr<const Image> pSource = GetMemory().GetImage( FCacheAddress(args.image,item) );

                    Ptr<const Image> pMask = nullptr;
                    if ( args.mask )
                    {
                        pMask = GetMemory().GetImage( FCacheAddress(args.mask,item) );

						// TODO: This shouldn't happen, but be defensive.
						FImageSize ResultSize(SizeX, SizeY);
						if (pMask && pMask->GetSize()!= ResultSize)
						{
							MUTABLE_CPUPROFILER_SCOPE(ImageResize_MaskFixForProjection);
							pMask = ImageResizeLinear(0, pMask.get(), ResultSize);
						}
                    }

                    float fadeStart = 180.0f;
                    float fadeEnd = 180.0f;
                    if ( args.angleFadeProperties )
                    {
                        FVector4f fadeProperties = GetMemory().GetColour(FScheduledOp::FromOpAndOptions(args.angleFadeProperties, item, 0));
                        fadeStart = fadeProperties[0];
                        fadeEnd = fadeProperties[1];
                    }
                    fadeStart *= PI / 180.0f;
                    fadeEnd *= PI / 180.0f;

					EImageFormat format = pSource ? GetUncompressedFormat(pSource->GetFormat()) : EImageFormat::IF_L_UBYTE;
                    pNew = new Image( SizeX, SizeY, 1, format );

                    if (pSource && pSource->GetFormat()!=format)
                    {
						MUTABLE_CPUPROFILER_SCOPE(RunCode_RasterMesh_ReformatSource);
                        pSource = ImagePixelFormat( m_pSettings->GetPrivate()->m_imageCompressionQuality, pSource.get(), format );
                    }

                    // Allocate memory for the temporary buffers
                    SCRATCH_IMAGE_PROJECT scratch;
                    scratch.vertices.SetNum( pMesh->GetVertexCount() );
                    scratch.culledVertex.SetNum( pMesh->GetVertexCount() );

                    // Layout is always 0 because the previous mesh project operations take care of
                    // moving the right layout channel to the 0.
                    int layout = 0;

                    if ( args.projector && pSource && pSource->GetSizeX()>0 && pSource->GetSizeY()>0 )
                    {
                        Ptr<const Projector> pProjector = GetMemory().GetProjector(FScheduledOp::FromOpAndOptions(args.projector, item, 0));
                        if (pProjector)
                        {
                            switch (pProjector->m_value.type)
                            {
                            case PROJECTOR_TYPE::PLANAR:
                                ImageRasterProjectedPlanar( pMesh.get(), pNew.get(),
                                                            pSource.get(), pMask.get(),
															args.bIsRGBFadingEnabled, args.bIsAlphaFadingEnabled,
                                                            fadeStart, fadeEnd,
                                                            layout, args.blockIndex,
                                                            &scratch );
                                break;

                            case PROJECTOR_TYPE::WRAPPING:
                                ImageRasterProjectedWrapping( pMesh.get(), pNew.get(),
                                                              pSource.get(), pMask.get(),
															  args.bIsRGBFadingEnabled, args.bIsAlphaFadingEnabled,
															  fadeStart, fadeEnd,
                                                              layout, args.blockIndex,
                                                              &scratch );
                                break;

                            case PROJECTOR_TYPE::CYLINDRICAL:
                                ImageRasterProjectedCylindrical( pMesh.get(), pNew.get(),
                                                                 pSource.get(), pMask.get(),
																 args.bIsRGBFadingEnabled, args.bIsAlphaFadingEnabled,
																 fadeStart, fadeEnd,
                                                                 layout,
																 pProjector->m_value.projectionAngle,
                                                                 &scratch );
                                break;

                            default:
                                check(false);
                                break;
                            }
                        }
                    }
                }
                else
                {
                    // Flat mesh UV raster
                    pNew = new Image( SizeX, SizeY, 1, EImageFormat::IF_L_UBYTE );
                    ImageRasterMesh( pMesh.get(), pNew.get(), args.blockIndex );
                }

                GetMemory().SetImage( item, pNew );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_MAKEGROWMAP:
        {
			OP::ImageMakeGrowMapArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageMakeGrowMapArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.mask, item) );
                break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_MAKEGROWMAP_1)

                Ptr<const Image> pMask = GetMemory().GetImage( FCacheAddress(args.mask,item) );

                ImagePtr pNew = new Image( pMask->GetSizeX(), pMask->GetSizeY(), pMask->GetLODCount(), EImageFormat::IF_L_UBYTE);

                ImageMakeGrowMap( pNew.get(), pMask.get(), args.border );
				pNew->m_flags |= Image::IF_CANNOT_BE_SCALED;

                GetMemory().SetImage( item, pNew );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_DISPLACE:
        {
			OP::ImageDisplaceArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageDisplaceArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.source, item ),
                           FScheduledOp( args.displacementMap, item ) );
                break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_DISPLACE_1)

                Ptr<const Image> pSource = GetMemory().GetImage( FCacheAddress(args.source,item) );
                Ptr<const Image> pMap = GetMemory().GetImage( FCacheAddress(args.displacementMap,item) );

				// TODO: This shouldn't happen: displacement maps cannot be scaled because their information
				// is resolution sensitive (pixel offsets). If the size doesn't match, scale the source, apply 
				// displacement and then unscale it.
				FImageSize OriginalSourceScale = pSource->GetSize();
				if (OriginalSourceScale.x()>0 && OriginalSourceScale.y()>0 && OriginalSourceScale != pMap->GetSize())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyHackForDisplacementStep1);
					pSource = ImageResizeLinear(0, pSource.get(), pMap->GetSize());
				}

				// This works based on the assumption that displacement maps never read from a position they actually write to.
				// Since they are used for UV border expansion, this should always be the case.
				//ImagePtr pNew = new Image(pSource->GetSizeX(), pSource->GetSizeY(), 1, pSource->GetFormat());
				Ptr<Image> pNew = mu::CloneOrTakeOver(pSource.get());

				if (OriginalSourceScale.x() > 0 && OriginalSourceScale.y() > 0)
				{
					ImageDisplace(pNew.get(), pSource.get(), pMap.get());

					if (OriginalSourceScale != pNew->GetSize())
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyHackForDisplacementStep2);
						pNew = ImageResizeLinear(0, pNew.get(), OriginalSourceScale);
					}
				}

                GetMemory().SetImage( item, pNew );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_TRANSFORM:
        {
            const OP::ImageTransformArgs Args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageTransformArgs>(item.At);

            switch (item.Stage)
            {
            case 0:
			{
				const TArray<FScheduledOp, TInlineAllocator<6>> Deps = {
						FScheduledOp( Args.base, item),
						FScheduledOp( Args.offsetX, item),
						FScheduledOp( Args.offsetY, item),
						FScheduledOp( Args.scaleX, item),
						FScheduledOp( Args.scaleY, item),
						FScheduledOp( Args.rotation, item) };

                AddOp( FScheduledOp( item.At, item, 1), Deps );

				break;
			}
            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_TRANSFORM_1)
            		
                Ptr<const Image> pBaseImage = GetMemory().GetImage( FCacheAddress(Args.base, item) );
                
                const FVector2f Offset = FVector2f(
                        Args.offsetX ? GetMemory().GetScalar( FCacheAddress(Args.offsetX, item) ) : 0.0f,
                        Args.offsetY ? GetMemory().GetScalar( FCacheAddress(Args.offsetY, item) ) : 0.0f );

                const FVector2f Scale = FVector2f(
                        Args.scaleX ? GetMemory().GetScalar( FCacheAddress(Args.scaleX, item) ) : 1.0f,
                        Args.scaleY ? GetMemory().GetScalar( FCacheAddress(Args.scaleY, item) ) : 1.0f );

				// Map Range 0-1 to a full rotation
                const float Rotation = GetMemory().GetScalar( FCacheAddress(Args.rotation, item) ) * UE_TWO_PI;


				Ptr<Image> pResult = new Image( pBaseImage->GetSizeX(), pBaseImage->GetSizeY(), 1, pBaseImage->GetFormat());
			
				FImageSize SampleImageSize =  FImageSize( 
					static_cast<uint16>( FMath::Clamp( FMath::FloorToInt( float(pBaseImage->GetSizeX()) * FMath::Abs(Scale.X)), 2, pBaseImage->GetSizeX() ) ),
					static_cast<uint16>( FMath::Clamp( FMath::FloorToInt( float(pBaseImage->GetSizeY()) * FMath::Abs(Scale.Y)), 2, pBaseImage->GetSizeY() ) ) );
	
				Ptr<Image> pSampleImage = ImageResizeLinear( 0, pBaseImage.get(), SampleImageSize );
                ImageTransform( pResult.get(), pSampleImage.get(), Offset, Scale, Rotation );

                GetMemory().SetImage( item, pResult );

                break;
            }

            default:
                check(false);
            }

			break;
		}

        default:
            if (type!=OP_TYPE::NONE)
            {
                // Operation not implemented
                check( false );
            }
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    Ptr<RangeIndex> CodeRunner::BuildCurrentOpRangeIndex( const FScheduledOp& item, const Parameters* pParams, const Model* pModel, int32 parameterIndex )
    {
        if (!item.ExecutionIndex)
        {
            return nullptr;
        }

        // \todo: optimise to avoid allocating the index here, we could access internal
        // data directly.
		Ptr<RangeIndex> index = pParams->NewRangeIndex( parameterIndex );
        if (!index)
        {
            return nullptr;
        }

        const FProgram& program = pModel->GetPrivate()->m_program;
        const FParameterDesc& paramDesc = program.m_parameters[ parameterIndex ];
        for( size_t rangeIndexInParam=0;
             rangeIndexInParam<paramDesc.m_ranges.Num();
             ++rangeIndexInParam )
        {
            uint32 rangeIndexInModel = paramDesc.m_ranges[rangeIndexInParam];
            const ExecutionIndex& currentIndex = GetMemory().GetRageIndex( item.ExecutionIndex );
            int position = currentIndex.GetFromModelRangeIndex(rangeIndexInModel);
            index->GetPrivate()->m_values[rangeIndexInParam] = position;
        }

        return index;
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Bool( FScheduledOp& item,
                                   const Parameters* pParams,
                                   const Model* pModel
                                   )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Bool);

        const FProgram& program = pModel->GetPrivate()->m_program;
        OP_TYPE type = program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::BO_CONSTANT:
        {
			OP::BoolConstantArgs args = program.GetOpArgs<OP::BoolConstantArgs>(item.At);
            bool result = args.value;
            GetMemory().SetBool( item, result );
            break;
        }

        case OP_TYPE::BO_PARAMETER:
        {
			OP::ParameterArgs args = program.GetOpArgs<OP::ParameterArgs>(item.At);
            bool result = false;
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            result = pParams->GetBoolValue( args.variable, index );
            GetMemory().SetBool( item, result );
            break;
        }

        case OP_TYPE::BO_LESS:
        {
			OP::BoolLessArgs args = program.GetOpArgs<OP::BoolLessArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.a, item),
                           FScheduledOp( args.b, item) );
                break;

            case 1:
            {
                float a = GetMemory().GetScalar( FCacheAddress(args.a,item) );
                float b = GetMemory().GetScalar( FCacheAddress(args.b,item) );
                bool result = a<b;
                GetMemory().SetBool( item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::BO_AND:
        {
			OP::BoolBinaryArgs args = program.GetOpArgs<OP::BoolBinaryArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                {
                    // Try to avoid the op entirely if we have some children cached
                    bool skip = false;
                    if ( args.a && GetMemory().IsValid( FCacheAddress(args.a,item) ) )
                    {
                         bool a = GetMemory().GetBool( FCacheAddress(args.a,item) );
                         if (!a)
                         {
                            GetMemory().SetBool( item, false );
                            skip=true;
                         }
                    }

                    if ( !skip && args.b && GetMemory().IsValid( FCacheAddress(args.b,item) ) )
                    {
                         bool b = GetMemory().GetBool( FCacheAddress(args.b,item) );
                         if (!b)
                         {
                            GetMemory().SetBool( item, false );
                            skip=true;
                         }
                    }

                    if (!skip)
                    {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.a, item));
                    }
				break;
                }

            case 1:
            {
                bool a = args.a ? GetMemory().GetBool( FCacheAddress(args.a,item) ) : true;
                if (!a)
                {
                    GetMemory().SetBool( item, false );
                }
                else
                {
                    AddOp( FScheduledOp( item.At, item, 2),
                           FScheduledOp( args.b, item));
                }
                break;
            }

            case 2:
            {
                // We arrived here because a is true
                bool b = args.b ? GetMemory().GetBool( FCacheAddress(args.b,item) ) : true;
                GetMemory().SetBool( item, b );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case OP_TYPE::BO_OR:
        {
			OP::BoolBinaryArgs args = program.GetOpArgs<OP::BoolBinaryArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                {
                    // Try to avoid the op entirely if we have some children cached
                    bool skip = false;
                    if ( args.a && GetMemory().IsValid( FCacheAddress(args.a,item) ) )
                    {
                         bool a = GetMemory().GetBool( FCacheAddress(args.a,item) );
                         if (a)
                         {
                            GetMemory().SetBool( item, true );
                            skip=true;
                         }
                    }

                    if ( !skip && args.b && GetMemory().IsValid( FCacheAddress(args.b,item) ) )
                    {
                         bool b = GetMemory().GetBool( FCacheAddress(args.b,item) );
                         if (b)
                         {
                            GetMemory().SetBool( item, true );
                            skip=true;
                         }
                    }

                    if (!skip)
                    {
                        AddOp( FScheduledOp( item.At, item, 1),
                               FScheduledOp( args.a, item));
                    }
				break;
                }

            case 1:
            {
                bool a = args.a ? GetMemory().GetBool( FCacheAddress(args.a,item) ) : false;
                if (a)
                {
                    GetMemory().SetBool( item, true );
                }
                else
                {
                    AddOp( FScheduledOp( item.At, item, 2),
                           FScheduledOp( args.b, item));
                }
                break;
            }

            case 2:
            {
                // We arrived here because a is false
                bool b = args.b ? GetMemory().GetBool( FCacheAddress(args.b,item) ) : false;
                GetMemory().SetBool( item, b );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case OP_TYPE::BO_NOT:
        {
			OP::BoolNotArgs args = program.GetOpArgs<OP::BoolNotArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.source, item) );
                break;

            case 1:
            {
                bool result = !GetMemory().GetBool( FCacheAddress(args.source,item) );
                GetMemory().SetBool( item, result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case OP_TYPE::BO_EQUAL_INT_CONST:
        {
			OP::BoolEqualScalarConstArgs args = program.GetOpArgs<OP::BoolEqualScalarConstArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.value, item) );
                break;

            case 1:
            {
                int a = GetMemory().GetInt( FCacheAddress(args.value,item) );
                bool result = a == args.constant;
                GetMemory().SetBool( item, result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Int( FScheduledOp& item,
                                  const Parameters* pParams,
                                  const Model* pModel
                              )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Int);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::NU_CONSTANT:
        {
			OP::IntConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::IntConstantArgs>(item.At);
            int result = args.value;
            GetMemory().SetInt( item, result );
            break;
        }

        case OP_TYPE::NU_PARAMETER:
        {
			OP::ParameterArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ParameterArgs>(item.At);
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            int result = pParams->GetIntValue( args.variable, index );

            // Check that the value is actually valid. Otherwise set the default.
            if ( pParams->GetIntPossibleValueCount( args.variable ) )
            {
                bool valid = false;
                for ( int i=0;
                      (!valid) && i<pParams->GetIntPossibleValueCount( args.variable );
                      ++i )
                {
                    if ( result == pParams->GetIntPossibleValue( args.variable, i ) )
                    {
                        valid = true;
                    }
                }

                if (!valid)
                {
                    result = pParams->GetIntPossibleValue( args.variable, 0 );
                }
            }

            GetMemory().SetInt( item, result );
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Scalar( FScheduledOp& item,
                                     const Parameters* pParams,
                                     const Model* pModel
                                     )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Scalar);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::SC_CONSTANT:
        {
			OP::ScalarConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ScalarConstantArgs>(item.At);
            float result = args.value;
            GetMemory().SetScalar( item, result );
            break;
        }

        case OP_TYPE::SC_PARAMETER:
        {
			OP::ParameterArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ParameterArgs>(item.At);
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            float result = pParams->GetFloatValue( args.variable, index );
            GetMemory().SetScalar( item, result );
            break;
        }

        case OP_TYPE::SC_CURVE:
        {
			OP::ScalarCurveArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ScalarCurveArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.time, item) );
                break;

            case 1:
            {
                float time = GetMemory().GetScalar( FCacheAddress(args.time,item) );

                const Curve& curve = pModel->GetPrivate()->m_program.m_constantCurves[args.curve];
                float result = EvalCurve(curve, time);

                GetMemory().SetScalar( item, result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case OP_TYPE::SC_MULTIPLYADD:
            // \TODO
            check( false );
            break;

        case OP_TYPE::SC_ARITHMETIC:
        {
			OP::ArithmeticArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ArithmeticArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.a, item),
                           FScheduledOp( args.b, item) );
                break;

            case 1:
            {
                float a = GetMemory().GetScalar( FCacheAddress(args.a,item) );
                float b = GetMemory().GetScalar( FCacheAddress(args.b,item) );

                float result = 1.0f;
                switch (args.operation)
                {
                case OP::ArithmeticArgs::ADD:
                    result = a + b;
                    break;

                case OP::ArithmeticArgs::MULTIPLY:
                    result = a * b;
                    break;

                case OP::ArithmeticArgs::SUBTRACT:
                    result = a - b;
                    break;

                case OP::ArithmeticArgs::DIVIDE:
                    result = a / b;
                    break;

                default:
                    checkf(false, TEXT("Arithmetic operation not implemented."));
                    break;
                }

                GetMemory().SetScalar( item, result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_String( FScheduledOp& item, const Parameters* pParams, const Model* pModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_String );

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType( item.At );
        switch ( type )
        {

        case OP_TYPE::ST_CONSTANT:
        {
			OP::ResourceConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ResourceConstantArgs>( item.At );
            check( args.value < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );

            const std::string& result = pModel->GetPrivate()->m_program.m_constantStrings[args.value];
            GetMemory().SetString( item, new String(result.c_str()) );

            break;
        }

        case OP_TYPE::ST_PARAMETER:
        {
			OP::ParameterArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ParameterArgs>( item.At );
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            string result = pParams->GetStringValue( args.variable, index );
            GetMemory().SetString( item, new String( result.c_str() ) );
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Colour( FScheduledOp& item,
                                     const Parameters* pParams,
                                     const Model* pModel
                                     )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Colour);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);

        const FProgram& program = pModel->GetPrivate()->m_program;

        switch ( type )
        {

        case OP_TYPE::CO_CONSTANT:
        {
			OP::ColourConstantArgs args = program.GetOpArgs<OP::ColourConstantArgs>(item.At);
			FVector4f result;
            result[0] = args.value[0];
            result[1] = args.value[1];
            result[2] = args.value[2];
            result[3] = args.value[3];
            GetMemory().SetColour( item, result );
            break;
        }

        case OP_TYPE::CO_PARAMETER:
        {
			OP::ParameterArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ParameterArgs>(item.At);
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            float r=0.0f;
            float g=0.0f;
            float b=0.0f;            
            pParams->GetColourValue( args.variable, &r, &g, &b, index );
            GetMemory().SetColour( item, FVector4f(r,g,b,1.0f) );
            break;
        }

        case OP_TYPE::CO_SAMPLEIMAGE:
        {
			OP::ColourSampleImageArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ColourSampleImageArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.x, item),
                           FScheduledOp( args.y, item),
						   // Don't skip mips for the texture to sample
                           FScheduledOp::FromOpAndOptions( args.image, item, 0) );
                break;

            case 1:
            {
                float x = args.x ? GetMemory().GetScalar( FCacheAddress(args.x,item) ) : 0.5f;
                float y = args.y ? GetMemory().GetScalar( FCacheAddress(args.y,item) ) : 0.5f;

                Ptr<const Image> pImage = GetMemory().GetImage(FScheduledOp::FromOpAndOptions(args.image, item, 0));

				FVector4f result;
                if (pImage)
                {
                    if (args.filter)
                    {
                        // TODO
                        result = pImage->Sample(FVector2f(x, y));
                    }
                    else
                    {
                        result = pImage->Sample(FVector2f(x, y));
                    }
                }
                else
                {
                    result = FVector4f();
                }

                GetMemory().SetColour( item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::CO_SWIZZLE:
        {
			OP::ColourSwizzleArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ColourSwizzleArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.sources[0], item),
                           FScheduledOp( args.sources[1], item),
                           FScheduledOp( args.sources[2], item),
                           FScheduledOp( args.sources[3], item) );
                break;

            case 1:
            {
				FVector4f result;

                for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
                {
                    if ( args.sources[t] )
                    {
                        FVector4f p = GetMemory().GetColour( FCacheAddress(args.sources[t],item) );
                        result[t] = p[ args.sourceChannels[t] ];
                    }
                }

                GetMemory().SetColour( item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::CO_IMAGESIZE:
        {
			OP::ColourImageSizeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ColourImageSizeArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.image, item) );
                break;

            case 1:
            {
                Ptr<const Image> pImage = GetMemory().GetImage( FCacheAddress(args.image,item) );

				FVector4f result = FVector4f( (float)pImage->GetSizeX(), (float)pImage->GetSizeY(), 0.0f, 0.0f );

                GetMemory().SetColour( item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::CO_LAYOUTBLOCKTRANSFORM:
        {
			OP::ColourLayoutBlockTransformArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ColourLayoutBlockTransformArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.layout, item) );
                break;

            case 1:
            {
                Ptr<const Layout> pLayout = GetMemory().GetLayout( FCacheAddress(args.layout,item) );

				FVector4f result = FVector4f(0,0,0,0);
                if ( pLayout )
                {
                    int relBlockIndex = pLayout->FindBlock( args.block );

                    if( relBlockIndex >=0 )
                    {
                        box< vec2<int> > rectInblocks;
                        pLayout->GetBlock
                                (
                                    relBlockIndex,
                                    &rectInblocks.min[0], &rectInblocks.min[1],
                                    &rectInblocks.size[0], &rectInblocks.size[1]
                                    );

                        // Convert the rect from blocks to pixels
                        FIntPoint grid = pLayout->GetGridSize();

                        result = FVector4f( float(rectInblocks.min[0]) / float(grid[0]),
                                              float(rectInblocks.min[1]) / float(grid[1]),
                                              float(rectInblocks.size[0]) / float(grid[0]),
                                              float(rectInblocks.size[1]) / float(grid[1]) );
                    }
                }

                GetMemory().SetColour( item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::CO_FROMSCALARS:
        {
			OP::ColourFromScalarsArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ColourFromScalarsArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.x, item),
                           FScheduledOp( args.y, item),
                           FScheduledOp( args.z, item),
                           FScheduledOp( args.w, item));
                break;

            case 1:
            {
				FVector4f result = FVector4f(1, 1, 1, 1);

                if (args.x)
                {
                    result[0] = GetMemory().GetScalar( FCacheAddress(args.x,item) );
                }

                if (args.y)
                {
                    result[1] = GetMemory().GetScalar( FCacheAddress(args.y,item) );
                }

                if (args.z)
                {
                    result[2] = GetMemory().GetScalar( FCacheAddress(args.z,item) );
                }

                if (args.w)
                {
                    result[3] = GetMemory().GetScalar( FCacheAddress(args.w,item) );
                }

                GetMemory().SetColour( item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::CO_ARITHMETIC:
        {
			OP::ArithmeticArgs args = program.GetOpArgs<OP::ArithmeticArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.a, item),
                           FScheduledOp( args.b, item));
                break;

            case 1:
            {
				OP_TYPE otype = program.GetOpType( args.a );
                DATATYPE dtype = GetOpDataType( otype );
                check( dtype == DT_COLOUR );
                otype = program.GetOpType( args.b );
                dtype = GetOpDataType( otype );
                check( dtype == DT_COLOUR );
				FVector4f a = args.a ? GetMemory().GetColour( FCacheAddress( args.a, item ) )
                                 : FVector4f( 0, 0, 0, 0 );
				FVector4f b = args.b ? GetMemory().GetColour( FCacheAddress( args.b, item ) )
                                 : FVector4f( 0, 0, 0, 0 );

				FVector4f result = FVector4f(0,0,0,0);
                switch (args.operation)
                {
                case OP::ArithmeticArgs::ADD:
                    result = a + b;
                    break;

                case OP::ArithmeticArgs::MULTIPLY:
                    result = a * b;
                    break;

                case OP::ArithmeticArgs::SUBTRACT:
                    result = a - b;
                    break;

                case OP::ArithmeticArgs::DIVIDE:
                    result = a / b;
                    break;

                default:
                    checkf(false, TEXT("Arithmetic operation not implemented."));
                    break;
                }

                GetMemory().SetColour( item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Projector( FScheduledOp& item,
                                const Parameters* pParams,
                                const Model* pModel
                              )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Projector);

        const FProgram& program = pModel->GetPrivate()->m_program;
		OP_TYPE type = program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::PR_CONSTANT:
        {
			OP::ResourceConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ResourceConstantArgs>(item.At);
            ProjectorPtr pResult = new Projector();
            pResult->m_value = program.m_constantProjectors[args.value];
            GetMemory().SetProjector( item, pResult );
            break;
        }

        case OP_TYPE::PR_PARAMETER:
        {
			OP::ParameterArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ParameterArgs>(item.At);
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            ProjectorPtr pResult = new Projector();
            pResult->m_value = pParams->GetPrivate()->GetProjectorValue(args.variable,index);

            // The type cannot be changed, take it from the default value
            const FProjector& def = program.m_parameters[args.variable].m_defaultValue.m_projector;
            pResult->m_value.type = def.type;

            GetMemory().SetProjector( item, pResult );
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Layout( FScheduledOp& item,
                                     const Model* pModel
                                     )
    {
        //MUTABLE_CPUPROFILER_SCOPE(RunCode_Layout);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::LA_CONSTANT:
        {
			OP::ResourceConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ResourceConstantArgs>(item.At);
            check( args.value < (uint32)pModel->GetPrivate()->m_program.m_constantLayouts.Num() );

            LayoutPtrConst pResult = pModel->GetPrivate()->m_program.m_constantLayouts
                    [ args.value ];
            GetMemory().SetLayout( item, pResult );
            break;
        }

        case OP_TYPE::LA_MERGE:
        {
			OP::LayoutMergeArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::LayoutMergeArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.Base, item),
                           FScheduledOp( args.Added, item) );
                break;

            case 1:
            {
                Ptr<const Layout> pA = GetMemory().GetLayout( FCacheAddress(args.Base,item) );
                Ptr<const Layout> pB = GetMemory().GetLayout( FCacheAddress(args.Added,item) );

                LayoutPtrConst pResult;

                if (pA && pB)
                {
					pResult = LayoutMerge(pA.get(),pB.get());
                }
                else if (pA)
                {
                    pResult = pA->Clone();
                }
                else if (pB)
                {
                    pResult = pB->Clone();
                }

                GetMemory().SetLayout( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::LA_PACK:
        {
			OP::LayoutPackArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::LayoutPackArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( item.At, item, 1),
                           FScheduledOp( args.Source, item) );
                break;

            case 1:
            {
                Ptr<const Layout> pSource = GetMemory().GetLayout( FCacheAddress(args.Source,item) );

				LayoutPtr pResult;

				if (pSource)
				{
					pResult = pSource->Clone();

					SCRATCH_LAYOUT_PACK scratch;
					int32 BlockCount = pSource->GetBlockCount();
					scratch.blocks.SetNum(BlockCount);
					scratch.sorted.SetNum(BlockCount);
					scratch.positions.SetNum(BlockCount);
					scratch.priorities.SetNum(BlockCount);
					scratch.reductions.SetNum(BlockCount);

					LayoutPack3(pResult.get(), pSource.get(), &scratch);
				}

                GetMemory().SetLayout( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case OP_TYPE::LA_FROMMESH:
		{
			OP::LayoutFromMeshArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::LayoutFromMeshArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1),
					FScheduledOp(args.Mesh, item));
				break;

			case 1:
			{
				Ptr<const Mesh> Mesh = GetMemory().GetMesh(FCacheAddress(args.Mesh, item));

				Ptr<const Layout> Result = LayoutFromMesh_RemoveBlocks(Mesh.get(), args.LayoutIndex);

				GetMemory().SetLayout(item, Result);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::LA_REMOVEBLOCKS:
		{
			OP::LayoutRemoveBlocksArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::LayoutRemoveBlocksArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1),
					FScheduledOp(args.Source, item),
					FScheduledOp(args.ReferenceLayout, item));
				break;

			case 1:
			{
				Ptr<const Layout> Source = GetMemory().GetLayout(FCacheAddress(args.Source, item));
				Ptr<const Layout> ReferenceLayout = GetMemory().GetLayout(FCacheAddress(args.ReferenceLayout, item));

				Ptr<const Layout> pResult;

				if (Source && ReferenceLayout)
				{
					pResult = LayoutRemoveBlocks(Source.get(), ReferenceLayout.get());
				}
				else if (Source)
				{
					pResult = Source;
				}

				GetMemory().SetLayout(item, pResult);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        default:
            // Operation not implemented
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode( FScheduledOp& item,
                              const Parameters* pParams,
                              const Model* pModel,
                              uint32 lodMask)
    {
		//UE_LOG( LogMutableCore, Log, TEXT("Running :%5d , %d "), item.At, item.Stage );
		check( item.Type == FScheduledOp::EType::Full );

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
		//UE_LOG(LogMutableCore, Log, TEXT("Running :%5d , %d, of type %d "), item.At, item.Stage, type);
		switch ( type )
        {
        case OP_TYPE::NONE:
            break;

        case OP_TYPE::NU_CONDITIONAL:
        case OP_TYPE::SC_CONDITIONAL:
        case OP_TYPE::CO_CONDITIONAL:
        case OP_TYPE::IM_CONDITIONAL:
        case OP_TYPE::ME_CONDITIONAL:
        case OP_TYPE::LA_CONDITIONAL:
        case OP_TYPE::IN_CONDITIONAL:
            RunCode_Conditional(item, pModel);
            break;

        case OP_TYPE::ME_CONSTANT:
        case OP_TYPE::IM_CONSTANT:
            RunCode_ConstantResource(item, pModel);
            break;

        case OP_TYPE::NU_SWITCH:
        case OP_TYPE::SC_SWITCH:
        case OP_TYPE::CO_SWITCH:
        case OP_TYPE::IM_SWITCH:
        case OP_TYPE::ME_SWITCH:
        case OP_TYPE::LA_SWITCH:
        case OP_TYPE::IN_SWITCH:
            RunCode_Switch(item, pModel);
            break;

        case OP_TYPE::IN_ADDMESH:
        case OP_TYPE::IN_ADDIMAGE:
            RunCode_InstanceAddResource(item, pModel, pParams);
            break;

		default:
		{
			DATATYPE DataType = GetOpDataType(type);
			switch (DataType)
			{
			case DT_INSTANCE:
				RunCode_Instance(item, pModel, lodMask);
				break;

			case DT_MESH:
				RunCode_Mesh(item, pModel);
				break;

			case DT_IMAGE:
				RunCode_Image(item, pParams, pModel);
				break;

			case DT_LAYOUT:
				RunCode_Layout(item, pModel);
				break;

			case DT_BOOL:
				RunCode_Bool(item, pParams, pModel);
				break;

			case DT_SCALAR:
				RunCode_Scalar(item, pParams, pModel);
				break;

			case DT_STRING:
				RunCode_String(item, pParams, pModel);
				break;

			case DT_INT:
				RunCode_Int(item, pParams, pModel);
				break;

			case DT_PROJECTOR:
				RunCode_Projector(item, pParams, pModel);
				break;

			case DT_COLOUR:
				RunCode_Colour(item, pParams, pModel);
				break;

			default:
				check(false);
				break;
			}
			break;
		}

        }
    }


	//---------------------------------------------------------------------------------------------
	void CodeRunner::RunCodeImageDesc(FScheduledOp& item,
		const Parameters* pParams,
		const Model* pModel, 
		uint32 lodMask
	)
	{
		MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc);

		check(item.Type == FScheduledOp::EType::ImageDesc);

		// Ensure there is room for the result in the heap.
		if (item.CustomState >= uint32(m_heapData.Num()))
		{
			m_heapImageDesc.SetNum(item.CustomState+1);
		}


		const FProgram& program = pModel->GetPrivate()->m_program;

		OP_TYPE type = program.GetOpType(item.At);
		switch (type)
		{

		case OP_TYPE::IM_CONSTANT:
		{
			check(item.Stage == 0);
			OP::ResourceConstantArgs args = program.GetOpArgs<OP::ResourceConstantArgs>(item.At);
			int32 ImageIndex = args.value;
			m_heapImageDesc[item.CustomState].m_format = EImageFormat::IF_NONE;	// TODO: precalculate if necessary
			m_heapImageDesc[item.CustomState].m_size[0] = program.m_constantImages[ImageIndex].ImageSizeX;
			m_heapImageDesc[item.CustomState].m_size[1] = program.m_constantImages[ImageIndex].ImageSizeY;
			m_heapImageDesc[item.CustomState].m_lods = program.m_constantImages[ImageIndex].LODCount;
			GetMemory().SetValidDesc(item);
			break;
		}

		case OP_TYPE::IM_PARAMETER:
		{
			check(item.Stage == 0);
			OP::ParameterArgs args = program.GetOpArgs<OP::ParameterArgs>(item.At);
			EXTERNAL_IMAGE_ID id = pParams->GetImageValue(args.variable);
			uint8 MipsToSkip = item.ExecutionOptions;
			m_heapImageDesc[item.CustomState] = GetExternalImageDesc(id, MipsToSkip);
			GetMemory().SetValidDesc(item);
			break;
		}

		case OP_TYPE::IM_CONDITIONAL:
		{
			OP::ConditionalArgs args = program.GetOpArgs<OP::ConditionalArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
			{
				// We need to run the full condition result
				FScheduledOp FullConditionOp(args.condition, item);
				FullConditionOp.Type = FScheduledOp::EType::Full;
				AddOp(FScheduledOp(item.At, item, 1), FullConditionOp);
				break;
			}

			case 1:
			{
				bool value = GetMemory().GetBool(FCacheAddress(args.condition, item.ExecutionIndex, item.ExecutionOptions));
				OP::ADDRESS resultAt = value ? args.yes : args.no;
				AddOp(FScheduledOp(item.At, item, 2), FScheduledOp(resultAt, item));
				break;
			}

			case 2: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_SWITCH:
		{
			const uint8* data = program.GetOpArgsPointer(item.At);
		
			OP::ADDRESS VarAddress;
			FMemory::Memcpy( &VarAddress, data, sizeof(OP::ADDRESS));
			data += sizeof(OP::ADDRESS);

			OP::ADDRESS DefAddress;
			FMemory::Memcpy( &DefAddress, data, sizeof(OP::ADDRESS));
			data += sizeof(OP::ADDRESS);

			uint32 CaseCount;
			FMemory::Memcpy( &CaseCount, data, sizeof(uint32));
			data += sizeof(uint32);
	
			switch (item.Stage)
			{
			case 0:
			{
				if (VarAddress)
				{
					// We need to run the full condition result
					FScheduledOp FullVariableOp(VarAddress, item);
					FullVariableOp.Type = FScheduledOp::EType::Full;
					AddOp(FScheduledOp(item.At, item, 1), FullVariableOp);
				}
				else
				{
					GetMemory().SetValidDesc(item);
				}
				break;
			}

			case 1:
			{
				// Get the variable result
				int var = GetMemory().GetInt(FCacheAddress(VarAddress, item));

				OP::ADDRESS valueAt = DefAddress;
				for (uint32 C = 0; C < CaseCount; ++C)
				{
					int32 Condition;
					FMemory::Memcpy( &Condition, data, sizeof(int32) );
					data += sizeof(int32);
					
					OP::ADDRESS At;
					FMemory::Memcpy( &At, data, sizeof(OP::ADDRESS) );
					data += sizeof(OP::ADDRESS);

					if (At && var == (int)Condition)
					{
						valueAt = At;
						break;
					}
				}

				AddOp(FScheduledOp(item.At, item, 2, valueAt),
					  FScheduledOp(valueAt, item));

				break;
			}

			case 2: GetMemory().SetValidDesc(item); break;
			default: check(false); break;
			}
			break;
		}

		case OP_TYPE::IM_LAYERCOLOUR:
		{
			OP::ImageLayerColourArgs args = program.GetOpArgs<OP::ImageLayerColourArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_LAYER:
		{
			OP::ImageLayerArgs args = program.GetOpArgs<OP::ImageLayerArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_MULTILAYER:
		{
			OP::ImageMultiLayerArgs args = program.GetOpArgs<OP::ImageMultiLayerArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_DIFFERENCE:
		{
			OP::ImageDifferenceArgs args = program.GetOpArgs<OP::ImageDifferenceArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.a, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_NORMALCOMPOSITE:
		{
			OP::ImageNormalCompositeArgs args = program.GetOpArgs<OP::ImageNormalCompositeArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_PIXELFORMAT:
		{
			OP::ImagePixelFormatArgs args = program.GetOpArgs<OP::ImagePixelFormatArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.source, item));
				break;

			case 1:
			{
				// Update directly in the heap
				EImageFormat OldFormat = m_heapImageDesc[item.CustomState].m_format;
				EImageFormat NewFormat = args.format;
				if (args.formatIfAlpha != EImageFormat::IF_NONE
					&&
					GetImageFormatData(OldFormat).m_channels > 3)
				{
					NewFormat = args.formatIfAlpha;
				}
				m_heapImageDesc[item.CustomState].m_format = NewFormat;				
				GetMemory().SetValidDesc(item);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::IM_MIPMAP:
		{
			OP::ImageMipmapArgs args = program.GetOpArgs<OP::ImageMipmapArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.source, item));
				break;

			case 1:
			{
				// Somewhat synched with Full op execution code.
				FImageDesc BaseDesc = m_heapImageDesc[item.CustomState];
				int levelCount = args.levels;
				int maxLevelCount = Image::GetMipmapCount(BaseDesc.m_size[0], BaseDesc.m_size[1]);
				if (levelCount == 0)
				{
					levelCount = maxLevelCount;
				}
				else if (levelCount > maxLevelCount)
				{
					// If code generation is smart enough, this should never happen.
					// \todo But apparently it does, sometimes.
					levelCount = maxLevelCount;
				}

				// At least keep the levels we already have.
				int startLevel = BaseDesc.m_lods;
				levelCount = FMath::Max(startLevel, levelCount);

				// Update result.
				m_heapImageDesc[item.CustomState].m_lods = levelCount;
				GetMemory().SetValidDesc(item);
				break;
			}

			default:
				check(false);
			}
			break;
		}

		case OP_TYPE::IM_RESIZE:
		{
			OP::ImageResizeArgs args = program.GetOpArgs<OP::ImageResizeArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.source, item));
				break;

			case 1:
				m_heapImageDesc[item.CustomState].m_size[0] = args.size[0];
				m_heapImageDesc[item.CustomState].m_size[1] = args.size[1];
				GetMemory().SetValidDesc(item);
				break;

			default:
				check(false);
			}
			break;
		}

		case OP_TYPE::IM_RESIZELIKE:
		{
			OP::ImageResizeLikeArgs args = program.GetOpArgs<OP::ImageResizeLikeArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
			{
				int32 ResultAndBaseDesc = item.CustomState;
				int32 SourceDescAddress = m_heapImageDesc.Add({});
				FScheduledOpData Data;
				Data.ResizeLike.ResultDescAt = ResultAndBaseDesc;
				Data.ResizeLike.SourceDescAt = SourceDescAddress;
				int32 SecondStageData = m_heapData.Add(Data);
				AddOp(FScheduledOp(item.At, item, 1, SecondStageData),
					FScheduledOp(args.source, item, 0, ResultAndBaseDesc),
					FScheduledOp(args.sizeSource, item, 0, SourceDescAddress));
				break;
			}

			case 1:
			{
				const FScheduledOpData& SecondStageData = m_heapData[ item.CustomState ];
				FImageDesc& ResultAndBaseDesc = m_heapImageDesc[SecondStageData.ResizeLike.ResultDescAt];
				const FImageDesc& SourceDesc = m_heapImageDesc[SecondStageData.ResizeLike.SourceDescAt];
				ResultAndBaseDesc.m_size = SourceDesc.m_size;
				GetMemory().SetValidDesc(item);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::IM_RESIZEREL:
		{
			OP::ImageResizeRelArgs args = program.GetOpArgs<OP::ImageResizeRelArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.source, item));
				break;

			case 1:
			{
				FImageDesc& ResultAndBaseDesc = m_heapImageDesc[item.CustomState];
				FImageSize destSize(
					uint16(ResultAndBaseDesc.m_size[0] * args.factor[0] + 0.5f),
					uint16(ResultAndBaseDesc.m_size[1] * args.factor[1] + 0.5f));
				ResultAndBaseDesc.m_size = destSize;
				GetMemory().SetValidDesc(item);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::IM_BLANKLAYOUT:
		{
			OP::ImageBlankLayoutArgs args = program.GetOpArgs<OP::ImageBlankLayoutArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
			{
				// We need to run the full layout
				FScheduledOp FullLayoutOp(args.layout, item);
				FullLayoutOp.Type = FScheduledOp::EType::Full;
				AddOp(FScheduledOp(item.At, item, 1), FullLayoutOp);
				break;
			}

			case 1:
			{
				Ptr<const Layout> pLayout = GetMemory().GetLayout(FCacheAddress(args.layout, item));

				FIntPoint SizeInBlocks = pLayout->GetGridSize();
				FIntPoint BlockSizeInPixels(args.blockSize[0], args.blockSize[1]);
				FIntPoint ImageSizeInPixels = SizeInBlocks * BlockSizeInPixels;

				FImageDesc& ResultAndBaseDesc = m_heapImageDesc[item.CustomState];
				FImageSize destSize(uint16(ImageSizeInPixels.X), uint16(ImageSizeInPixels.Y));
				ResultAndBaseDesc.m_size = destSize;
				
				if (args.generateMipmaps)
				{
					if (args.mipmapCount == 0)
					{
						ResultAndBaseDesc.m_lods = Image::GetMipmapCount(ImageSizeInPixels.X, ImageSizeInPixels.Y);
					}
					else
					{
						ResultAndBaseDesc.m_lods = args.mipmapCount;
					}
				}
				GetMemory().SetValidDesc(item);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::IM_COMPOSE:
		{
			OP::ImageComposeArgs args = program.GetOpArgs<OP::ImageComposeArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_INTERPOLATE:
		{
			OP::ImageInterpolateArgs args = program.GetOpArgs<OP::ImageInterpolateArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.targets[0], item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_INTERPOLATE3:
		{
			OP::ImageInterpolate3Args args = program.GetOpArgs<OP::ImageInterpolate3Args>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.target0, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_SATURATE:
		{
			OP::ImageSaturateArgs args = program.GetOpArgs<OP::ImageSaturateArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_LUMINANCE:
		{
			OP::ImageLuminanceArgs args = program.GetOpArgs<OP::ImageLuminanceArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item));
				break;

			case 1:
				m_heapImageDesc[item.CustomState].m_format = EImageFormat::IF_L_UBYTE;
				GetMemory().SetValidDesc(item);
				break;

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::IM_SWIZZLE:
		{
			OP::ImageSwizzleArgs args = program.GetOpArgs<OP::ImageSwizzleArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.sources[0], item));
				break;

			case 1:
				m_heapImageDesc[item.CustomState].m_format = args.format;
				GetMemory().SetValidDesc(item);
				break;

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::IM_SELECTCOLOUR:
		{
			OP::ImageSelectColourArgs args = program.GetOpArgs<OP::ImageSelectColourArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item));
				break;

			case 1:
				m_heapImageDesc[item.CustomState].m_format = EImageFormat::IF_L_UBYTE;
				GetMemory().SetValidDesc(item);
				break;

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::IM_COLOURMAP:
		{
			OP::ImageColourMapArgs args = program.GetOpArgs<OP::ImageColourMapArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_GRADIENT:
		{
			OP::ImageGradientArgs args = program.GetOpArgs<OP::ImageGradientArgs>(item.At);
			m_heapImageDesc[item.CustomState].m_size[0] = args.size[0];
			m_heapImageDesc[item.CustomState].m_size[1] = args.size[1];
			m_heapImageDesc[item.CustomState].m_lods = 1;
			m_heapImageDesc[item.CustomState].m_format = EImageFormat::IF_RGB_UBYTE;
			GetMemory().SetValidDesc(item);
			break;
		}

		case OP_TYPE::IM_BINARISE:
		{
			OP::ImageBinariseArgs args = program.GetOpArgs<OP::ImageBinariseArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item));
				break;

			case 1:
				m_heapImageDesc[item.CustomState].m_format = EImageFormat::IF_L_UBYTE;
				GetMemory().SetValidDesc(item);
				break;

			default:
				check(false);
			}
			break;
		}

		case OP_TYPE::IM_INVERT:
		{
			OP::ImageInvertArgs args = program.GetOpArgs<OP::ImageInvertArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_PLAINCOLOUR:
		{
			OP::ImagePlainColourArgs args = program.GetOpArgs<OP::ImagePlainColourArgs>(item.At);
			m_heapImageDesc[item.CustomState].m_size[0] = args.size[0];
			m_heapImageDesc[item.CustomState].m_size[1] = args.size[1];
			m_heapImageDesc[item.CustomState].m_lods = 1;
			m_heapImageDesc[item.CustomState].m_format = EImageFormat::IF_RGB_UBYTE;
			GetMemory().SetValidDesc(item);
			break;
		}

		case OP_TYPE::IM_CROP:
		{
			OP::ImageCropArgs args = program.GetOpArgs<OP::ImageCropArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.source, item));
				break;

			case 1:
				m_heapImageDesc[item.CustomState].m_size[0] = args.sizeX;
				m_heapImageDesc[item.CustomState].m_size[1] = args.sizeY;
				m_heapImageDesc[item.CustomState].m_lods = 1;
				GetMemory().SetValidDesc(item);
				break;

			default:
				check(false);
			}
			break;
		}

		case OP_TYPE::IM_PATCH:
		{
			OP::ImagePatchArgs args = program.GetOpArgs<OP::ImagePatchArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_RASTERMESH:
		{
			OP::ImageRasterMeshArgs args = program.GetOpArgs<OP::ImageRasterMeshArgs>(item.At);
			m_heapImageDesc[item.CustomState].m_size[0] = args.sizeX;
			m_heapImageDesc[item.CustomState].m_size[1] = args.sizeY;
			m_heapImageDesc[item.CustomState].m_lods = 1;
			m_heapImageDesc[item.CustomState].m_format = EImageFormat::IF_L_UBYTE;
			GetMemory().SetValidDesc(item);
			break;
		}

		case OP_TYPE::IM_MAKEGROWMAP:
		{
			OP::ImageMakeGrowMapArgs args = program.GetOpArgs<OP::ImageMakeGrowMapArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.mask, item));
				break;

			case 1:
				m_heapImageDesc[item.CustomState].m_format = EImageFormat::IF_L_UBYTE;
				m_heapImageDesc[item.CustomState].m_lods = 1;
				GetMemory().SetValidDesc(item);
				break;

			default:
				check(false);
			}

			break;
		}

		case OP_TYPE::IM_DISPLACE:
		{
			OP::ImageDisplaceArgs args = program.GetOpArgs<OP::ImageDisplaceArgs>(item.At);
			switch (item.Stage)
			{
			case 0: AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.source, item)); break;
			case 1: GetMemory().SetValidDesc(item); break;
			default: check(false);
			}
			break;
		}

        case OP_TYPE::IM_TRANSFORM:
        {

			OP::ImageTransformArgs Args = program.GetOpArgs<OP::ImageTransformArgs>(item.At);

            switch (item.Stage)
            {
            case 0:
			{
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(Args.base, item));
                break;
			}
            case 1:
            {
				GetMemory().SetValidDesc(item);
                break;
            }

            default:
                check(false);
            }

			break;
		}

		default:
			if (type != OP_TYPE::NONE)
			{
				// Operation not implemented
				check(false);
				m_heapImageDesc[item.CustomState] = FImageDesc();
			}
			break;
		}
	}


}

