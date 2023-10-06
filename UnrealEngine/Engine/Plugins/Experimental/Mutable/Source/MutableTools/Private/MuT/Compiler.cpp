// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Compiler.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformCrt.h"
#include "Hash/CityHash.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Serialisation.h"
#include "MuR/System.h"
#include "MuT/AST.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CodeGenerator.h"
#include "MuT/CodeOptimiser.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/Node.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"
#include "Trace/Detail/Channel.h"

#include <algorithm>
#include <memory>
#include <utility>


namespace mu
{

    const char* CompilerOptions::GetTextureLayoutStrategyName( CompilerOptions::TextureLayoutStrategy s )
    {
        static const char* s_textureLayoutStrategyName[ 2 ] =
        {
            "Unscaled Pack",
            "No Packing",
        };

        return s_textureLayoutStrategyName[(int)s];
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    CompilerOptions::CompilerOptions()
    {
        m_pD = new Private();
    }


    //---------------------------------------------------------------------------------------------
    CompilerOptions::~CompilerOptions()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    CompilerOptions::Private* CompilerOptions::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetLogEnabled( bool bEnabled)
    {
        m_pD->bLog = bEnabled;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetOptimisationEnabled( bool bEnabled)
    {
        m_pD->OptimisationOptions.bEnabled = bEnabled;
        if (bEnabled)
        {
            m_pD->OptimisationOptions.bConstReduction = true;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetConstReductionEnabled( bool bConstReductionEnabled )
    {
        m_pD->OptimisationOptions.bConstReduction = bConstReductionEnabled;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetUseDiskCache( bool enabled )
    {
        m_pD->OptimisationOptions.bUseDiskCache = enabled;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetOptimisationMaxIteration( int maxIterations )
    {
        m_pD->OptimisationOptions.MaxOptimisationLoopCount = maxIterations;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetIgnoreStates( bool bIgnore )
    {
        m_pD->bIgnoreStates = bIgnore;
    }


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetImageCompressionQuality(int32 Quality)
	{
		m_pD->ImageCompressionQuality = Quality;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetImageTiling(int32 Tiling)
	{
		m_pD->ImageTiling = Tiling;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetDataPackingStrategy(int32 MinRomSize, int32 MinTextureResidentMipCount)
	{
		m_pD->MinRomSize = MinRomSize;
		m_pD->MinTextureResidentMipCount = MinTextureResidentMipCount;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetEnableProgressiveImages(bool bEnabled)
	{
		m_pD->OptimisationOptions.bEnableProgressiveImages = bEnabled;
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    Compiler::Compiler( CompilerOptionsPtr options )
    {
        m_pD = new Private();
        m_pD->m_options = options;
        if (!m_pD->m_options)
        {
            m_pD->m_options = new CompilerOptions;
        }
    }


    //---------------------------------------------------------------------------------------------
    Compiler::~Compiler()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
	TSharedPtr<Model> Compiler::Compile( const Ptr<Node>& pNode )
    {
        MUTABLE_CPUPROFILER_SCOPE(Compile);

        vector< STATE_COMPILATION_DATA > states;
        Ptr<ErrorLog> genErrorLog;
        {
            CodeGenerator gen( m_pD->m_options->GetPrivate() );

            gen.GenerateRoot( pNode );

            check( !gen.m_states.IsEmpty() );

            for ( const auto& s: gen.m_states )
            {
                STATE_COMPILATION_DATA data;
                data.nodeState = s.first;
                data.root = s.second;
                data.state.m_name = s.first.m_name;
                states.push_back( data );
            }

            genErrorLog = gen.m_pErrorLog;
        }

        vector<Ptr<ASTOp>> roots;
        for( const STATE_COMPILATION_DATA& s: states)
        {
            roots.push_back(s.root);
        }

        // Slow AST code verification for debugging.
        //ASTOp::FullAssert(roots);

        // Optimize the generated code
        {
            CodeOptimiser optimiser( m_pD->m_options, states );
            optimiser.OptimiseAST( );
        }


        // Link the program and generate state data.
		TSharedPtr<Model> pResult = MakeShared<Model>();
        FProgram& program = pResult->GetPrivate()->m_program;

		// Preallocate ample memory
		program.m_byteCode.Reserve(16 * 1024 * 1024);
		program.m_opAddress.Reserve(1024 * 1024);

		// Keep the link options outside the scope because it is also used to cache constant data that has already been 
		// added and could be reused across states.
		FLinkerOptions LinkerOptions;

		for( STATE_COMPILATION_DATA& s: states )
        {
			LinkerOptions.MinTextureResidentMipCount = m_pD->m_options->GetPrivate()->MinTextureResidentMipCount;

            ASTOp::FullLink(s.root,program, &LinkerOptions);
            if (s.root)
            {
                s.state.m_root = s.root->linkedAddress;
            }
            else
            {
                s.state.m_root = 0;
            }
        }

		program.m_byteCode.Shrink();
		program.m_opAddress.Shrink();

        // Set the runtime parameter indices.
        for( STATE_COMPILATION_DATA& s: states )
        {
            for ( int32 p=0; p<s.nodeState.m_runtimeParams.Num(); ++p )
            {
                int32 paramIndex = -1;
                for ( int32 i=0; paramIndex<0 && i<program.m_parameters.Num(); ++i )
                {
                    if ( program.m_parameters[i].m_name
                         ==
                         s.nodeState.m_runtimeParams[p] )
                    {
                        paramIndex = (int)i;
                    }
                }

                if (paramIndex>=0)
                {
                    s.state.m_runtimeParameters.Add( paramIndex );
                }
                else
                {
					FString Temp = FString::Printf(TEXT(
						"The state [%s] refers to a parameter [%s] "
						"that has not been found in the model. This error can be "
						"safely dismissed in case of partial compilation."), 
						StringCast<TCHAR>(s.nodeState.m_name.c_str()).Get(),
						StringCast<TCHAR>(s.nodeState.m_runtimeParams[p].c_str()).Get());
                    m_pD->m_pErrorLog->GetPrivate()->Add(Temp, ELMT_WARNING, pNode->GetBasePrivate()->m_errorContext );
                }
            }

            // Generate the mask of update cache ops
            for ( const auto& a: s.m_updateCache )
            {
                s.state.m_updateCache.Emplace(a->linkedAddress);
            }

            // Sort the update cache addresses for performance and determinism
            s.state.m_updateCache.Sort();

            // Generate the mask of dynamic resources
            for ( const auto& a: s.m_dynamicResources )
            {
                uint64 relevantMask = 0;
                for ( const auto& b: a.Value )
                {
                    // Find the index in the model parameter list
                    int paramIndex = -1;
                    for ( int32 i=0; paramIndex<0 && i<program.m_parameters.Num(); ++i )
                    {
                        if ( program.m_parameters[i].m_name == b )
                        {
                            paramIndex = (int)i;
                        }
                    }
                    check(paramIndex>=0);

                    // Find the position in the state data vector.
                    int32 IndexInRuntimeList = s.state.m_runtimeParameters.Find( paramIndex );

                    if (IndexInRuntimeList !=INDEX_NONE )
                    {
                        relevantMask |= uint64(1) << IndexInRuntimeList;
                    }
                }

                // TODO: this shouldn't happen but it seems to happen. Investigate.
                // Maybe something with the difference of precision between the relevant parameters
                // in operation subtrees.
                //check(relevantMask!=0);
                if (relevantMask!=0)
                {
                    s.state.m_dynamicResources.Emplace( a.Key->linkedAddress, relevantMask );
                }
            }            

            // Sort for performance and determinism
            s.state.m_dynamicResources.Sort();

            program.m_states.Add(s.state);
        }

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("program size"), int64(program.m_opAddress.Num()));

        // Merge the log in the right order
        genErrorLog->Merge( m_pD->m_pErrorLog.get() );
        m_pD->m_pErrorLog = genErrorLog.get();

		// Pack data
		int32 MinimumBytesPerRom = 1024; // \TODO: compilation parameter
		m_pD->GenerateRoms(pResult.Get(),MinimumBytesPerRom);

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("program size"), int64(program.m_opAddress.Num()));

        return pResult;
    }


    //---------------------------------------------------------------------------------------------
    ErrorLogPtrConst Compiler::GetLog() const
    {
        return m_pD->m_pErrorLog;
    }


	//---------------------------------------------------------------------------------------------
	namespace
	{
		inline void EnsureUniqueRomId(uint32& RomId, const mu::FProgram& program)
		{
			while (true)
			{
				bool bUnique = true;
				for (const FRomData& Rom : program.m_roms)
				{
					if (Rom.Id == RomId)
					{
						bUnique = false;
						break;
					}
				}

				if (bUnique)
				{
					break;
				}

				// Generate a new Id. It is not going to be stable accross build, which means it may hurt content patching
				// a little bit, but it shouldn't happen often.
				RomId++;
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void Compiler::Private::GenerateRoms(Model* p, int32 MinRomSize)
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(Mutable_GenerateRoms);

		mu::FProgram& program = p->GetPrivate()->m_program;

		int32 NumRoms = 0;
		int32 NumEmbedded = 0;

		// Serialise roms in a separate files if possible
		if (MinRomSize >= 0)
		{
			// Save images and unload from memory
			for (int32 ResourceIndex = 0; ResourceIndex < program.m_constantImageLODs.Num(); ++ResourceIndex)
			{
				TPair<int32, mu::ImagePtrConst>& ResData = program.m_constantImageLODs[ResourceIndex];

				// This shouldn't have been serialised with rom support before.
				check(ResData.Key < 0);

				// Serialize to memory, to find out final size of this rom
				OutputMemoryStream MemStream(1024 * 1024);
				OutputArchive MemoryArch(&MemStream);
				Image::Serialise(ResData.Value.get(), MemoryArch);

				// If the resource uses less memory than the threshold, don't save it in a separate rom.
				if (MemStream.GetBufferSize() < MinRomSize)
				{
					NumEmbedded++;
					continue;
				}

				NumRoms++;

				FRomData RomData;
				RomData.ResourceType = DT_IMAGE;
				RomData.ResourceIndex = ResourceIndex;
				RomData.Size = MemStream.GetBufferSize();

				// Ensure that the Id is unique
				RomData.Id = CityHash32(static_cast<const char*>(MemStream.GetBuffer()), MemStream.GetBufferSize());
				EnsureUniqueRomId(RomData.Id, program);

				int32 RomIndex = program.m_roms.Add(RomData);
				ResData.Key = RomIndex;
			}

			// Save meshes and unload from memory
			for (int32 ResourceIndex = 0; ResourceIndex < program.m_constantMeshes.Num(); ++ResourceIndex)
			{
				TPair<int32, mu::MeshPtrConst>& ResData = program.m_constantMeshes[ResourceIndex];

				// This shouldn't have been serialised with rom support before.
				check(ResData.Key < 0);

				// Serialize to memory, to find out final size of this rom
				OutputMemoryStream MemStream(1024 * 1024);
				OutputArchive MemoryArch(&MemStream);
				Mesh::Serialise(ResData.Value.get(), MemoryArch);

				// If the resource uses less memory than the threshold, don't save it in a separate rom.
				if (MemStream.GetBufferSize() < MinRomSize)
				{
					NumEmbedded++;
					continue;
				}

				NumRoms++;

				FRomData RomData;
				RomData.ResourceType = DT_MESH;
				RomData.ResourceIndex = ResourceIndex;
				RomData.Size = MemStream.GetBufferSize();

				// Ensure that the Id is unique
				RomData.Id = CityHash32(static_cast<const char*>(MemStream.GetBuffer()), MemStream.GetBufferSize());
				EnsureUniqueRomId(RomData.Id, program);

				int32 RomIndex = program.m_roms.Add(RomData);
				ResData.Key = RomIndex;
			}
		}

		UE_LOG(LogMutableCore, Log, TEXT("Generated roms for model with %d embedded constants and %d in roms."), NumEmbedded, NumRoms);
	}


}

