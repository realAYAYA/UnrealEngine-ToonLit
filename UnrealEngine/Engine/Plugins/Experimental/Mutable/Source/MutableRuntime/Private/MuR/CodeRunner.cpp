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
#include "MuR/OpImageDisplace.h"
#include "MuR/OpImageGradient.h"
#include "MuR/OpImageInterpolate.h"
#include "MuR/OpImageInvert.h"
#include "MuR/OpImageLuminance.h"
#include "MuR/OpImageNormalCombine.h"
#include "MuR/OpImageProject.h"
#include "MuR/OpImageRasterMesh.h"
#include "MuR/OpImageSaturate.h"
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
#include "MuR/OpMeshRemove.h"
#include "MuR/OpMeshReshape.h"
#include "MuR/OpMeshTransform.h"
#include "MuR/OpMeshOptimizeSkinning.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/PhysicsBody.h"
#include "MuR/Platform.h"
#include "MuR/Skeleton.h"
#include "MuR/SystemPrivate.h"
#include "Templates/Tuple.h"
#include "Trace/Detail/Channel.h"

namespace
{

int32 ForcedProjectionMode = -1;
static FAutoConsoleVariableRef CVarForceProjectionSamplingMode (
	TEXT("mutable.ForceProjectionMode"),
	ForcedProjectionMode,
	TEXT("force mutable to use an specific projection mode, 0 = Point + None, 1 = Bilinear + TotalAreaHeuristic, -1 uses the values provided by the projector."),
	ECVF_Default);

float GlobalProjectionLodBias = 0.0f;
static FAutoConsoleVariableRef CVarGlobalProjectionLodBias (
	TEXT("mutable.GlobalProjectionLodBias"),
	GlobalProjectionLodBias,
	TEXT("Lod bias applied to the lod resulting form the best mip computation, only used if a min filter method different than None is used."),
	ECVF_Default);

bool bUseProjectionVectorImpl = true;
static FAutoConsoleVariableRef CVarUseProjectionVectorImpl (
	TEXT("mutable.UseProjectionVectorImpl"),
	bUseProjectionVectorImpl,
	TEXT("If set to true, enables the vectorized implementation of the projection pixel processing."),
	ECVF_Default);
}

namespace mu
{

    //---------------------------------------------------------------------------------------------
    CodeRunner::CodeRunner(const Ptr<const Settings>& InSettings,
		class System::Private* InSystem,
		EExecutionStrategy InExecutionStrategy,
		const TSharedPtr<const Model>& InModel,
		const Parameters* InParams,
		OP::ADDRESS at,
		uint32 InLodMask, uint8 executionOptions, int32 InImageLOD, FScheduledOp::EType Type )
		: m_pSettings(InSettings)
		, ExecutionStrategy(InExecutionStrategy)
		, m_pSystem(InSystem)
		, m_pModel(InModel)
		, m_pParams(InParams)
		, m_lodMask(InLodMask)
	{
		FProgram& program = m_pModel->GetPrivate()->m_program;
		ScheduledStagePerOp.resize(program.m_opAddress.Num());

		// We will read this in the end, so make sure we keep it.
   		if (Type == FScheduledOp::EType::Full)
   		{
			GetMemory().IncreaseHitCount(FCacheAddress(at, 0, executionOptions));
		}
    	

		ImageLOD = InImageLOD;

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
		return *m_pSystem->WorkingMemoryManager.CurrentInstanceCache;
	}


    //---------------------------------------------------------------------------------------------
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	TTuple<UE::Tasks::FTask, TFunction<void()>> CodeRunner::LoadExternalImageAsync(FName Id, uint8 MipmapsToSkip, TFunction<void(Ptr<Image>)>& ResultCallback)
#else
	TTuple<FGraphEventRef, TFunction<void()>> CodeRunner::LoadExternalImageAsync(FName Id, uint8 MipmapsToSkip, TFunction<void(Ptr<Image>)>& ResultCallback)
#endif
    {
		MUTABLE_CPUPROFILER_SCOPE(LoadExternalImageAsync);

		check(m_pSystem);

		if (m_pSystem->ImageParameterGenerator)
		{
			return m_pSystem->ImageParameterGenerator->GetImageAsync(Id, MipmapsToSkip, ResultCallback);
		}
		else
		{
			// Not found and there is no generator!
			check(false);
		}

		// Not needed as it should never reach this point, but added for correctness.
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		UE::Tasks::FTaskEvent CompletionEvent(TEXT("LoadExternalImageAsyncCompletion"));
		CompletionEvent.Trigger();
#else
		FGraphEventRef CompletionEvent = FGraphEvent::CreateGraphEvent();
		CompletionEvent->DispatchSubsequents();
#endif

		return MakeTuple(CompletionEvent, []() -> void {});
	}

	
    //---------------------------------------------------------------------------------------------
	FImageDesc CodeRunner::GetExternalImageDesc(FName Id, uint8 MipmapsToSkip)
	{
		MUTABLE_CPUPROFILER_SCOPE(GetExternalImageDesc);

		check(m_pSystem);

		if (m_pSystem->ImageParameterGenerator)
		{
			return m_pSystem->ImageParameterGenerator->GetImageDesc(Id, MipmapsToSkip);
		}
		else
		{
			// Not found and there is no generator!
			check(false);
		}

		return FImageDesc();
	}

	
    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Conditional( const FScheduledOp& item, const Model* pModel )
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
            value = LoadBool( FCacheAddress(args.condition, item.ExecutionIndex, item.ExecutionOptions) );

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
            case DT_BOOL:       StoreBool( cat, LoadBool(rat) ); break;
            case DT_INT:        StoreInt( cat, LoadInt(rat) ); break;
            case DT_SCALAR:     StoreScalar( cat, LoadScalar(rat) ); break;
			case DT_STRING:		StoreString( cat, LoadString( rat ) ); break;
            case DT_COLOUR:		StoreColor( cat, LoadColor( rat ) ); break;
            case DT_PROJECTOR:  StoreProjector( cat, LoadProjector(rat) ); break;
            case DT_MESH:       StoreMesh( cat, LoadMesh(rat) ); break;
            case DT_IMAGE:      StoreImage( cat, LoadImage(rat) ); break;
            case DT_LAYOUT:     StoreLayout( cat, LoadLayout(rat) ); break;
            case DT_INSTANCE:   StoreInstance( cat, LoadInstance(rat) ); break;
			case DT_EXTENSION_DATA: StoreExtensionData( cat, LoadExtensionData(rat) ); break;
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
	void CodeRunner::RunCode_Switch(const FScheduledOp& item, const Model* pModel )
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
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(VarAddress, item));
			}
			else
			{
				switch (GetOpDataType(type))
				{
				case DT_BOOL:       StoreBool(item, false); break;
				case DT_INT:        StoreInt(item, 0); break;
				case DT_SCALAR:		StoreScalar(item, 0.0f); break;
				case DT_STRING:		StoreString(item, nullptr); break;
				case DT_COLOUR:		StoreColor(item, FVector4f()); break;
				case DT_PROJECTOR:  StoreProjector(item, FProjector()); break;
				case DT_MESH:       StoreMesh(item, nullptr); break;
				case DT_IMAGE:      StoreImage(item, nullptr); break;
				case DT_LAYOUT:     StoreLayout(item, nullptr); break;
				case DT_INSTANCE:   StoreInstance(item, nullptr); break;
				case DT_EXTENSION_DATA: StoreExtensionData(item, new ExtensionData); break;
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
			int var = LoadInt(FCacheAddress(VarAddress, item));

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
            case DT_BOOL:       StoreBool( cat, LoadBool(rat) ); break;
            case DT_INT:        StoreInt( cat, LoadInt(rat) ); break;
            case DT_SCALAR:     StoreScalar( cat, LoadScalar(rat) ); break;
            case DT_STRING:		StoreString( cat, LoadString( rat ) ); break;
            case DT_COLOUR:		StoreColor( cat, LoadColor( rat ) ); break;
            case DT_PROJECTOR:  StoreProjector( cat, LoadProjector(rat) ); break;
			case DT_MESH:       StoreMesh( cat, LoadMesh(rat) ); break;
            case DT_IMAGE:      StoreImage( cat, LoadImage(rat) ); break;
            case DT_LAYOUT:     StoreLayout( cat, LoadLayout(rat) ); break;
            case DT_INSTANCE:   StoreInstance( cat, LoadInstance(rat) ); break;
			case DT_EXTENSION_DATA: StoreExtensionData( cat, LoadExtensionData(rat) ); break;
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
    void CodeRunner::RunCode_Instance(const FScheduledOp& item, const Model* pModel, uint32 lodMask )
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
                InstancePtrConst pBase = LoadInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = mu::CloneOrTakeOver<Instance>(pBase.get());
                }

                if ( args.value )
                {
					FVector4f value = LoadColor( FCacheAddress(args.value,item) );

                    OP::ADDRESS nameAd = args.name;
                    check(  nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName = pModel->GetPrivate()->m_program.m_constantStrings[ nameAd ].c_str();

                    pResult->GetPrivate()->AddVector( 0, 0, 0, value, strName );
                }
                StoreInstance( item, pResult );
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
                InstancePtrConst pBase = LoadInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
                    pResult = mu::CloneOrTakeOver<Instance>(pBase.get());
                }

                if ( args.value )
                {
                    float value = LoadScalar( FCacheAddress(args.value,item) );

                    OP::ADDRESS nameAd = args.name;
                    check(  nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName = pModel->GetPrivate()->m_program.m_constantStrings[ nameAd ].c_str();

                    pResult->GetPrivate()->AddScalar( 0, 0, 0, value, strName );
                }
                StoreInstance( item, pResult );
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
                    LoadInstance( FCacheAddress( args.instance, item ) );
                InstancePtr pResult;
                if ( !pBase )
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = mu::CloneOrTakeOver<Instance>(pBase.get());
				}

                if ( args.value )
                {
                    Ptr<const String> value =
                        LoadString( FCacheAddress( args.value, item ) );

                    OP::ADDRESS nameAd = args.name;
                    check( nameAd < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num() );
                    const char* strName =
                        pModel->GetPrivate()->m_program.m_constantStrings[nameAd].c_str();

                    pResult->GetPrivate()->AddString( 0, 0, 0, value->GetValue(), strName );
                }
                StoreInstance( item, pResult );
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
                InstancePtrConst pBase = LoadInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = mu::CloneOrTakeOver<Instance>(pBase.get());
				}

                if ( args.value )
                {
                    InstancePtrConst pComp = LoadInstance( FCacheAddress(args.value,item) );

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
                StoreInstance( item, pResult );
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
                InstancePtrConst pBase = LoadInstance( FCacheAddress(args.instance,item) );

                InstancePtr pResult;
				if (pBase)
				{
					pResult = mu::CloneOrTakeOver<Instance>(pBase.get());
				}
				else
				{
					pResult = new Instance();
				}

                // Empty surfaces are ok, they still need to be created, because they may contain
                // additional information like internal or external IDs
                //if ( args.value )
                {
                    InstancePtrConst pSurf = LoadInstance( FCacheAddress(args.value,item) );

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
                    pResult->GetPrivate()->m_lods[0].m_components[0].m_surfaces[sindex].InternalId = args.id;
                    pResult->GetPrivate()->m_lods[0].m_components[0].m_surfaces[sindex].ExternalId = args.ExternalId;
                    pResult->GetPrivate()->m_lods[0].m_components[0].m_surfaces[sindex].SharedId = args.SharedSurfaceId;
                }
                StoreInstance( item, pResult );
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
                            InstancePtrConst pLOD = LoadInstance( FCacheAddress(args.lod[i],item) );

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

                StoreInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case OP_TYPE::IN_ADDEXTENSIONDATA:
		{
			OP::InstanceAddExtensionDataArgs Args = pModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddExtensionDataArgs>(item.At);
			switch (item.Stage)
			{
				case 0:
				{
					// Must pass in an Instance op and ExtensionData op
					check(Args.Instance);
					check(Args.ExtensionData);

					TArray<FScheduledOp> Dependencies;
					Dependencies.Emplace(Args.Instance, item);
					Dependencies.Emplace(Args.ExtensionData, item);

					AddOp(FScheduledOp(item.At, item, 1), Dependencies);

					break;
				}

				case 1:
				{
					// Assemble result
					InstancePtrConst InstanceOpResult = LoadInstance(FCacheAddress(Args.Instance, item));

					InstancePtr Result = mu::CloneOrTakeOver<Instance>(InstanceOpResult.get());

					if (ExtensionDataPtrConst ExtensionData = LoadExtensionData(FCacheAddress(Args.ExtensionData, item)))
					{
						const OP::ADDRESS NameAddress = Args.ExtensionDataName;
						check(NameAddress < (uint32)pModel->GetPrivate()->m_program.m_constantStrings.Num());
						const char* NameString = pModel->GetPrivate()->m_program.m_constantStrings[NameAddress].c_str();

						Result->GetPrivate()->AddExtensionData(ExtensionData, NameString);
					}

					StoreInstance(item, Result);
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
    void CodeRunner::RunCode_InstanceAddResource(const FScheduledOp& item, const TSharedPtr<const Model>& InModel, const Parameters* InParams )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_InstanceAddResource);

		if (!InModel || !m_pSystem)
		{
			return;
		}

		OP_TYPE type = InModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::IN_ADDMESH:
        {
			OP::InstanceAddArgs args = InModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>(item.At);
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
                InstancePtrConst pBase = LoadInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = mu::CloneOrTakeOver<Instance>(pBase.get());
				}

                if ( args.value )
                {
					FResourceID MeshId = m_pSystem->WorkingMemoryManager.GetResourceKey(InModel,InParams,args.relevantParametersListIndex, args.value);
					OP::ADDRESS NameAd = args.name;
					check(NameAd < (uint32)InModel->GetPrivate()->m_program.m_constantStrings.Num());
					const char* Name = InModel->GetPrivate()->m_program.m_constantStrings[NameAd].c_str();
					pResult->GetPrivate()->AddMesh(0, 0, MeshId, Name);
                }
                StoreInstance( item, pResult );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case OP_TYPE::IN_ADDIMAGE:
        {
			OP::InstanceAddArgs args = InModel->GetPrivate()->m_program.GetOpArgs<OP::InstanceAddArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
				// We don't build the resources when building instance: just store ids for them.
				AddOp( FScheduledOp( item.At, item, 1), FScheduledOp( args.instance, item) );
                break;

            case 1:
            {
                InstancePtrConst pBase = LoadInstance( FCacheAddress(args.instance,item) );
                InstancePtr pResult;
                if (!pBase)
                {
                    pResult = new Instance();
                }
                else
                {
					pResult = mu::CloneOrTakeOver<Instance>(pBase.get());
				}

                if ( args.value )
                {
					FResourceID ImageId = m_pSystem->WorkingMemoryManager.GetResourceKey(InModel, InParams, args.relevantParametersListIndex, args.value);
					OP::ADDRESS NameAd = args.name;
					check(NameAd < (uint32)InModel->GetPrivate()->m_program.m_constantStrings.Num());
					const char* Name = InModel->GetPrivate()->m_program.m_constantStrings[NameAd].c_str();
					pResult->GetPrivate()->AddImage(0, 0, 0, ImageId, Name);
                }
                StoreInstance( item, pResult );
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
    void CodeRunner::RunCode_ConstantResource(const FScheduledOp& item, const Model* pModel )
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
            check(program.m_constantMeshes[cat].Value)

            Ptr<const Mesh> SourceConst;
            program.GetConstant(cat, SourceConst);

			check(SourceConst);
			Ptr<Mesh> Source = CreateMesh(SourceConst->GetDataSize());
			Source->CopyFrom(*SourceConst);

            // Set the separate skeleton if necessary
            if (args.skeleton >= 0)
            {
                check(program.m_constantSkeletons.Num() > size_t(args.skeleton));
                Ptr<const Skeleton> pSkeleton = program.m_constantSkeletons[args.skeleton];
                Source->SetSkeleton(pSkeleton);
            }

			if (args.physicsBody >= 0)
			{
                check(program.m_constantPhysicsBodies.Num() > size_t(args.physicsBody));
                Ptr<const PhysicsBody> pPhysicsBody = program.m_constantPhysicsBodies[args.physicsBody];
                Source->SetPhysicsBody(pPhysicsBody);
			}

            StoreMesh(item, Source);
			//UE_LOG(LogMutableCore, Log, TEXT("Set mesh constant %d."), item.At);
            break;
        }

        case OP_TYPE::IM_CONSTANT:
        {
			OP::ResourceConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ResourceConstantArgs>(item.At);
            OP::ADDRESS cat = args.value;

            const FProgram& program = pModel->GetPrivate()->m_program;

			int32 MipsToSkip = item.ExecutionOptions;
            Ptr<const Image> Source;
			program.GetConstant(cat, Source, MipsToSkip, [this](int32 x, int32 y, int32 m, EImageFormat f, EInitializationType i)
				{
					return CreateImage(x, y, m, f, i);
				});

			// Assume the ROM has been loaded previously in a task generated at IssueOp
			check(Source);

            StoreImage( item, Source );
			//UE_LOG(LogMutableCore, Log, TEXT("Set image constant %d."), item.At);
			break;
        }

		case OP_TYPE::ED_CONSTANT:
		{
			OP::ResourceConstantArgs Args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ResourceConstantArgs>(item.At);

            const FProgram& Program = pModel->GetPrivate()->m_program;

			// Assume the ROM has been loaded previously
			ExtensionDataPtrConst SourceConst;
			Program.GetExtensionDataConstant(Args.value, SourceConst);

			check(SourceConst);

            StoreExtensionData(item, SourceConst);
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
	void CodeRunner::RunCode_Mesh(const FScheduledOp& item, const Model* pModel )
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
			{
				AddOp(FScheduledOp(item.At, item, 1),
					FScheduledOp(args.mesh, item),
					FScheduledOp(args.layout, item));
				break;
			}
            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_APPLYLAYOUT)
            		
                Ptr<const Mesh> pBase = LoadMesh(FCacheAddress(args.mesh, item));

                if (pBase)
                {
					Ptr<Mesh> Result = CloneOrTakeOver(pBase);

                    Ptr<const Layout> pLayout = LoadLayout(FCacheAddress(args.layout, item));
                    int texCoordsSet = args.channel;

                    MeshApplyLayout(Result.get(), pLayout.get(), texCoordsSet);
					
					StoreMesh(item, Result);
                }
				else
				{
					StoreMesh(item, nullptr);
				}


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
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);

            switch (item.Stage)
            {
            case 0:
			{
				if (BaseAt && TargetAt)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(BaseAt, item),
						FScheduledOp(TargetAt, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
       	        MUTABLE_CPUPROFILER_SCOPE(ME_DIFFERENCE)

                Ptr<const Mesh> pBase = LoadMesh(FCacheAddress(BaseAt,item));
                Ptr<const Mesh> pTarget = LoadMesh(FCacheAddress(TargetAt,item));

				TArray<MESH_BUFFER_SEMANTIC, TInlineAllocator<8>> Semantics;
				TArray<int32, TInlineAllocator<8>> SemanticIndices;

				uint8 bIgnoreTextureCoords = 0;
				FMemory::Memcpy(&bIgnoreTextureCoords, data, sizeof(uint8)); 
				data += sizeof(uint8);

				uint8 NumChannels = 0;
				FMemory::Memcpy(&NumChannels, data, sizeof(uint8)); 
				data += sizeof(uint8);

                for (uint8 i = 0; i < NumChannels; ++i)
                {
					uint8 Semantic = 0;
					FMemory::Memcpy(&Semantic, data, sizeof(uint8)); 
					data += sizeof(uint8);
					
					uint8 SemanticIndex = 0;
					FMemory::Memcpy(&SemanticIndex, data, sizeof(uint8)); 
					data += sizeof(uint8);

					Semantics.Add(MESH_BUFFER_SEMANTIC(Semantic));
					SemanticIndices.Add(SemanticIndex);
                }

				Ptr<Mesh> Result = CreateMesh();
				bool bOutSuccess = false;
                MeshDifference(Result.get(), pBase.get(), pTarget.get(),
                               NumChannels, Semantics.GetData(), SemanticIndices.GetData(),
                               bIgnoreTextureCoords != 0, bOutSuccess);
				Release(pBase);
				Release(pTarget);

                StoreMesh(item, Result);
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::ME_MORPH:
        {
			const uint8* data = pModel->GetPrivate()->m_program.GetOpArgsPointer(item.At);

			OP::ADDRESS FactorAt = 0;
			FMemory::Memcpy(&FactorAt, data, sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);
			
			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);

			switch (item.Stage)
            {
            case 0:
                if (BaseAt)
                {
                    AddOp(FScheduledOp(item.At, item, 1),
                           FScheduledOp(FactorAt, item));
                }
                else
                {
                    StoreMesh(item, nullptr);
                }
                break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(ME_MORPH_1)

                float Factor = LoadScalar(FCacheAddress(FactorAt, item));

                // Factor goes from -1 to 1 across all targets. [0 - 1] represents positive morphs, while [-1, 0) represent negative morphs.
				Factor = FMath::Clamp(Factor, -1.0f, 1.0f); // Is the factor not in range [-1, 1], it will index a non existing morph.

                FScheduledOpData HeapData;
				HeapData.Interpolate.Bifactor = Factor;
				uint32 dataAddress = uint32(m_heapData.Add(HeapData));

                // No morph
				if (FMath::IsNearlyZero(Factor))
                {                        
                    AddOp(FScheduledOp(item.At, item, 2, dataAddress),
						FScheduledOp(BaseAt, item));
                }
                // The Morph, partial or full
                else
                {
                    // We will need the base again
                    AddOp(FScheduledOp(item.At, item, 2, dataAddress),
						FScheduledOp(BaseAt, item),
						FScheduledOp(TargetAt, item));
                }

                break;
            }

            case 2:
            {
       		    MUTABLE_CPUPROFILER_SCOPE(ME_MORPH_2)

                Ptr<const Mesh> pBase = LoadMesh(FCacheAddress(BaseAt, item));

                // Factor from 0 to 1 between the two targets
                const FScheduledOpData& HeapData = m_heapData[(size_t)item.CustomState];
                float Factor = HeapData.Interpolate.Bifactor;

                if (pBase)
                {
					// No morph
					if (FMath::IsNearlyZero(Factor))
                    {
						StoreMesh(item, pBase);
                    }
					// The Morph, partial or full
                    else 
                    {
                        Ptr<const Mesh> pMorph = LoadMesh(FCacheAddress(TargetAt,item));
						
						if (pMorph)
						{
							Ptr<Mesh> Result = CreateMesh(pBase->GetDataSize());
							bool bOutSuccess = false;
							MeshMorph(Result.get(), pBase.get(), pMorph.get(), Factor, bOutSuccess);

							Release(pMorph);

							if (!bOutSuccess)
							{
								Release(Result);
								StoreMesh(item, pBase);
							}
							else
							{
								Release(pBase);
								StoreMesh(item, Result);
							}
						}
						else
						{
							StoreMesh(item, pBase);
						}
                    }
                }
				else
				{
					StoreMesh(item, nullptr);
				}

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
			{
				AddOp(FScheduledOp(item.At, item, 1),
					FScheduledOp(args.base, item),
					FScheduledOp(args.added, item));
				break;
			}
            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_MERGE_1)

                Ptr<const Mesh> pA = LoadMesh(FCacheAddress(args.base, item));
                Ptr<const Mesh> pB = LoadMesh(FCacheAddress(args.added, item));

                if (pA && pB && pA->GetVertexCount() && pB->GetVertexCount())
                {
					FMeshMergeScratchMeshes Scratch;
					Scratch.FirstReformat = CreateMesh();
					Scratch.SecondReformat = CreateMesh();

					Ptr<Mesh> Result = CreateMesh(pA->GetDataSize() + pB->GetDataSize());

					MeshMerge(Result.get(), pA.get(), pB.get(), !args.newSurfaceID, Scratch);

					Release(Scratch.FirstReformat);
					Release(Scratch.SecondReformat);

                    if (args.newSurfaceID)
                    {
						check(pB->GetSurfaceCount() == 1);
						Result->m_surfaces.Last().m_id = args.newSurfaceID;
                    }

					Release(pA);
					Release(pB);
					StoreMesh(item, Result);
                }
                else if (pA && pA->GetVertexCount())
                {
					Release(pB);
					StoreMesh(item, pA);
                }
                else if (pB && pB->GetVertexCount())
                {
					Ptr<Mesh> Result = CloneOrTakeOver(pB);

                    check(Result->GetSurfaceCount() == 1);

                    if (Result->GetSurfaceCount() > 0 && args.newSurfaceID)
                    {
                        Result->m_surfaces.Last().m_id = args.newSurfaceID;
                    }

					Release(pA);
					StoreMesh(item, Result);
                }
                else
                {
					Release(pA);
					Release(pB);
					StoreMesh(item, CreateMesh());
                }

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
			{
				if (args.base)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.base, item),
						FScheduledOp(args.factor, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
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
                float factor = LoadScalar(FCacheAddress(args.factor, item));

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
                if (bifactor < UE_SMALL_NUMBER)
                {
                    if (min == 0)
                    {
                        // Just the base
						Ptr<const Mesh> pBase = LoadMesh(FCacheAddress(args.base, item));
						StoreMesh(item, pBase);
					}
                    else
                    {
                        // Base with one full morph
                        m_heapData.Add(data);
						AddOp(FScheduledOp(item.At, item, 2, dataAddress),
							FScheduledOp(args.base, item),
							FScheduledOp(args.targets[min-1], item));
					}
				}
                // Just the second of the targets
                else if (bifactor > 1.0f-UE_SMALL_NUMBER)
                {
                    m_heapData.Add(data);
					AddOp(FScheduledOp(item.At, item, 2, dataAddress),
						FScheduledOp(args.base, item),
						FScheduledOp(args.targets[max-1], item));
				}
                // Mix the first target on the base
                else if (min == 0)
                {
                    m_heapData.Add(data);
					AddOp(FScheduledOp(item.At, item, 2, dataAddress),
						FScheduledOp(args.base, item),
						FScheduledOp(args.targets[0], item));
				}
                // Mix two targets on the base
                else
                {
                    m_heapData.Add(data);
					AddOp(FScheduledOp(item.At, item, 2, dataAddress),
						FScheduledOp(args.base, item),
						FScheduledOp(args.targets[min-1], item),
						FScheduledOp(args.targets[max-1], item));
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

                Ptr<const Mesh> pBase = LoadMesh(FCacheAddress(args.base, item));

                if (pBase)
                {
                    // Just the first of the targets
                    if (bifactor < UE_SMALL_NUMBER)
                    {
                        if (min == 0)
                        {
                            // Just the base. It should have been dealt with in the previous stage.
                            check(false);
                        }
                        else
                        {
                            // Base with one full morph
                            Ptr<const Mesh> pMorph = LoadMesh(FCacheAddress(args.targets[min-1], item));
							
							Ptr<Mesh> Result = CreateMesh(pBase->GetDataSize());

							bool bOutSuccess = false;
                            MeshMorph(Result.get(), pBase.get(), pMorph.get(), bOutSuccess);
						
							Release(pMorph);

							if (!bOutSuccess)
							{
								Release(Result);
								StoreMesh(item, pBase);
							}
							else
							{
								Release(pBase);
								StoreMesh(item, Result);
							}
                        }
                    }
                    // Just the second of the targets
                    else if (bifactor > 1.0f-UE_SMALL_NUMBER)
                    {
                        check(max > 0);
                        Ptr<const Mesh> pMorph = LoadMesh(FCacheAddress(args.targets[max-1], item));

                        if (pMorph)
                        {
							Ptr<Mesh> Result = CreateMesh(pBase->GetDataSize());
							
							bool bOutSuccess = false;
                            MeshMorph(Result.get(), pBase.get(), pMorph.get(), bOutSuccess);

							Release(pMorph);
							if (!bOutSuccess)
							{
								Release(Result);
								StoreMesh(item, pBase);
							}
							else
							{
								Release(pBase);
								StoreMesh(item, Result);
							}

                        }
                        else
                        {
							StoreMesh(item, pBase);
                        }
                    }
                    // Mix the first target on the base
                    else if (min == 0)
                    {
                        Ptr<const Mesh> pMorph = LoadMesh(FCacheAddress(args.targets[0], item));
                        if (pMorph)
                        {
							Ptr<Mesh> Result = CreateMesh(pBase->GetDataSize());

							bool bOutSuccess = false;
                            MeshMorph(Result.get(), pBase.get(), pMorph.get(), bifactor, bOutSuccess);

							Release(pMorph);

							if (!bOutSuccess)
							{
								Release(Result);
								StoreMesh(item, pBase);
							}
							else
							{
								Release(pBase);
								StoreMesh(item, Result);
							}
                        }
                        else
                        {
							StoreMesh(item, pBase);
                        }
                    }
                    // Mix two targets on the base
                    else
                    {
                        Ptr<const Mesh> pMin = LoadMesh(FCacheAddress(args.targets[min-1], item));
                        Ptr<const Mesh> pMax = LoadMesh(FCacheAddress(args.targets[max-1], item));

                        if (pMin && pMax)
                        {
							Ptr<Mesh> Result = CreateMesh(pBase->GetDataSize());

							bool bOutSuccess = false;
                            MeshMorph2(Result.get(), pBase.get(), pMin.get(), pMax.get(), bifactor, bOutSuccess);

							Release(pMin);
							Release(pMax);

							if (!bOutSuccess)
							{
								Release(Result);
								StoreMesh(item, pBase);
							}
							else
							{
								Release(pBase);
								StoreMesh(item, Result);
							}
                        }
                        else
                        {
							StoreMesh(item, pBase);
                        }
                    }
                }
				else
				{
					StoreMesh(item, pBase);
				}

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
			{
				AddOp(FScheduledOp(item.At, item, 1),
					FScheduledOp(args.source, item),
					FScheduledOp(args.clip, item));
				break;
			}
            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_MASKCLIPMESH_1)
            		
                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source, item));
                Ptr<const Mesh> pClip = LoadMesh(FCacheAddress(args.clip, item));

                // Only if both are valid.
                if (Source.get() && pClip.get())
                {
					Ptr<Mesh> Result = CreateMesh();

					bool bOutSuccess = false;
                    MeshMaskClipMesh(Result.get(), Source.get(), pClip.get(), bOutSuccess);
					
					Release(Source);
					Release(pClip);
					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, nullptr);
					}
					else
					{
						StoreMesh(item, Result);
					}
                }
				else
				{
					Release(Source);
					Release(pClip);
					StoreMesh(item, nullptr);
				}

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
			{
				AddOp(FScheduledOp(item.At, item, 1),
					FScheduledOp(args.source, item),
					FScheduledOp(args.fragment, item));
				break;
			}
            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_MASKDIFF_1)
            		
                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source, item));
                Ptr<const Mesh> pClip = LoadMesh(FCacheAddress(args.fragment, item));

                // Only if both are valid.
                if (Source.get() && pClip.get())
                {
					Ptr<Mesh> Result = CreateMesh();

					bool bOutSuccess = false;
                    MeshMaskDiff(Result.get(), Source.get(), pClip.get(), bOutSuccess);

					Release(Source);
					Release(pClip);

					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, nullptr);
					}
					else
					{
						StoreMesh(item, Result);
					}
                }
				else
				{
					Release(Source);
					Release(pClip);
					StoreMesh(item, nullptr);
				}

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
			{
				if (args.source && args.format)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.source, item),
						FScheduledOp(args.format, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_FORMAT_1)
            		
                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source,item));
                Ptr<const Mesh> pFormat = LoadMesh(FCacheAddress(args.format,item));

				if (Source)
				{
					uint8 flags = args.buffers;
					if (!pFormat && !(flags & OP::MeshFormatArgs::BT_RESETBUFFERINDICES))
					{
						StoreMesh(item, Source);
					}
					else if (!pFormat)
					{
						Ptr<Mesh> Result = CloneOrTakeOver(Source);

						if (flags & OP::MeshFormatArgs::BT_RESETBUFFERINDICES)
						{
							Result->ResetBufferIndices();
						}

						StoreMesh(item, Result);
					}
					else
					{
						Ptr<Mesh> Result = CreateMesh();

						bool bOutSuccess = false;
						MeshFormat(Result.get(), Source.get(), pFormat.get(),
							true,
							(flags & OP::MeshFormatArgs::BT_VERTEX) != 0,
							(flags & OP::MeshFormatArgs::BT_INDEX) != 0,
							(flags & OP::MeshFormatArgs::BT_FACE) != 0,
							(flags & OP::MeshFormatArgs::BT_IGNORE_MISSING) != 0,
							bOutSuccess);

						check(bOutSuccess);

						if (flags & OP::MeshFormatArgs::BT_RESETBUFFERINDICES)
						{
							Result->ResetBufferIndices();
						}

						Release(Source);
						Release(pFormat);
						StoreMesh(item, Result);
					}
				}
				else
				{
					Release(pFormat);
					StoreMesh(item, nullptr);
				}
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
			{
				if (source)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(source, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_EXTRACTLAYOUTBLOCK_1)

                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(source, item));

                // Access with memcpy necessary for unaligned arm issues.
                uint32 blocks[1024];
				FMemory::Memcpy(blocks, data, sizeof(uint32)*FMath::Min(1024,int(blockCount)));

				if (Source)
				{
					Ptr<Mesh> Result = CreateMesh();
					bool bOutSuccess;
					MeshExtractLayoutBlock(Result.get(), Source.get(), layout, blockCount, blocks, bOutSuccess);

					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, Source);
					}
					else
					{
						Release(Source);
						StoreMesh(item, Result);
					}
				}
				else
				{
					StoreMesh(item, nullptr);
				}
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
			{
				if (args.source)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.source, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}

				break;
			}
            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_EXTRACTFACEGROUP_1)

                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source, item));
				if (Source)
				{
					Ptr<Mesh> Result = CreateMesh();

					bool bOutSuccess = false;
					MeshExtractFaceGroup(Result.get(), Source.get(), args.group, bOutSuccess);

					Release(Source);
					if (!bOutSuccess)
					{
						check(false);
						Release(Result);
						StoreMesh(item, nullptr);
					}
					else
					{
						StoreMesh(item, Result);
					}
				}
				else
				{
					StoreMesh(item, nullptr);
				}
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
			{
				if (args.source)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.source, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_TRANSFORM_1)
            		
                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source,item));

                const mat4f& mat = pModel->GetPrivate()->m_program.m_constantMatrices[args.matrix];

				Ptr<Mesh> Result = CreateMesh(Source ? Source->GetDataSize() : 0);

				bool bOutSuccess = false;
                MeshTransform(Result.get(), Source.get(), ToUnreal(mat), bOutSuccess);

				if (!bOutSuccess)
				{
					Release(Result);
					StoreMesh(item, Source);
				}
				else
				{
					Release(Source);
					StoreMesh(item, Result);
				}
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
			{
				if (args.source)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.source, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_CLIPMORPHPLANE_1)
            		
                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source, item));

                check(args.morphShape < (uint32)pModel->GetPrivate()->m_program.m_constantShapes.Num());

                // Should be an ellipse
                const FShape& morphShape = pModel->GetPrivate()->m_program.m_constantShapes[args.morphShape];

                const mu::vec3f& origin = morphShape.position;
                const mu::vec3f& normal = morphShape.up;

                if (args.vertexSelectionType == OP::MeshClipMorphPlaneArgs::VS_SHAPE)
                {
                    check(args.vertexSelectionShapeOrBone < (uint32)pModel->GetPrivate()->m_program.m_constantShapes.Num());

                    // Should be None or an axis aligned box
                    const FShape& selectionShape = pModel->GetPrivate()->m_program.m_constantShapes[args.vertexSelectionShapeOrBone];

					Ptr<Mesh> Result = CreateMesh(Source ? Source->GetDataSize() : 0);

					bool bOutSuccess = false;
					MeshClipMorphPlane(Result.get(), Source.get(), origin, normal, args.dist, args.factor, morphShape.size[0], morphShape.size[1], morphShape.size[2], selectionShape, bOutSuccess, INDEX_NONE, -1);
					
					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, Source);
					}
					else
					{
						Release(Source);
						StoreMesh(item, Result);
					}
                }

				else if (args.vertexSelectionType == OP::MeshClipMorphPlaneArgs::VS_BONE_HIERARCHY)
				{
					FShape selectionShape;
					selectionShape.type = (uint8)FShape::Type::None;

					Ptr<Mesh> Result = CreateMesh(Source->GetDataSize());

					bool bOutSuccess = false;
					MeshClipMorphPlane(Result.get(), Source.get(), origin, normal, args.dist, args.factor, morphShape.size[0], morphShape.size[1], morphShape.size[2], selectionShape, bOutSuccess, args.vertexSelectionShapeOrBone, args.maxBoneRadius);

					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, Source);
					}
					else
					{
						Release(Source);
						StoreMesh(item, Result);
					}
                }
                else
                {
                    // No vertex selection
                    FShape selectionShape;
                    selectionShape.type = (uint8)FShape::Type::None;

					Ptr<Mesh> Result = CreateMesh(Source ? Source->GetDataSize() : 0);

					bool bOutSuccess = false;
					MeshClipMorphPlane(Result.get(), Source.get(), origin, normal, args.dist, args.factor, morphShape.size[0], morphShape.size[1], morphShape.size[2], selectionShape, bOutSuccess, INDEX_NONE, -1.0f);

					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, Source);
					}
					else
					{
						Release(Source);
						StoreMesh(item, Result);
					}
                }

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
			{
				if (args.source)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.source, item),
						FScheduledOp(args.clipMesh, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}

				break;
			}
            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_CLIPWITHMESH_1)

                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source, item));
                Ptr<const Mesh> pClip = LoadMesh(FCacheAddress(args.clipMesh, item));

                // Only if both are valid.
                if (Source && pClip)
                {
					Ptr<Mesh> Result = CreateMesh(Source->GetDataSize());

					bool bOutSuccess = false;
                    MeshClipWithMesh(Result.get(), Source.get(), pClip.get(), bOutSuccess);

					Release(pClip);
					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, Source);
					}
					else
					{
						Release(Source);
						StoreMesh(item, Result);
					}
                }
                else
                {
					Release(pClip);
					StoreMesh(item, Source);
                }

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
			{
				if (args.mesh)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.mesh, item),
						FScheduledOp(args.clipShape, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_CLIPDEFORM_1)

				Ptr<const Mesh> BaseMesh = LoadMesh(FCacheAddress(args.mesh, item));
				Ptr<const Mesh> ClipShape = LoadMesh(FCacheAddress(args.clipShape, item));

				if (BaseMesh && ClipShape)
				{
					Ptr<Mesh> Result = CreateMesh(BaseMesh->GetDataSize());

					bool bOutSuccess = false;
					MeshClipDeform(Result.get(), BaseMesh.get(), ClipShape.get(), args.clipWeightThreshold, bOutSuccess);

					Release(ClipShape);

					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, BaseMesh);
					}
					else
					{
						Release(BaseMesh);
						StoreMesh(item, Result);
					}
				}
				else
				{
					Release(ClipShape);
					StoreMesh(item, BaseMesh);
				}

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
			{
				if (args.source)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.source, item),
						FScheduledOp(args.reference, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
    			MUTABLE_CPUPROFILER_SCOPE(ME_REMAPINDICES_1)

                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source,item));
                Ptr<const Mesh> pReference = LoadMesh(FCacheAddress(args.reference,item));

                // Only if both are valid.
                MeshPtr Result = CreateMesh(Source ? Source->GetDataSize() : 0);
                
				bool bOutSuccess = false;
				if (Source && pReference)
                {	
                    MeshRemapIndices(Result.get(), Source.get(), pReference.get(), bOutSuccess);
                }
			
				Release(pReference);
				if (!bOutSuccess)
				{
					Release(Result);
					StoreMesh(item, Source);
				}
				else
				{	
					Release(Source);
					StoreMesh(item, Result);
				}

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
			{
				if (args.base)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.base, item),
						FScheduledOp(args.pose, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
          		MUTABLE_CPUPROFILER_SCOPE(ME_APPLYPOSE_1)

                Ptr<const Mesh> pBase = LoadMesh(FCacheAddress(args.base, item));
                Ptr<const Mesh> pPose = LoadMesh(FCacheAddress(args.pose, item));

                // Only if both are valid.
                if (pBase && pPose)
                {
					Ptr<Mesh> Result = CreateMesh(pBase->GetSkeleton() ? pBase->GetDataSize() : 0);

					bool bOutSuccess = false;
					MeshApplyPose(Result.get(), pBase.get(), pPose.get(), bOutSuccess);

					Release(pPose);
					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, pBase);
					}
					else
					{
						Release(pBase);
						StoreMesh(item, Result);
					}
                }
                else
                {
					Release(pPose);
					StoreMesh(item, pBase);
                }

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
			{
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
					StoreMesh(item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_GEOMETRYOPERATION_1)
				
				Ptr<const Mesh> MeshA = LoadMesh(FCacheAddress(args.meshA, item));
				Ptr<const Mesh> MeshB = LoadMesh(FCacheAddress(args.meshB, item));
				float ScalarA = LoadScalar(FCacheAddress(args.scalarA, item));
				float ScalarB = LoadScalar(FCacheAddress(args.scalarB, item));

				Ptr<Mesh> Result = CreateMesh(MeshA ? MeshA->GetDataSize() : 0);

				bool bOutSuccess = false;
				MeshGeometryOperation(Result.get(), MeshA.get(), MeshB.get(), ScalarA, ScalarB, bOutSuccess);

				Release(MeshA);
				Release(MeshB);

				if (!bOutSuccess)
				{
					Release(Result);
					StoreMesh(item, nullptr);
				}
				else
				{
					StoreMesh(item, Result);
				}

				break;
			}

			default:
				check(false);
			}

			break;
		}


		case OP_TYPE::ME_BINDSHAPE:
		{
			OP::MeshBindShapeArgs Args = pModel->GetPrivate()->m_program.GetOpArgs<OP::MeshBindShapeArgs>(item.At);
			const uint8* Data = pModel->GetPrivate()->m_program.GetOpArgsPointer(item.At);

			switch (item.Stage)
			{
			case 0:
			{
				if (Args.mesh)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(Args.mesh, item),
						FScheduledOp(Args.shape, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_BINDSHAPE_1)
				Ptr<const Mesh> BaseMesh = LoadMesh(FCacheAddress(Args.mesh, item));
				Ptr<const Mesh> Shape = LoadMesh(FCacheAddress(Args.shape, item));
				
				EShapeBindingMethod BindingMethod = static_cast<EShapeBindingMethod>(Args.bindingMethod); 

				if (BindingMethod == EShapeBindingMethod::ReshapeClosestProject)
				{ 
					// Bones are stored after the args
					Data += sizeof(Args);

					// Rebuilding array of bone names ----
					int32 NumBones;
					FMemory::Memcpy(&NumBones, Data, sizeof(int32)); 
					Data += sizeof(int32);
					
					TArray<uint16> BonesToDeform;
					BonesToDeform.SetNumUninitialized(NumBones);
					FMemory::Memcpy(BonesToDeform.GetData(), Data, NumBones * sizeof(uint16));
					Data += NumBones * sizeof(uint16);

					int32 NumPhysicsBodies;
					FMemory::Memcpy(&NumPhysicsBodies, Data, sizeof(int32)); 
					Data += sizeof(int32);

					TArray<uint16> PhysicsToDeform;
					PhysicsToDeform.SetNumUninitialized(NumPhysicsBodies);
					FMemory::Memcpy(PhysicsToDeform.GetData(), Data, NumPhysicsBodies * sizeof(uint16));
					Data += NumPhysicsBodies * sizeof(uint16);

					const EMeshBindShapeFlags BindFlags = static_cast<EMeshBindShapeFlags>(Args.flags);

					FMeshBindColorChannelUsages ColorChannelUsages;
					FMemory::Memcpy(&ColorChannelUsages, &Args.ColorUsage, sizeof(ColorChannelUsages));
					static_assert(sizeof(ColorChannelUsages) == sizeof(Args.ColorUsage));

					Ptr<Mesh> BindMeshResult = CreateMesh();

					bool bOutSuccess = false;
					MeshBindShapeReshape(BindMeshResult.get(), BaseMesh.get(), Shape.get(), BonesToDeform, PhysicsToDeform, BindFlags, ColorChannelUsages, bOutSuccess);
				
					Release(Shape);
					// not success indicates nothing has bond so the base mesh can be reused.
					if (!bOutSuccess)
					{
						Release(BindMeshResult);
						StoreMesh(item, BaseMesh);
					}
					else
					{
						if (!EnumHasAnyFlags(BindFlags, EMeshBindShapeFlags::ReshapeVertices))
						{
							Ptr<Mesh> BindMeshNoVertsResult = CloneOrTakeOver(BaseMesh);
							BindMeshNoVertsResult->m_AdditionalBuffers = MoveTemp(BindMeshResult->m_AdditionalBuffers);

							Release(BaseMesh);
							Release(BindMeshResult);
							StoreMesh(item, BindMeshNoVertsResult);
						}
						else
						{
							Release(BaseMesh);
							StoreMesh(item, BindMeshResult);
						}
					}
				}	
				else
				{
					Ptr<Mesh> Result = CreateMesh(BaseMesh ? BaseMesh->GetDataSize() : 0);

					bool bOutSuccess = false;
					MeshBindShapeClipDeform(Result.get(), BaseMesh.get(), Shape.get(), BindingMethod, bOutSuccess);

					Release(Shape);
					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, BaseMesh);
					}
					else
					{
						Release(BaseMesh);
						StoreMesh(item, Result);
					}
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
			{
				if (args.mesh)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.mesh, item),
						FScheduledOp(args.shape, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_APPLYSHAPE_1)
					
				Ptr<const Mesh> BaseMesh = LoadMesh(FCacheAddress(args.mesh, item));
				Ptr<const Mesh> Shape = LoadMesh(FCacheAddress(args.shape, item));

				const EMeshBindShapeFlags ReshapeFlags = static_cast<EMeshBindShapeFlags>(args.flags);
				const bool bReshapeVertices = EnumHasAnyFlags(ReshapeFlags, EMeshBindShapeFlags::ReshapeVertices);

				Ptr<Mesh> ReshapedMeshResult = CreateMesh(BaseMesh ? BaseMesh->GetDataSize() : 0);

				bool bOutSuccess = false;
				MeshApplyShape(ReshapedMeshResult.get(), BaseMesh.get(), Shape.get(), ReshapeFlags, bOutSuccess);

				Release(Shape);
				
				if (!bOutSuccess)
				{
					Release(ReshapedMeshResult);
					StoreMesh(item, BaseMesh);
				}
				else
				{
					if (!bReshapeVertices)
					{
						// Clone without Skeleton, Physics or Poses 
						EMeshCopyFlags CopyFlags = ~(
							EMeshCopyFlags::WithSkeleton |
							EMeshCopyFlags::WithPhysicsBody |
							EMeshCopyFlags::WithAdditionalPhysics |
							EMeshCopyFlags::WithPoses);

						Ptr<Mesh> NoVerticesReshpedMesh = CloneOrTakeOver(BaseMesh);

						NoVerticesReshpedMesh->SetSkeleton(ReshapedMeshResult->GetSkeleton().get());
						NoVerticesReshpedMesh->SetPhysicsBody(ReshapedMeshResult->GetPhysicsBody().get());
						NoVerticesReshpedMesh->AdditionalPhysicsBodies = ReshapedMeshResult->AdditionalPhysicsBodies;
						NoVerticesReshpedMesh->BonePoses = ReshapedMeshResult->BonePoses;

						Release(BaseMesh);
						Release(ReshapedMeshResult);
						StoreMesh(item, NoVerticesReshpedMesh);
					}
					else
					{
						Release(BaseMesh);
						StoreMesh(item, ReshapedMeshResult);
					}
				}
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
					StoreMesh(item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_MORPHRESHAPE_1)
					
				Ptr<const Mesh> MorphedMesh = LoadMesh(FCacheAddress(Args.Morph, item));
				Ptr<const Mesh> ReshapeMesh = LoadMesh(FCacheAddress(Args.Reshape, item));

				if (ReshapeMesh && MorphedMesh)
				{
					// Copy without Skeleton, Physics or Poses 
					EMeshCopyFlags CopyFlags = ~(
							EMeshCopyFlags::WithSkeleton    | 
							EMeshCopyFlags::WithPhysicsBody | 
							EMeshCopyFlags::WithPoses);

					Ptr<Mesh> Result = CreateMesh(MorphedMesh->GetDataSize());
					Result->CopyFrom(*MorphedMesh, CopyFlags);

					Result->SetSkeleton(ReshapeMesh->GetSkeleton().get());
					Result->SetPhysicsBody(ReshapeMesh->GetPhysicsBody().get());
					Result->BonePoses = ReshapeMesh->BonePoses;

					Release(MorphedMesh);
					Release(ReshapeMesh);
					StoreMesh(item, Result);
				}
				else
				{
					StoreMesh(item, MorphedMesh);
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
			{
				if (args.source)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.source, item),
						FScheduledOp(args.skeleton, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_SETSKELETON_1)
            		
                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source, item));
                Ptr<const Mesh> pSkeleton = LoadMesh(FCacheAddress(args.skeleton, item));

                // Only if both are valid.
                if (Source && pSkeleton)
                {
                    if ( Source->GetSkeleton()
                         &&
                         !Source->GetSkeleton()->BoneIds.IsEmpty() )
                    {
                        // For some reason we already have bone data, so we can't just overwrite it
                        // or the skinning may break. This may happen because of a problem in the
                        // optimiser that needs investigation.
                        // \TODO Be defensive, for now.
                        UE_LOG(LogMutableCore, Warning, TEXT("Performing a MeshRemapSkeleton, instead of MeshSetSkeletonData because source mesh already has some skeleton."));

						Ptr<Mesh> Result = CreateMesh(Source->GetDataSize());

						bool bOutSuccess = false;
                        MeshRemapSkeleton(Result.get(), Source.get(), pSkeleton->GetSkeleton().get(), bOutSuccess);

						Release(pSkeleton);

                        if (!bOutSuccess)
                        {
							Release(Result);
							StoreMesh(item, Source);
                        }
						else
						{
							//Result->GetPrivate()->CheckIntegrity();
							Release(Source);
							StoreMesh(item, Result);
						}
                    }
                    else
                    {
						Ptr<Mesh> Result = CloneOrTakeOver(Source);

                        Result->SetSkeleton(pSkeleton->GetSkeleton().get());

						//Result->GetPrivate()->CheckIntegrity();
						Release(pSkeleton);
						StoreMesh(item, Result);
                    }
                }
                else
                {
					Release(pSkeleton);
					StoreMesh(item, Source);
                }

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
            FMemory::Memcpy(&source,data,sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);

            TArray<FScheduledOp> conditions;
			TArray<OP::ADDRESS> masks;

            uint16 removes;
			FMemory::Memcpy(&removes,data,sizeof(uint16)); 
			data += sizeof(uint16);

            for( uint16 r=0; r<removes; ++r)
            {
                OP::ADDRESS condition;
				FMemory::Memcpy(&condition,data,sizeof(OP::ADDRESS)); 
				data += sizeof(OP::ADDRESS);
                
				conditions.Emplace(condition, item);

                OP::ADDRESS mask;
				FMemory::Memcpy(&mask,data,sizeof(OP::ADDRESS)); 
				data += sizeof(OP::ADDRESS);

                masks.Add(mask);
            }


            // Schedule next stages
            switch (item.Stage)
            {
            case 0:
			{
				if (source)
				{
					// Request the conditions
					AddOp(FScheduledOp(item.At, item, 1), conditions);
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
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
                        value = LoadBool(FCacheAddress(conditions[r].At, item));
                    }

                    if (value)
                    {
                        deps.Emplace(masks[r], item);
                    }
                }

                if (source)
                {
					AddOp(FScheduledOp(item.At, item, 2), deps);
				}
                break;
            }

            case 2:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_REMOVEMASK_2)
            	
                // \todo: single remove operation with all masks?
                Ptr<const Mesh> Source = LoadMesh(FCacheAddress(source, item));

				if (Source)
				{
					Ptr<Mesh> Result = CreateMesh(Source->GetDataSize());
					Result->CopyFrom(*Source);

					Release(Source);

					for (int32 r = 0; r < conditions.Num(); ++r)
					{
						// If there is no expression, we'll assume true.
						bool value = true;
						if (conditions[r].At)
						{
							value = LoadBool(FCacheAddress(conditions[r].At, item));
						}

						if (value)
						{
							Ptr<const Mesh> Mask = LoadMesh(FCacheAddress(masks[r], item));
							if (Mask)
							{
								//MeshRemoveMask will make a copy of Result, try to make room for it. 
								Ptr<Mesh> IterResult = CreateMesh(Result->GetDataSize());

								bool bOutSuccess = false;
								MeshRemoveMask(IterResult.get(), Result.get(), Mask.get(), bOutSuccess);

								Release(Mask);

								if (!bOutSuccess)
								{
									Release(IterResult);
								}
								else
								{
									Swap(Result, IterResult);
									Release(IterResult);
								}
							}
						}
					}

					StoreMesh(item, Result);
				}
				else
				{
					StoreMesh(item, nullptr);
				}

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
			{
				if (args.mesh)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.mesh, item),
						FScheduledOp(args.projector, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_PROJECT_1)

                Ptr<const Mesh> pMesh = LoadMesh(FCacheAddress(args.mesh,item));
                const FProjector Projector = LoadProjector(FCacheAddress(args.projector, item));

                // Only if both are valid.
                if (pMesh && pMesh->GetVertexBuffers().GetBufferCount() > 0)
                {
					Ptr<Mesh> Result = CreateMesh();

					bool bOutSuccess = false;
					MeshProject(Result.get(), pMesh.get(), Projector, bOutSuccess);

					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(item, pMesh);
					}
					else
					{	
//						Result->GetPrivate()->CheckIntegrity();
						Release(pMesh);
						StoreMesh(item, Result);
					}
                }
				else
				{
					Release(pMesh);
					StoreMesh(item, nullptr);
				}

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
			{
				if (args.source)
				{
					AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.source, item));
				}
				else
				{
					StoreMesh(item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_OPTIMIZESKINNING_1)

				Ptr<const Mesh> Source = LoadMesh(FCacheAddress(args.source, item));

				Ptr<Mesh> Result = CreateMesh();

				bool bOutSuccess = false;
				MeshOptimizeSkinning(Result.get(), Source.get(), bOutSuccess);

				if (!bOutSuccess)
				{
					Release(Result);
					StoreMesh(item, Source);
				}
				else
				{
					Release(Source);
					StoreMesh(item, Result);
				}

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
    void CodeRunner::RunCode_Image(const FScheduledOp& item, const Parameters* pParams, const Model* pModel )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Image);

		FImageOperator ImOp = MakeImageOperator(this);

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

			if (ExecutionStrategy == EExecutionStrategy::MinimizeMemory)
			{
				switch (item.Stage)
				{
				case 0:
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.base, item));
					break;

				case 1:
					// Request the rest of the data.
					AddOp(FScheduledOp(item.At, item, 2),
						FScheduledOp(args.blended, item),
						FScheduledOp(args.mask, item));
					break;

				case 2:
					// This has been moved to a task. It should have been intercepted in IssueOp.
					check(false);
					break;

				default:
					check(false);
				}
			}
			else
			{
				switch (item.Stage)
				{
				case 0:
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp(args.base, item),
						FScheduledOp(args.blended, item),
						FScheduledOp(args.mask, item));
					break;

				case 1:
					// This has been moved to a task. It should have been intercepted in IssueOp.
					check(false);
					break;

				default:
					check(false);
				}
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
						Iterations = LoadInt(RangeAddress);
                    }
                    else if (RangeSizeType == DT_SCALAR)
                    {
						Iterations = int32( LoadScalar(RangeAddress) );
                    }
                }

				Ptr<const Image> Base = LoadImage(FCacheAddress(args.base, item));

				if (Iterations <= 0)
				{
					// There are no layers: return the base
					StoreImage(item, Base);
				}
				else
				{
					// Store the base
					Ptr<Image> New = CloneOrTakeOver(Base);
					EImageFormat InitialBaseFormat = New->GetFormat();

					// Reset relevancy map.
					New->m_flags &= ~Image::EImageFlags::IF_HAS_RELEVANCY_MAP;

					// This shouldn't happen in optimised models, but it could happen in editors, etc.
					// \todo: raise a performance warning?
					EImageFormat BaseFormat = GetUncompressedFormat(New->GetFormat());
					if (New->GetFormat() != BaseFormat)
					{
						Ptr<Image> Formatted = CreateImage( New->GetSizeX(), New->GetSizeY(), New->GetLODCount(), BaseFormat, EInitializationType::NotInitialized );

						bool bSuccess = false;
						ImOp.ImagePixelFormat(bSuccess, m_pSettings->ImageCompressionQuality, Formatted.get(), New.get());
						check(bSuccess); // Decompression cannot fail

						Release(New);
						New = Formatted;
					}

					FScheduledOpData Data;
					Data.Resource = New;
					Data.MultiLayer.Iterations = Iterations;
					Data.MultiLayer.OriginalBaseFormat = InitialBaseFormat;
					Data.MultiLayer.bBlendOnlyOneMip = false;
					int32 DataPos = m_heapData.Add(Data);

					// Request the first layer
					int32 CurrentIteration = 0;
					FScheduledOp ItemCopy = item;
					ExecutionIndex Index = GetMemory().GetRangeIndex(item.ExecutionIndex);
					Index.SetFromModelRangeIndex(args.rangeId, CurrentIteration);
					ItemCopy.ExecutionIndex = GetMemory().GetRangeIndexIndex(Index);
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
                ExecutionIndex index = GetMemory().GetRangeIndex( item.ExecutionIndex );
				
                {
                    index.SetFromModelRangeIndex( args.rangeId, CurrentIteration);
                    itemCopy.ExecutionIndex = GetMemory().GetRangeIndexIndex(index);
					itemCopy.CustomState = 0;

                    Ptr<const Image> Blended = LoadImage( FCacheAddress(args.blended,itemCopy) );

                    // This shouldn't happen in optimised models, but it could happen in editors, etc.
                    // \todo: raise a performance warning?
                    if (Blended && Blended->GetFormat()!=Base->GetFormat() )
                    {
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_BlendedReformat);

						Ptr<Image> Formatted = CreateImage(Blended->GetSizeX(), Blended->GetSizeY(), Blended->GetLODCount(), Base->GetFormat(), EInitializationType::NotInitialized);

						bool bSuccess = false;
						ImOp.ImagePixelFormat(bSuccess, m_pSettings->ImageCompressionQuality, Formatted.get(), Blended.get());
						check(bSuccess);

						Release(Blended);
						Blended = Formatted;
                    }

					// TODO: This shouldn't happen, but be defensive.
					FImageSize ResultSize = Base->GetSize();
					if (Blended && Blended->GetSize() != ResultSize)
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_BlendedFixForMultilayer);

						Ptr<Image> Resized = CreateImage(ResultSize[0], ResultSize[1], Blended->GetLODCount(), Blended->GetFormat(), EInitializationType::NotInitialized);
						ImOp.ImageResizeLinear(Resized.get(), 0, Blended.get());
						Release(Blended);
						Blended = Resized;
					}

					if (Blended->GetLODCount() < Base->GetLODCount())
					{
						Data.MultiLayer.bBlendOnlyOneMip = true;
					}

					bool bApplyColorBlendToAlpha = false;

					bool bDone = false;

					// This becomes true if we need to update the mips of the resulting image
					// This could happen in the base image has mips, but one of the blended one doesn't.
					bool bBlendOnlyOneMip = Data.MultiLayer.bBlendOnlyOneMip;
					bool bUseBlendSourceFromBlendAlpha = false; // (Args.flags& OP::ImageLayerArgs::F_BLENDED_RGB_FROM_ALPHA) != 0;

					if (!args.mask && args.bUseMaskFromBlended
						&&
						args.blendType == uint8(EBlendType::BT_BLEND)
						&&
						args.blendTypeAlpha == uint8(EBlendType::BT_LIGHTEN) )
					{
						// This is a frequent critical-path case because of multilayer projectors.
						bDone = true;
					
						constexpr bool bUseVectorImpl = false;
						if constexpr (bUseVectorImpl)
						{
							BufferLayerCompositeVector<VectorBlendChannelMasked, VectorLightenChannel, false>(Base.get(), Blended.get(), bBlendOnlyOneMip, args.BlendAlphaSourceChannel);
						}
						else
						{
							BufferLayerComposite<BlendChannelMasked, LightenChannel, false>(Base.get(), Blended.get(), bBlendOnlyOneMip, args.BlendAlphaSourceChannel);
						}
					}

                    if (!bDone && args.mask)
                    {
                        Ptr<const Image> Mask = LoadImage( FCacheAddress(args.mask,itemCopy) );

						// TODO: This shouldn't happen, but be defensive.
						if (Mask && Mask->GetSize() != ResultSize)
						{
							MUTABLE_CPUPROFILER_SCOPE(ImageResize_MaskFixForMultilayer);

							Ptr<Image> Resized = CreateImage(ResultSize[0], ResultSize[1], Mask->GetLODCount(), Mask->GetFormat(), EInitializationType::NotInitialized);
							ImOp.ImageResizeLinear(Resized.get(), 0, Mask.get());
							Release(Mask);
							Mask = Resized;
						}

						// Not implemented yet
						check(!bUseBlendSourceFromBlendAlpha);

                        switch (EBlendType(args.blendType))
                        {
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
                        case EBlendType::BT_SOFTLIGHT: BufferLayer<SoftLightChannelMasked, SoftLightChannel, false>( Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_HARDLIGHT: BufferLayer<HardLightChannelMasked, HardLightChannel, false>(Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_BURN: BufferLayer<BurnChannelMasked, BurnChannel, false>(Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_DODGE: BufferLayer<DodgeChannelMasked, DodgeChannel, false>(Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_SCREEN: BufferLayer<ScreenChannelMasked, ScreenChannel, false>(Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_OVERLAY: BufferLayer<OverlayChannelMasked, OverlayChannel, false>(Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_LIGHTEN: BufferLayer<LightenChannelMasked, LightenChannel, false>(Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_MULTIPLY: BufferLayer<MultiplyChannelMasked, MultiplyChannel, false>(Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_BLEND: BufferLayer<BlendChannelMasked, BlendChannel, false>(Base->GetData(), Base.get(), Mask.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        default: check(false);
                        }

						Release(Mask);
                    }
					else if (!bDone && args.bUseMaskFromBlended)
					{
						// Not implemented yet
						check(!bUseBlendSourceFromBlendAlpha);

						switch (EBlendType(args.blendType))
						{
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
						case EBlendType::BT_SOFTLIGHT: BufferLayerEmbeddedMask<SoftLightChannelMasked, SoftLightChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_HARDLIGHT: BufferLayerEmbeddedMask<HardLightChannelMasked, HardLightChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_BURN: BufferLayerEmbeddedMask<BurnChannelMasked, BurnChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_DODGE: BufferLayerEmbeddedMask<DodgeChannelMasked, DodgeChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_SCREEN: BufferLayerEmbeddedMask<ScreenChannelMasked, ScreenChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_OVERLAY: BufferLayerEmbeddedMask<OverlayChannelMasked, OverlayChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_LIGHTEN: BufferLayerEmbeddedMask<LightenChannelMasked, LightenChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_MULTIPLY: BufferLayerEmbeddedMask<MultiplyChannelMasked, MultiplyChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_BLEND: BufferLayerEmbeddedMask<BlendChannelMasked, BlendChannel, false>(Base->GetData(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						default: check(false);
						}
					}
                    else if (!bDone)
                    {
                        switch (EBlendType(args.blendType))
                        {
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
                        case EBlendType::BT_SOFTLIGHT: BufferLayer<SoftLightChannel, false>(Base.get(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_HARDLIGHT: BufferLayer<HardLightChannel, false>(Base.get(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_BURN: BufferLayer<BurnChannel, false>(Base.get(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_DODGE: BufferLayer<DodgeChannel, false>(Base.get(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_SCREEN: BufferLayer<ScreenChannel, false>(Base.get(), Base.get(),  Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_OVERLAY: BufferLayer<OverlayChannel, false>(Base.get(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_LIGHTEN: BufferLayer<LightenChannel, false>(Base.get(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_MULTIPLY: BufferLayer<MultiplyChannel, false>(Base.get(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_BLEND: BufferLayer<BlendChannel, false>(Base.get(), Base.get(), Blended.get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        default: check(false);
                        }
                    }

					// Apply the separate blend operation for alpha
					if (!bDone && !bApplyColorBlendToAlpha && args.blendTypeAlpha != uint8(EBlendType::BT_NONE) )
					{
						// Separate alpha operation ignores the mask.
						switch (EBlendType(args.blendTypeAlpha))
						{
						case EBlendType::BT_SOFTLIGHT: BufferLayerInPlace<SoftLightChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_HARDLIGHT: BufferLayerInPlace<HardLightChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_BURN: BufferLayerInPlace<BurnChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_DODGE: BufferLayerInPlace<DodgeChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_SCREEN: BufferLayerInPlace<ScreenChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_OVERLAY: BufferLayerInPlace<OverlayChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_LIGHTEN: BufferLayerInPlace<LightenChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_MULTIPLY: BufferLayerInPlace<MultiplyChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_BLEND: BufferLayerInPlace<BlendChannel, false, 1>(Base.get(), Blended.get(), bBlendOnlyOneMip, 3, args.BlendAlphaSourceChannel); break;
						default: check(false);
						}
					}

					Release(Blended);
				}

				// Are we done?
				if (CurrentIteration + 1 == Iterations)
				{
					if (Data.MultiLayer.bBlendOnlyOneMip)
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageLayer_MipFix);
						FMipmapGenerationSettings DummyMipSettings{};
						ImageMipmapInPlace(m_pSettings->ImageCompressionQuality, Base.get(), DummyMipSettings);
					}

					// TODO: Reconvert to OriginalBaseFormat if necessary?

					Data.Resource = nullptr;
					StoreImage(item, Base);
					break;
				}
				else
				{
					// Request a new layer
					++CurrentIteration;
					FScheduledOp ItemCopy = item;
					ExecutionIndex Index = GetMemory().GetRangeIndex(item.ExecutionIndex);
					Index.SetFromModelRangeIndex(args.rangeId, CurrentIteration);
					ItemCopy.ExecutionIndex = GetMemory().GetRangeIndexIndex(Index);
					AddOp(FScheduledOp(item.At, item, 2+CurrentIteration, item.CustomState), FScheduledOp(args.blended, ItemCopy), FScheduledOp(args.mask, ItemCopy));

				}

                break;
            }

            } // switch stage

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
					StoreImage(item, nullptr);
				}
				break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(IM_NORMALCOMPOSITE_1)

				Ptr<const Image> Base = LoadImage(FCacheAddress(args.base, item));
				Ptr<const Image> Normal = LoadImage(FCacheAddress(args.normal, item));

				if (Normal->GetLODCount() < Base->GetLODCount())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageNormalComposite_EmergencyFix);

					int levelCount = Base->GetLODCount();
					ImagePtr Dest = CreateImage(Normal->GetSizeX(), Normal->GetSizeY(), levelCount, Normal->GetFormat(), EInitializationType::NotInitialized);

					FMipmapGenerationSettings mipSettings{};
					ImOp.ImageMipmap(m_pSettings->ImageCompressionQuality, Dest.get(), Normal.get(), levelCount, mipSettings);

					Release(Normal);
					Normal = Dest;
				}


                ImagePtr Result = CreateImage( Base->GetSizeX(), Base->GetSizeY(), Base->GetLODCount(), Base->GetFormat(), EInitializationType::NotInitialized);
				ImageNormalComposite(Result.get(), Base.get(), Normal.get(), args.mode, args.power);

				Release(Base);
				Release(Normal);
				StoreImage(item, Result);
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
                AddOp( FScheduledOp( item.At, item, 1), FScheduledOp( args.source, item) );
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
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
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
            	
                Ptr<const Image> Base = LoadImage( FCacheAddress(args.source,item) );
                Ptr<const Image> SizeBase = LoadImage( FCacheAddress(args.sizeSource,item) );
				FImageSize DestSize = SizeBase->GetSize();
				Release(SizeBase);

                if ( Base->GetSize()!=DestSize )
                {
					int32 BaseLODCount = Base->GetLODCount();
					Ptr<Image> Result = CreateImage(DestSize[0], DestSize[1], BaseLODCount, Base->GetFormat(), EInitializationType::NotInitialized);
					ImOp.ImageResizeLinear( Result.get(), m_pSettings->ImageCompressionQuality, Base.get());
					Release(Base);

                    // If the source image had mips, generate them as well for the resized image.
                    // This shouldn't happen often since "ResizeLike" should be usually optimised out
                    // during model compilation. The mipmap generation below is not very precise with
                    // the number of mips that are needed and will probably generate too many
                    bool bSourceHasMips = BaseLODCount>1;
                    if (bSourceHasMips)
                    {
                        int levelCount = Image::GetMipmapCount( Result->GetSizeX(), Result->GetSizeY() );
                        Ptr<Image> Mipmapped = CreateImage( Result->GetSizeX(), Result->GetSizeY(), levelCount, Result->GetFormat(), EInitializationType::NotInitialized);

						FMipmapGenerationSettings mipSettings{};

						ImOp.ImageMipmap( m_pSettings->ImageCompressionQuality, Mipmapped.get(), Result.get(), levelCount, mipSettings );

						Release(Result);
						Result = Mipmapped;
                    }				

					StoreImage(item, Result);
				}
                else
                {
					StoreImage(item, Base);
				}
				
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
                AddOp( FScheduledOp( item.At, item, 1), FScheduledOp( args.source, item) );
                break;

            case 1:
            {
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
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
                AddOp( FScheduledOp( item.At, item, 1), FScheduledOp::FromOpAndOptions( args.layout, item, 0) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_BLANKLAYOUT_1)
            		
                Ptr<const Layout> pLayout = LoadLayout(FScheduledOp::FromOpAndOptions(args.layout, item, 0));

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
					//int MinBlockSize = FMath::Max(FormatData.PixelsPerBlockX, FormatData.PixelsPerBlockY);
					//ReducedBlockSizeInPixels.X = FMath::Max<int32>(ReducedBlockSizeInPixels.X, FormatData.PixelsPerBlockX);
					//ReducedBlockSizeInPixels.Y = FMath::Max<int32>(ReducedBlockSizeInPixels.Y, FormatData.PixelsPerBlockY);
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

				// It needs to be initialized in case it has gaps.
                ImagePtr New = CreateImage(ImageSizeInPixels.X, ImageSizeInPixels.Y, MipsToGenerate, EImageFormat(args.format), EInitializationType::Black );
                StoreImage( item, New );
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
                AddOp( FScheduledOp( item.At, item, 1 ), FScheduledOp::FromOpAndOptions( args.layout, item, 0 ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_COMPOSE_1)
            		
                Ptr<const Layout> ComposeLayout = LoadLayout( FCacheAddress( args.layout, FScheduledOp::FromOpAndOptions(args.layout, item, 0)) );

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

                float factor = LoadScalar( FCacheAddress(args.factor,item) );

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

                if ( bifactor < UE_SMALL_NUMBER )
                {
                    Ptr<const Image> Source = LoadImage( FCacheAddress(args.targets[min],item) );
					StoreImage(item, Source);
				}
                else if ( bifactor > 1.0f-UE_SMALL_NUMBER )
                {
                    Ptr<const Image> Source = LoadImage( FCacheAddress(args.targets[max],item) );
					StoreImage(item, Source);
				}
                else
                {
					Ptr<const Image> pMin = LoadImage( FCacheAddress(args.targets[min],item) );
                    Ptr<const Image> pMax = LoadImage( FCacheAddress(args.targets[max],item) );

                    if (pMin && pMax)
                    {
						int32 LevelCount = FMath::Max(pMin->GetLODCount(), pMax->GetLODCount());
						
						Ptr<Image> pNew = CloneOrTakeOver(pMin);

						// Be defensive: ensure image sizes match.
						if (pNew->GetSize() != pMax->GetSize())
						{
							MUTABLE_CPUPROFILER_SCOPE(ImageResize_ForInterpolate);
							Ptr<Image> Resized = CreateImage(pNew->GetSizeX(), pNew->GetSizeY(), pMax->GetLODCount(), pMax->GetFormat(), EInitializationType::NotInitialized);
							ImOp.ImageResizeLinear(Resized.get(), 0, pMax.get());
							Release(pMax);
							pMax = Resized;
						}

						if (pNew->GetLODCount() != LevelCount)
						{
							MUTABLE_CPUPROFILER_SCOPE(Mipmap_ForInterpolate);

							ImagePtr pDest = CreateImage(pNew->GetSizeX(), pNew->GetSizeY(), LevelCount, pNew->GetFormat(), EInitializationType::NotInitialized);

							FMipmapGenerationSettings settings{};
							ImOp.ImageMipmap(m_pSettings->ImageCompressionQuality, pDest.get(), pNew.get(), LevelCount, settings);

							Release(pNew);
							pNew = pDest;
						}

						if (pMax->GetLODCount() != LevelCount)
						{
							MUTABLE_CPUPROFILER_SCOPE(Mipmap_ForInterpolate);

							ImagePtr pDest = CreateImage(pMax->GetSizeX(), pMax->GetSizeY(), LevelCount, pMax->GetFormat(), EInitializationType::NotInitialized);

							FMipmapGenerationSettings settings{};
							ImOp.ImageMipmap(m_pSettings->ImageCompressionQuality, pDest.get(), pMax.get(), LevelCount, settings);

							Release(pMax);
							pMax = pDest;
						}

                        ImageInterpolate( pNew.get(), pMax.get(), bifactor );

						Release(pMax);
						StoreImage(item, pNew);
					}
                    else if (pMin)
                    {
						StoreImage(item, pMin);
					}
                    else if (pMax)
                    {
						StoreImage(item, pMax);
					}

				}

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
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
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
            		
                Ptr<const Image> Base = LoadImage( FCacheAddress(args.base,item) );

				Ptr<Image> Result = CreateImage(Base->GetSizeX(), Base->GetSizeY(), Base->GetLODCount(), EImageFormat::IF_L_UBYTE, EInitializationType::NotInitialized);
                ImageLuminance( Result.get(),Base.get() );

				Release(Base);
				StoreImage( item, Result );
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
            		
                Ptr<const Image> Source = LoadImage( FCacheAddress(args.base,item) );
                Ptr<const Image> Mask = LoadImage( FCacheAddress(args.mask,item) );
                Ptr<const Image> Map = LoadImage( FCacheAddress(args.map,item) );

				bool bOnlyOneMip = (Mask->GetLODCount() < Source->GetLODCount());

				// Be defensive: ensure image sizes match.
				if (Mask->GetSize() != Source->GetSize())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageResize_ForColourmap);
					Ptr<Image> Resized = CreateImage(Source->GetSizeX(), Source->GetSizeY(), 1, Mask->GetFormat(), EInitializationType::NotInitialized);
					ImOp.ImageResizeLinear(Resized.get(), 0, Mask.get());
					Release(Mask);
					Mask = Resized;
				}

				Ptr<Image> Result = CreateImage(Source->GetSizeX(), Source->GetSizeY(), Source->GetLODCount(), Source->GetFormat(), EInitializationType::NotInitialized);
				ImageColourMap( Result.get(), Source.get(), Mask.get(), Map.get(), bOnlyOneMip);

				if (bOnlyOneMip)
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageColourMap_MipFix);
					FMipmapGenerationSettings DummyMipSettings{};
					ImageMipmapInPlace(m_pSettings->ImageCompressionQuality, Result.get(), DummyMipSettings);
				}

				Release(Source);
				Release(Mask);
				Release(Map);
				StoreImage( item, Result );
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
            		
				FVector4f colour0 = LoadColor(FScheduledOp::FromOpAndOptions(args.colour0, item, 0));
				FVector4f colour1 = LoadColor(FScheduledOp::FromOpAndOptions(args.colour1, item, 0));

				ImagePtr pResult = CreateImage(args.size[0], args.size[1], 1, EImageFormat::IF_RGB_UBYTE, EInitializationType::NotInitialized);
                ImageGradient( pResult.get(), colour0, colour1 );

				StoreImage( item, pResult );
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
            		
                Ptr<const Image> pA = LoadImage( FCacheAddress(args.base,item) );

                float c = LoadScalar(FScheduledOp::FromOpAndOptions(args.threshold, item, 0));

                Ptr<Image> Result = CreateImage(pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), EImageFormat::IF_L_UBYTE, EInitializationType::NotInitialized);
				ImageBinarise( Result.get(), pA.get(), c );

				Release(pA);
				StoreImage( item, Result );
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
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item));
				break;

			case 1:
			{
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
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
				AddOp( FScheduledOp( item.At, item, 1), FScheduledOp::FromOpAndOptions( args.colour, item, 0 ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_PLAINCOLOUR_1)
            		
				FVector4f c = LoadColor(FScheduledOp::FromOpAndOptions(args.colour, item, 0));

				uint16 SizeX = args.size[0];
				uint16 SizeY = args.size[1];
				int32 LODs = args.LODs;
				
				// This means all the mip chain
				if (LODs == 0)
				{
					LODs = FMath::CeilLogTwo(FMath::Max(SizeX,SizeY));
				}

				for (int l=0; l<item.ExecutionOptions; ++l)
				{
					SizeX = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeX, uint16(2)));
					SizeY = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeY, uint16(2)));
					--LODs;
				}

                ImagePtr pA = CreateImage( SizeX, SizeY, FMath::Max(LODs,1), EImageFormat(args.format), EInitializationType::NotInitialized );

				ImOp.FillColor(pA.get(), c);

				StoreImage( item, pA );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case OP_TYPE::IM_REFERENCE:
		{
			OP::ResourceReferenceArgs Args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ResourceReferenceArgs>(item.At);
			switch (item.Stage)
			{
			case 0:
			{
				Ptr<Image> Result = Image::CreateAsReference(Args.ID);
				StoreImage(item, Result);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case OP_TYPE::IM_CROP:
        {
			OP::ImageCropArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImageCropArgs>(item.At);
            switch (item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( item.At, item, 1), FScheduledOp( args.source, item ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_CROP_1)
            		
                Ptr<const Image> pA = LoadImage( FCacheAddress(args.source,item) );

                box< UE::Math::TIntVector2<int32> > rect;
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
					pResult = CreateImage( rect.size[0], rect.size[1], 1, pA->GetFormat(), EInitializationType::NotInitialized);
					ImOp.ImageCrop(pResult.get(), m_pSettings->ImageCompressionQuality, pA.get(), rect);
				}

				Release(pA);
				StoreImage( item, pResult );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case OP_TYPE::IM_PATCH:
        {
			// TODO: This is optimized for memory-usage but base and patch could be requested at the same time
			OP::ImagePatchArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ImagePatchArgs>(item.At);
            switch (item.Stage)
            {
			case 0:
				AddOp(FScheduledOp(item.At, item, 1), FScheduledOp(args.base, item));
				break;

			case 1:
				AddOp(FScheduledOp(item.At, item, 2), FScheduledOp(args.patch, item));
				break;

			case 2:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_PATCH_1)

                Ptr<const Image> pA = LoadImage( FCacheAddress(args.base,item) );
                Ptr<const Image> pB = LoadImage( FCacheAddress(args.patch,item) );

				// Failsafe
				if (!pA || !pB)
				{
					Release(pB);
					StoreImage(item, pA);
					break;
				}

				// Apply the mipmap reduction to the crop rectangle.
				int32 MipsToSkip = item.ExecutionOptions;
				box<UE::Math::TIntVector2<uint16>> rect;
				rect.min[0] = args.minX / (1 << MipsToSkip);
				rect.min[1] = args.minY / (1 << MipsToSkip);
				rect.size[0] = pB->GetSizeX();
				rect.size[1] = pB->GetSizeY();

                ImagePtr pResult = CloneOrTakeOver(pA);

				bool bApplyPatch = !rect.IsEmpty();
				if (bApplyPatch)
				{
					// Change the block image format if it doesn't match the composed image
					// This is usually enforced at object compilation time.
					if (pResult->GetFormat() != pB->GetFormat())
					{
						MUTABLE_CPUPROFILER_SCOPE(ImagPatchReformat);

						EImageFormat format = GetMostGenericFormat(pResult->GetFormat(), pB->GetFormat());

						const FImageFormatData& finfo = GetImageFormatData(format);
						if (finfo.PixelsPerBlockX == 0)
						{
							format = GetUncompressedFormat(format);
						}

						if (pResult->GetFormat() != format)
						{
							Ptr<Image> Formatted = CreateImage(pResult->GetSizeX(), pResult->GetSizeY(), pResult->GetLODCount(), format, EInitializationType::NotInitialized);
							bool bSuccess = false;
							ImOp.ImagePixelFormat(bSuccess, m_pSettings->ImageCompressionQuality, Formatted.get(), pResult.get());
							check(bSuccess);
							Release(pResult);
							pResult = Formatted;
						}
						if (pB->GetFormat() != format)
						{
							Ptr<Image> Formatted = CreateImage(pB->GetSizeX(), pB->GetSizeY(), pB->GetLODCount(), format, EInitializationType::NotInitialized);
							bool bSuccess = false;
							ImOp.ImagePixelFormat(bSuccess, m_pSettings->ImageCompressionQuality, Formatted.get(), pB.get());
							check(bSuccess);
							Release(pB);
						}
					}

					// Don't patch if below the image compression block size.
					const FImageFormatData& finfo = GetImageFormatData(pResult->GetFormat());
					bApplyPatch =
						(rect.min[0] % finfo.PixelsPerBlockX == 0) &&
						(rect.min[1] % finfo.PixelsPerBlockY == 0) &&
						(rect.size[0] % finfo.PixelsPerBlockX == 0) &&
						(rect.size[1] % finfo.PixelsPerBlockY == 0) &&
						(rect.min[0] + rect.size[0]) <= pResult->GetSizeX() &&
						(rect.min[1] + rect.size[1]) <= pResult->GetSizeY()
						;
				}

				if (bApplyPatch)
				{
					ImOp.ImageCompose(pResult.get(), pB.get(), rect);
					pResult->m_flags = 0;
				}
				else
				{
					// This happens very often when skipping mips, and floods the log.
					//UE_LOG( LogMutableCore, Verbose, TEXT("Skipped patch operation for image not fitting the block compression size. Small image? Patch rect is (%d, %d), (%d, %d), base is (%d, %d)"),
					//	rect.min[0], rect.min[1], rect.size[0], rect.size[1], pResult->GetSizeX(), pResult->GetSizeY());
				}

				Release(pB);
				StoreImage( item, pResult );
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
				if (args.image)
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp::FromOpAndOptions(args.mesh, item, 0),
						FScheduledOp::FromOpAndOptions(args.projector, item, 0));
				}
				else
				{
					AddOp(FScheduledOp(item.At, item, 1),
						FScheduledOp::FromOpAndOptions(args.mesh, item, 0));
				}
                break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(IM_RASTERMESH_1)

				Ptr<const Mesh> pMesh = LoadMesh(FScheduledOp::FromOpAndOptions(args.mesh, item, 0));

				// If no image, we are generating a flat mesh UV raster. This is the final stage in this case.
				if (!args.image)
				{
					uint16 SizeX = args.sizeX;
					uint16 SizeY = args.sizeY;
					UE::Math::TIntVector2<uint16> CropMin(args.CropMinX, args.CropMinY);
					UE::Math::TIntVector2<uint16> UncroppedSize(args.UncroppedSizeX, args.UncroppedSizeY);

					// Drop mips while possible
					int32 MipsToDrop = item.ExecutionOptions;
					bool bUseCrop = UncroppedSize[0] > 0;
					while (MipsToDrop && !(SizeX % 2) && !(SizeY % 2))
					{
						SizeX = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeX, uint16(2)));
						SizeY = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeY, uint16(2)));
						if (bUseCrop)
						{
							CropMin[0] = FMath::DivideAndRoundUp(CropMin[0], uint16(2));
							CropMin[1] = FMath::DivideAndRoundUp(CropMin[1], uint16(2));
							UncroppedSize[0] = FMath::Max(uint16(1), FMath::DivideAndRoundUp(UncroppedSize[0], uint16(2)));
							UncroppedSize[1] = FMath::Max(uint16(1), FMath::DivideAndRoundUp(UncroppedSize[1], uint16(2)));
						}
						--MipsToDrop;
					}

                    // Flat mesh UV raster
					Ptr<Image> ResultImage = CreateImage(SizeX, SizeY, 1, EImageFormat::IF_L_UBYTE, EInitializationType::Black);
					if (pMesh)
					{
						ImageRasterMesh(pMesh.get(), ResultImage.get(), args.LayoutIndex, args.blockId, CropMin, UncroppedSize);
						Release(pMesh);
					}

					// Stop execution.
					StoreImage(item, ResultImage);
					break;
				}

				const int32 MipsToSkip = item.ExecutionOptions;
				int32 ProjectionMip = MipsToSkip;

				FScheduledOpData Data;
				Data.RasterMesh.Mip = ProjectionMip;
				Data.RasterMesh.MipValue = static_cast<float>(ProjectionMip);
				FProjector Projector = LoadProjector(FScheduledOp::FromOpAndOptions(args.projector, item, 0));

				EMinFilterMethod MinFilterMethod = Invoke([&]() -> EMinFilterMethod
				{
					if (ForcedProjectionMode == 0)
					{
						return EMinFilterMethod::None;
					}
					else if (ForcedProjectionMode == 1)
					{
						return EMinFilterMethod::TotalAreaHeuristic;
					}
						
					return static_cast<EMinFilterMethod>(args.MinFilterMethod);
				});

				if (MinFilterMethod == EMinFilterMethod::TotalAreaHeuristic)
				{
					FVector2f TargetImageSizeF = FVector2f(
						FMath::Max(args.sizeX >> MipsToSkip, 1),
						FMath::Max(args.sizeY >> MipsToSkip, 1));
					FVector2f SourceImageSizeF = FVector2f(args.SourceSizeX, args.SourceSizeY);
						
					if (pMesh)
					{ 
						const float ComputedMip = ComputeProjectedFootprintBestMip(pMesh.get(), Projector, TargetImageSizeF, SourceImageSizeF);

						Data.RasterMesh.MipValue = FMath::Max(0.0f, ComputedMip + GlobalProjectionLodBias);
						Data.RasterMesh.Mip = static_cast<uint8>(FMath::FloorToInt32(Data.RasterMesh.MipValue));
					}
				}
		
				const int32 DataHeapAddress = m_heapData.Add(Data);

				// pMesh is need again in the next stage, store it in the heap.
				m_heapData[DataHeapAddress].Resource = const_cast<Mesh*>(pMesh.get());

				AddOp(FScheduledOp(item.At, item, 2, DataHeapAddress),
					FScheduledOp::FromOpAndOptions(args.projector, item, 0),
					FScheduledOp::FromOpAndOptions(args.image, item, Data.RasterMesh.Mip),
					FScheduledOp(args.mask, item),
					FScheduledOp::FromOpAndOptions(args.angleFadeProperties, item, 0));

				break;
			}

            case 2:
            {
				MUTABLE_CPUPROFILER_SCOPE(IM_RASTERMESH_2)

				if (!args.image)
				{
					// This case is treated at the previous stage.
					check(false);
					StoreImage(item, nullptr);
					break;
				}

				FScheduledOpData& Data = m_heapData[item.CustomState];

				// Unsafe downcast, should be fine as it is known to be a Mesh.
				Ptr<const Mesh> pMesh = static_cast<Mesh*>(Data.Resource.get());
				Data.Resource = nullptr;

				if (!pMesh)
				{
					check(false);
					StoreImage(item, nullptr);
					break;
				}

				uint16 SizeX = args.sizeX;
				uint16 SizeY = args.sizeY;
				UE::Math::TIntVector2<uint16> CropMin(args.CropMinX, args.CropMinY);
				UE::Math::TIntVector2<uint16> UncroppedSize(args.UncroppedSizeX, args.UncroppedSizeY);

				// Drop mips while possible
				int32 MipsToDrop = item.ExecutionOptions;
				bool bUseCrop = UncroppedSize[0] > 0;
				while (MipsToDrop && !(SizeX % 2) && !(SizeY % 2))
				{
					SizeX = FMath::Max(uint16(1),FMath::DivideAndRoundUp(SizeX, uint16(2)));
					SizeY = FMath::Max(uint16(1),FMath::DivideAndRoundUp(SizeY, uint16(2)));
					if (bUseCrop)
					{
						CropMin[0] = FMath::DivideAndRoundUp(CropMin[0], uint16(2));
						CropMin[1] = FMath::DivideAndRoundUp(CropMin[1], uint16(2));
						UncroppedSize[0] = FMath::Max(uint16(1), FMath::DivideAndRoundUp(UncroppedSize[0], uint16(2)));
						UncroppedSize[1] = FMath::Max(uint16(1), FMath::DivideAndRoundUp(UncroppedSize[1], uint16(2)));
					}
					--MipsToDrop;
				}

				// Raster with projection
				Ptr<const Image> Source = LoadImage(FCacheAddress(args.image, item.ExecutionIndex, Data.RasterMesh.Mip));

				Ptr<const Image> Mask = nullptr;
				if (args.mask)
				{
					Mask = LoadImage(FCacheAddress(args.mask, item));

					// TODO: This shouldn't happen, but be defensive.
					FImageSize ResultSize(SizeX, SizeY);
					if (Mask && Mask->GetSize()!= ResultSize)
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_MaskFixForProjection);

						Ptr<Image> Resized = CreateImage(SizeX, SizeY, Mask->GetLODCount(), Mask->GetFormat(), EInitializationType::NotInitialized);
						ImOp.ImageResizeLinear(Resized.get(), 0, Mask.get());
						Release(Mask);
						Mask = Resized;
					}
				}

				float fadeStart = 180.0f;
				float fadeEnd = 180.0f;
				if ( args.angleFadeProperties )
				{
					FVector4f fadeProperties = LoadColor(FScheduledOp::FromOpAndOptions(args.angleFadeProperties, item, 0));
					fadeStart = fadeProperties[0];
					fadeEnd = fadeProperties[1];
				}
				const float FadeStartRad = FMath::DegreesToRadians(fadeStart);
				const float FadeEndRad = FMath::DegreesToRadians(fadeEnd);

				EImageFormat Format = Source ? GetUncompressedFormat(Source->GetFormat()) : EImageFormat::IF_L_UBYTE;

				if (Source && Source->GetFormat()!=Format)
				{
					MUTABLE_CPUPROFILER_SCOPE(RunCode_RasterMesh_ReformatSource);
					Ptr<Image> Formatted = CreateImage(Source->GetSizeX(), Source->GetSizeY(), Source->GetLODCount(), Format, EInitializationType::NotInitialized);
					bool bSuccess = false;
					ImOp.ImagePixelFormat(bSuccess, m_pSettings->ImageCompressionQuality, Formatted.get(), Source.get());
					check(bSuccess); 
					Release(Source);
					Source = Formatted;
				}

				// Allocate memory for the temporary buffers
				SCRATCH_IMAGE_PROJECT scratch;
				scratch.vertices.SetNum( pMesh->GetVertexCount() );
				scratch.culledVertex.SetNum( pMesh->GetVertexCount() );

				ESamplingMethod SamplingMethod = Invoke([&]() -> ESamplingMethod
				{
					if (ForcedProjectionMode == 0)
					{
						return ESamplingMethod::Point;
					}
					else if (ForcedProjectionMode == 1)
					{
						return ESamplingMethod::BiLinear;
					}
					
					return static_cast<ESamplingMethod>(args.SamplingMethod);
				});

				if (SamplingMethod == ESamplingMethod::BiLinear)
				{
					if (Source->GetLODCount() < 2 && Source->GetSizeX() > 1 && Source->GetSizeY() > 1)
					{
						MUTABLE_CPUPROFILER_SCOPE(RunCode_RasterMesh_BilinearMipGen);

						Ptr<Image> NewImage = CreateImage(Source->GetSizeX(), Source->GetSizeY(), 2, Source->GetFormat(), EInitializationType::NotInitialized);

						check(NewImage->GetDataSize() >= Source->GetDataSize());
						FMemory::Memcpy(NewImage->GetData(), Source->GetData(), Source->GetDataSize());

						ImageMipmapInPlace(0, NewImage.get(), FMipmapGenerationSettings{});

						Release(Source);
						Source = NewImage;
					}
				}

				// Allocate new image after bilinear mip generation to reduce operation memory peak.
				Ptr<Image> New = CreateImage(SizeX, SizeY, 1, Format, EInitializationType::Black);

				if (args.projector && Source && Source->GetSizeX() > 0 && Source->GetSizeY() > 0)
				{
					FProjector Projector = LoadProjector(FScheduledOp::FromOpAndOptions(args.projector, item, 0));

					switch (Projector.type)
					{
					case PROJECTOR_TYPE::PLANAR:
						ImageRasterProjectedPlanar(pMesh.get(), New.get(),
							Source.get(), Mask.get(),
							args.bIsRGBFadingEnabled, args.bIsAlphaFadingEnabled,
							SamplingMethod,
							FadeStartRad, FadeEndRad, FMath::Frac(Data.RasterMesh.MipValue),
							args.LayoutIndex, args.blockId,
							CropMin, UncroppedSize,
							&scratch, bUseProjectionVectorImpl);
						break;

					case PROJECTOR_TYPE::WRAPPING:
						ImageRasterProjectedWrapping(pMesh.get(), New.get(),
							Source.get(), Mask.get(),
							args.bIsRGBFadingEnabled, args.bIsAlphaFadingEnabled,
							SamplingMethod,
							FadeStartRad, FadeEndRad, FMath::Frac(Data.RasterMesh.MipValue),
							args.LayoutIndex, args.blockId,
							CropMin, UncroppedSize,
							&scratch);
						break;

					case PROJECTOR_TYPE::CYLINDRICAL:
						ImageRasterProjectedCylindrical(pMesh.get(), New.get(),
							Source.get(), Mask.get(),
							args.bIsRGBFadingEnabled, args.bIsAlphaFadingEnabled,
							FadeStartRad, FadeEndRad,
							args.LayoutIndex,
							Projector.projectionAngle,
							CropMin, UncroppedSize,
							&scratch);
						break;

					default:
						check(false);
						break;
					}
				}

				Release(pMesh);
				Release(Source);
				Release(Mask);
				StoreImage(item, New);

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
                AddOp( FScheduledOp( item.At, item, 1), FScheduledOp( args.mask, item) );
                break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_MAKEGROWMAP_1)

                Ptr<const Image> Mask = LoadImage( FCacheAddress(args.mask,item) );

                Ptr<Image> Result = CreateImage( Mask->GetSizeX(), Mask->GetSizeY(), Mask->GetLODCount(), EImageFormat::IF_L_UBYTE, EInitializationType::NotInitialized);

                ImageMakeGrowMap(Result.get(), Mask.get(), args.border );
				Result->m_flags |= Image::IF_CANNOT_BE_SCALED;

				Release(Mask);
                StoreImage( item, Result);
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

                Ptr<const Image> Source = LoadImage( FCacheAddress(args.source,item) );
                Ptr<const Image> pMap = LoadImage( FCacheAddress(args.displacementMap,item) );

				if (!Source)
				{
					Release(pMap);
					StoreImage(item, nullptr);
					break;
				}

				// TODO: This shouldn't happen: displacement maps cannot be scaled because their information
				// is resolution sensitive (pixel offsets). If the size doesn't match, scale the source, apply 
				// displacement and then unscale it.
				FImageSize OriginalSourceScale = Source->GetSize();
				if (OriginalSourceScale[0]>0 && OriginalSourceScale[1]>0 && OriginalSourceScale != pMap->GetSize())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyHackForDisplacementStep1);

					Ptr<Image> Resized = CreateImage(pMap->GetSizeX(), pMap->GetSizeY(), Source->GetLODCount(), Source->GetFormat(), EInitializationType::NotInitialized);
					ImOp.ImageResizeLinear(Resized.get(), 0, Source.get());
					Release(Source);
					Source = Resized;
				}

				// This works based on the assumption that displacement maps never read from a position they actually write to.
				// Since they are used for UV border expansion, this should always be the case.
				Ptr<Image> Result = CloneOrTakeOver(Source);

				if (OriginalSourceScale[0] > 0 && OriginalSourceScale[1] > 0)
				{
					ImageDisplace(Result.get(), Result.get(), pMap.get());

					if (OriginalSourceScale != Result->GetSize())
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyHackForDisplacementStep2);
						Ptr<Image> Resized = CreateImage(OriginalSourceScale[0], OriginalSourceScale[1], Result->GetLODCount(), Result->GetFormat(), EInitializationType::NotInitialized);
						ImOp.ImageResizeLinear(Resized.get(), 0, Result.get());
						Release(Result);
						Result = Resized;
					}
				}

				Release(pMap);
                StoreImage( item, Result);
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
						FScheduledOp(Args.base, item),
						FScheduledOp(Args.offsetX, item),
						FScheduledOp(Args.offsetY, item),
						FScheduledOp(Args.scaleX, item),
						FScheduledOp(Args.scaleY, item),
						FScheduledOp(Args.rotation, item) };

                AddOp(FScheduledOp(item.At, item, 1), Deps);

				break;
			}
            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_TRANSFORM_1)
            		
                Ptr<const Image> pBaseImage = LoadImage(FCacheAddress(Args.base, item));
                
                const FVector2f Offset = FVector2f(
                        Args.offsetX ? LoadScalar(FCacheAddress(Args.offsetX, item)) : 0.0f,
                        Args.offsetY ? LoadScalar(FCacheAddress(Args.offsetY, item)) : 0.0f);

                const FVector2f Scale = FVector2f(
                        Args.scaleX ? LoadScalar(FCacheAddress(Args.scaleX, item)) : 1.0f,
                        Args.scaleY ? LoadScalar(FCacheAddress(Args.scaleY, item)) : 1.0f);

				// Map Range 0-1 to a full rotation
                const float Rotation = LoadScalar(FCacheAddress(Args.rotation, item)) * UE_TWO_PI;
			
				EImageFormat BaseFormat = pBaseImage->GetFormat();
				int32 BaseLODs = pBaseImage->GetLODCount();
				FImageSize BaseSize = pBaseImage->GetSize();
				FImageSize SampleImageSize =  FImageSize(
					static_cast<uint16>(FMath::Clamp(FMath::FloorToInt(float(BaseSize.X) * FMath::Abs(Scale.X)), 2, BaseSize.X)),
					static_cast<uint16>(FMath::Clamp(FMath::FloorToInt(float(BaseSize.Y) * FMath::Abs(Scale.Y)), 2, BaseSize.Y)));
	
				Ptr<Image> pSampleImage = CreateImage(SampleImageSize[0], SampleImageSize[1], BaseLODs, BaseFormat, EInitializationType::NotInitialized);
				ImOp.ImageResizeLinear( pSampleImage.get(), 0, pBaseImage.get());
				Release(pBaseImage);

				Ptr<Image> Result = CreateImage(BaseSize.X, BaseSize.Y, 1, BaseFormat, EInitializationType::NotInitialized);
				ImageTransform(Result.get(), pSampleImage.get(), Offset, Scale, Rotation, static_cast<EAddressMode>(Args.AddressMode));

				Release(pSampleImage);
				StoreImage(item, Result);

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
            const ExecutionIndex& currentIndex = GetMemory().GetRangeIndex( item.ExecutionIndex );
            int position = currentIndex.GetFromModelRangeIndex(rangeIndexInModel);
            index->GetPrivate()->m_values[rangeIndexInParam] = position;
        }

        return index;
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Bool(const FScheduledOp& item, const Parameters* pParams, const Model* pModel )
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
            StoreBool( item, result );
            break;
        }

        case OP_TYPE::BO_PARAMETER:
        {
			OP::ParameterArgs args = program.GetOpArgs<OP::ParameterArgs>(item.At);
            bool result = false;
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            result = pParams->GetBoolValue( args.variable, index );
            StoreBool( item, result );
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
                float a = LoadScalar( FCacheAddress(args.a,item) );
                float b = LoadScalar( FCacheAddress(args.b,item) );
                bool result = a<b;
                StoreBool( item, result );
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
                         bool a = LoadBool( FCacheAddress(args.a,item) );
                         if (!a)
                         {
                            StoreBool( item, false );
                            skip=true;
                         }
                    }

                    if ( !skip && args.b && GetMemory().IsValid( FCacheAddress(args.b,item) ) )
                    {
                         bool b = LoadBool( FCacheAddress(args.b,item) );
                         if (!b)
                         {
                            StoreBool( item, false );
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
                bool a = args.a ? LoadBool( FCacheAddress(args.a,item) ) : true;
                if (!a)
                {
                    StoreBool( item, false );
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
                bool b = args.b ? LoadBool( FCacheAddress(args.b,item) ) : true;
                StoreBool( item, b );
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
                         bool a = LoadBool( FCacheAddress(args.a,item) );
                         if (a)
                         {
                            StoreBool( item, true );
                            skip=true;
                         }
                    }

                    if ( !skip && args.b && GetMemory().IsValid( FCacheAddress(args.b,item) ) )
                    {
                         bool b = LoadBool( FCacheAddress(args.b,item) );
                         if (b)
                         {
                            StoreBool( item, true );
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
                bool a = args.a ? LoadBool( FCacheAddress(args.a,item) ) : false;
                if (a)
                {
                    StoreBool( item, true );
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
                bool b = args.b ? LoadBool( FCacheAddress(args.b,item) ) : false;
                StoreBool( item, b );
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
                bool result = !LoadBool( FCacheAddress(args.source,item) );
                StoreBool( item, result );
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
                int a = LoadInt( FCacheAddress(args.value,item) );
                bool result = a == args.constant;
                StoreBool( item, result );
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
    void CodeRunner::RunCode_Int(const FScheduledOp& item, const Parameters* pParams, const Model* pModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Int);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::NU_CONSTANT:
        {
			OP::IntConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::IntConstantArgs>(item.At);
            int result = args.value;
            StoreInt( item, result );
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

            StoreInt( item, result );
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Scalar(const FScheduledOp& item, const Parameters* pParams, const Model* pModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Scalar);

		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::SC_CONSTANT:
        {
			OP::ScalarConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ScalarConstantArgs>(item.At);
            float result = args.value;
            StoreScalar( item, result );
            break;
        }

        case OP_TYPE::SC_PARAMETER:
        {
			OP::ParameterArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ParameterArgs>(item.At);
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            float result = pParams->GetFloatValue( args.variable, index );
            StoreScalar( item, result );
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
                float time = LoadScalar( FCacheAddress(args.time,item) );

                const Curve& curve = pModel->GetPrivate()->m_program.m_constantCurves[args.curve];
                float result = EvalCurve(curve, time);

                StoreScalar( item, result );
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
                float a = LoadScalar( FCacheAddress(args.a,item) );
                float b = LoadScalar( FCacheAddress(args.b,item) );

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

                StoreScalar( item, result );
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
    void CodeRunner::RunCode_String(const FScheduledOp& item, const Parameters* pParams, const Model* pModel )
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
            StoreString( item, new String(result.c_str()) );

            break;
        }

        case OP_TYPE::ST_PARAMETER:
        {
			OP::ParameterArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ParameterArgs>( item.At );
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            string result = pParams->GetStringValue( args.variable, index );
            StoreString( item, new String( result.c_str() ) );
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Colour(const FScheduledOp& item, const Parameters* pParams, const Model* pModel )
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
            StoreColor( item, result );
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
            StoreColor( item, FVector4f(r,g,b,1.0f) );
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
                float x = args.x ? LoadScalar( FCacheAddress(args.x,item) ) : 0.5f;
                float y = args.y ? LoadScalar( FCacheAddress(args.y,item) ) : 0.5f;

                Ptr<const Image> pImage = LoadImage(FScheduledOp::FromOpAndOptions(args.image, item, 0));

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

				Release(pImage);
                StoreColor( item, result );
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
                        FVector4f p = LoadColor( FCacheAddress(args.sources[t],item) );
                        result[t] = p[ args.sourceChannels[t] ];
                    }
                }

                StoreColor( item, result );
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
                Ptr<const Image> pImage = LoadImage( FCacheAddress(args.image,item) );

				FVector4f result = FVector4f( (float)pImage->GetSizeX(), (float)pImage->GetSizeY(), 0.0f, 0.0f );

				Release(pImage);
                StoreColor( item, result );
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
                Ptr<const Layout> pLayout = LoadLayout( FCacheAddress(args.layout,item) );

				FVector4f result = FVector4f(0,0,0,0);
                if ( pLayout )
                {
                    int relBlockIndex = pLayout->FindBlock( args.block );

                    if( relBlockIndex >=0 )
                    {
                        box< UE::Math::TIntVector2<uint16> > rectInblocks;
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

                StoreColor( item, result );
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
                           FScheduledOp( args.v[0], item),
                           FScheduledOp( args.v[1], item),
                           FScheduledOp( args.v[2], item),
                           FScheduledOp( args.v[3], item));
                break;

            case 1:
            {
				FVector4f Result = FVector4f(1, 1, 1, 1);

				for (int32 t = 0; t < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++t)
				{
					if (args.v[t])
					{
						Result[t] = LoadScalar(FCacheAddress(args.v[t], item));
					}
				}

                StoreColor( item, Result );
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
				FVector4f a = args.a ? LoadColor( FCacheAddress( args.a, item ) )
                                 : FVector4f( 0, 0, 0, 0 );
				FVector4f b = args.b ? LoadColor( FCacheAddress( args.b, item ) )
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

                StoreColor( item, result );
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
    void CodeRunner::RunCode_Projector(const FScheduledOp& item, const Parameters* pParams, const Model* pModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Projector);

        const FProgram& program = pModel->GetPrivate()->m_program;
		OP_TYPE type = program.GetOpType(item.At);
        switch (type)
        {

        case OP_TYPE::PR_CONSTANT:
        {
			OP::ResourceConstantArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ResourceConstantArgs>(item.At);
            FProjector Result = program.m_constantProjectors[args.value];
            StoreProjector( item, Result );
            break;
        }

        case OP_TYPE::PR_PARAMETER:
        {
			OP::ParameterArgs args = pModel->GetPrivate()->m_program.GetOpArgs<OP::ParameterArgs>(item.At);
			Ptr<RangeIndex> index = BuildCurrentOpRangeIndex( item, pParams, pModel, args.variable );
            FProjector Result = pParams->GetPrivate()->GetProjectorValue(args.variable,index);

            // The type cannot be changed, take it from the default value
            const FProjector& def = program.m_parameters[args.variable].m_defaultValue.Get<ParamProjectorType>();
            Result.type = def.type;

            StoreProjector( item, Result );
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Layout(const FScheduledOp& item, const Model* pModel )
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
            StoreLayout( item, pResult );
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
                Ptr<const Layout> pA = LoadLayout( FCacheAddress(args.Base,item) );
                Ptr<const Layout> pB = LoadLayout( FCacheAddress(args.Added,item) );

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

                StoreLayout( item, pResult );
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
                Ptr<const Layout> Source = LoadLayout( FCacheAddress(args.Source,item) );

				LayoutPtr pResult;

				if (Source)
				{
					pResult = Source->Clone();

					SCRATCH_LAYOUT_PACK scratch;
					int32 BlockCount = Source->GetBlockCount();
					scratch.blocks.SetNum(BlockCount);
					scratch.sorted.SetNum(BlockCount);
					scratch.positions.SetNum(BlockCount);
					scratch.priorities.SetNum(BlockCount);
					scratch.reductions.SetNum(BlockCount);
					scratch.useSymmetry.SetNum(BlockCount);

					LayoutPack3(pResult.get(), Source.get(), &scratch);
				}

                StoreLayout( item, pResult );
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
				Ptr<const Mesh> Mesh = LoadMesh(FCacheAddress(args.Mesh, item));

				Ptr<const Layout> Result = LayoutFromMesh_RemoveBlocks(Mesh.get(), args.LayoutIndex);

				Release(Mesh);
				StoreLayout(item, Result);
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
				Ptr<const Layout> Source = LoadLayout(FCacheAddress(args.Source, item));
				Ptr<const Layout> ReferenceLayout = LoadLayout(FCacheAddress(args.ReferenceLayout, item));

				Ptr<const Layout> pResult;

				if (Source && ReferenceLayout)
				{
					pResult = LayoutRemoveBlocks(Source.get(), ReferenceLayout.get());
				}
				else if (Source)
				{
					pResult = Source;
				}

				StoreLayout(item, pResult);
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
    void CodeRunner::RunCode( const FScheduledOp& item, const Parameters* pParams, const TSharedPtr<const Model>& InModel, uint32 lodMask)
    {
		//UE_LOG( LogMutableCore, Log, TEXT("Running :%5d , %d "), item.At, item.Stage );
		check( item.Type == FScheduledOp::EType::Full );

		const Model* pModel = InModel.Get();
		OP_TYPE type = pModel->GetPrivate()->m_program.GetOpType(item.At);
		//UE_LOG(LogMutableCore, Log, TEXT("Running :%5d , %d, of type %d "), item.At, item.Stage, type);

		// Very spammy, for debugging purposes.
		//if (m_pSystem)
		//{
		//	m_pSystem->WorkingMemoryManager.LogWorkingMemory( this );
		//}

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
		case OP_TYPE::ED_CONDITIONAL:
            RunCode_Conditional(item, pModel);
            break;

        case OP_TYPE::ME_CONSTANT:
		case OP_TYPE::IM_CONSTANT:
		case OP_TYPE::ED_CONSTANT:
            RunCode_ConstantResource(item, pModel);
            break;

        case OP_TYPE::NU_SWITCH:
        case OP_TYPE::SC_SWITCH:
        case OP_TYPE::CO_SWITCH:
        case OP_TYPE::IM_SWITCH:
        case OP_TYPE::ME_SWITCH:
        case OP_TYPE::LA_SWITCH:
        case OP_TYPE::IN_SWITCH:
		case OP_TYPE::ED_SWITCH:
            RunCode_Switch(item, pModel);
            break;

        case OP_TYPE::IN_ADDMESH:
        case OP_TYPE::IN_ADDIMAGE:
            RunCode_InstanceAddResource(item, InModel, pParams);
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
	void CodeRunner::RunCodeImageDesc(const FScheduledOp& item, const Parameters* pParams, const Model* pModel,  uint32 lodMask )
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

			FImageDesc& Result = m_heapImageDesc[item.CustomState];
			Result.m_format = program.m_constantImages[ImageIndex].ImageFormat;
			Result.m_size[0] = program.m_constantImages[ImageIndex].ImageSizeX;
			Result.m_size[1] = program.m_constantImages[ImageIndex].ImageSizeY;
			Result.m_lods = program.m_constantImages[ImageIndex].LODCount;
			StoreValidDesc(item);
			break;
		}

		case OP_TYPE::IM_PARAMETER:
		{
			check(item.Stage == 0);
			OP::ParameterArgs args = program.GetOpArgs<OP::ParameterArgs>(item.At);
			FName Id = pParams->GetImageValue(args.variable);
			uint8 MipsToSkip = item.ExecutionOptions;
			m_heapImageDesc[item.CustomState] = GetExternalImageDesc(Id, MipsToSkip);
			StoreValidDesc(item);
			break;
		}

		case OP_TYPE::IM_REFERENCE:
		{
			check(item.Stage == 0);
			FImageDesc& Result = m_heapImageDesc[item.CustomState];
			Result.m_format = EImageFormat::IF_NONE;
			Result.m_size[0] = 0;
			Result.m_size[1] = 0;
			Result.m_lods = 0;
			StoreValidDesc(item);
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
				bool value = LoadBool(FCacheAddress(args.condition, item.ExecutionIndex, item.ExecutionOptions));
				OP::ADDRESS resultAt = value ? args.yes : args.no;
				AddOp(FScheduledOp(item.At, item, 2), FScheduledOp(resultAt, item));
				break;
			}

			case 2: StoreValidDesc(item); break;
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
					StoreValidDesc(item);
				}
				break;
			}

			case 1:
			{
				// Get the variable result
				int var = LoadInt(FCacheAddress(VarAddress, item));

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

			case 2: StoreValidDesc(item); break;
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
			case 1: StoreValidDesc(item); break;
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
			case 1: StoreValidDesc(item); break;
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
			case 1: StoreValidDesc(item); break;
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
			case 1: StoreValidDesc(item); break;
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
					GetImageFormatData(OldFormat).Channels > 3)
				{
					NewFormat = args.formatIfAlpha;
				}
				m_heapImageDesc[item.CustomState].m_format = NewFormat;				
				StoreValidDesc(item);
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
				StoreValidDesc(item);
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
				StoreValidDesc(item);
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
				StoreValidDesc(item);
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
				StoreValidDesc(item);
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
				Ptr<const Layout> pLayout = LoadLayout(FCacheAddress(args.layout, item));

				FIntPoint SizeInBlocks = pLayout->GetGridSize();
				FIntPoint BlockSizeInPixels(args.blockSize[0], args.blockSize[1]);
				FIntPoint ImageSizeInPixels = SizeInBlocks * BlockSizeInPixels;

				FImageDesc& ResultAndBaseDesc = m_heapImageDesc[item.CustomState];
				FImageSize destSize(uint16(ImageSizeInPixels.X), uint16(ImageSizeInPixels.Y));
				ResultAndBaseDesc.m_size = destSize;
				ResultAndBaseDesc.m_format = args.format;
				
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
				StoreValidDesc(item);
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
			case 1: StoreValidDesc(item); break;
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
			case 1: StoreValidDesc(item); break;
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
			case 1: StoreValidDesc(item); break;
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
				StoreValidDesc(item);
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
				StoreValidDesc(item);
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
			case 1: StoreValidDesc(item); break;
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
			StoreValidDesc(item);
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
				StoreValidDesc(item);
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
			case 1: StoreValidDesc(item); break;
			default: check(false);
			}
			break;
		}

		case OP_TYPE::IM_PLAINCOLOUR:
		{
			OP::ImagePlainColourArgs args = program.GetOpArgs<OP::ImagePlainColourArgs>(item.At);
			m_heapImageDesc[item.CustomState].m_size[0] = args.size[0];
			m_heapImageDesc[item.CustomState].m_size[1] = args.size[1];
			m_heapImageDesc[item.CustomState].m_lods = args.LODs;
			m_heapImageDesc[item.CustomState].m_format = args.format;
			StoreValidDesc(item);
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
				StoreValidDesc(item);
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
			case 1: StoreValidDesc(item); break;
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
			StoreValidDesc(item);
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
				StoreValidDesc(item);
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
			case 1: StoreValidDesc(item); break;
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
				StoreValidDesc(item);
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
