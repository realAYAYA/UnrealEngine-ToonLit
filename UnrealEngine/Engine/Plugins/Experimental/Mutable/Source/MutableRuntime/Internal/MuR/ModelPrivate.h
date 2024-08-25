// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Model.h"
#include "MuR/System.h"

#include "MuR/SerialisationPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ExtensionData.h"
#include "MuR/ExtensionDataStreamer.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/MutableRuntimeModule.h"

#define MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE	64
#define MUTABLE_GROW_BORDER_VALUE					2

namespace mu
{

	/** Used to debug and log. */
	constexpr bool DebugRom = false;
	constexpr bool DebugRomAll = false;
	constexpr int32 DebugRomIndex = 44;
	constexpr int32 DebugImageIndex = 9;

    //! Data stored for a rom even if it is not loaded
    struct FRomData
    {
        //! This is used to identify a ROM file. It is usually a hash from its data.
        uint32 Id;

		//! Size of the rom
		uint32 Size;

		//! Index of the resource in its type-sepcific array
		uint32 ResourceIndex;

		//! Index of the resource in its type-sepcific array (one of DATATYPE values)
		uint32 ResourceType;        
    };

    MUTABLE_DEFINE_POD_SERIALISABLE(FRomData);

    //!
    template<typename DATA>
    inline void AppendCode(TArray<uint8>& code, const DATA& data )
    {
        int32 pos = code.Num();
        code.SetNum( pos+sizeof(DATA), EAllowShrinking::No);
		FMemory::Memcpy (&code[pos], &data, sizeof(DATA));
    }


	//!
	struct FImageLODRange
	{
		int32 FirstIndex = 0;
		uint16 ImageSizeX = 0;
		uint16 ImageSizeY = 0;
		uint16 _Padding = 0;
		uint8 LODCount = 0;
		EImageFormat ImageFormat = EImageFormat::IF_NONE;
	};
	MUTABLE_DEFINE_POD_SERIALISABLE(FImageLODRange);

	struct FExtensionDataConstant
	{
		// This should always be valid, but if the state is Unloaded it won't be usable.
		//
		// Avoid storing references to this Data in Memory while the state is Unloaded.
		ExtensionDataPtrConst Data;

		enum class ELoadState : uint8
		{
			Invalid,
			Unloaded,
			FailedToLoad,
			CurrentlyLoaded,
			AlwaysLoaded
		};

		// This should be initialized to a valid load state when Data is set
		ELoadState LoadState = ELoadState::Invalid;

		inline void Serialise(OutputArchive& arch) const
		{
			arch << Data;
		}
		
		inline void Unserialise(InputArchive& arch)
		{
			arch >> Data;

			check(Data.get());
			check(Data->Origin == ExtensionData::EOrigin::ConstantAlwaysLoaded
				|| Data->Origin == ExtensionData::EOrigin::ConstantStreamed);

			if (Data->Origin == ExtensionData::EOrigin::ConstantAlwaysLoaded)
			{
				LoadState = ELoadState::AlwaysLoaded;
			}
			else
			{
				// Streamed constants are assumed to be unloaded to start with
				LoadState = ELoadState::Unloaded;
			}
		}
	};

    //!
    struct FProgram
    {
        FProgram()
        {
            // Add the null instruction at address 0.
            // TODO: Will do it in the linker
            AppendCode( m_byteCode, OP_TYPE::NONE );
            m_opAddress.Add(0);
        }

        struct FState
        {
            //! Name of the state
            FString Name;

            //! First instruction of the full build of an instance in this state
            OP::ADDRESS m_root = 0;

            //! List of parameters index (to FProgram::m_parameters) of the runtime parameters of
            //! this state.
			TArray<int> m_runtimeParameters;

            //! List of instructions that need to be cached to efficiently update this state
			TArray<OP::ADDRESS> m_updateCache;

            //! List of root instructions for the dynamic resources that depend on the runtime
            //! parameters of this state, with a mask of relevant runtime parameters.
            //! The mask has a bit on for every runtime parameter in the m_runtimeParameters array.
			//! The uint64 is linked to MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE
			TArray< TPair<OP::ADDRESS,uint64> > m_dynamicResources;

            //!
            inline void Serialise( OutputArchive& arch ) const
            {
                arch << Name;
                arch << m_root;
                arch << m_runtimeParameters;
                arch << m_updateCache;
                arch << m_dynamicResources;
            }

            //!
            inline void Unserialise( InputArchive& arch )
            {
                arch >> Name;
                arch >> m_root;
                arch >> m_runtimeParameters;
                arch >> m_updateCache;
                arch >> m_dynamicResources;
            }

            /** Returns the mask of parameters (from the runtime parameter list of this state) including the parameters that 
			* are relevant for the dynamic resource at the given address.
			*/
            uint64 IsDynamic( OP::ADDRESS at ) const
            {
                uint64 res = 0;

                for ( int32 i=0; i<m_dynamicResources.Num(); ++i )
                {
                    if ( m_dynamicResources[i].Key==at )
                    {
                        res = m_dynamicResources[i].Value;
						break;
                    }
                }

                return res;
            }

            //!
            bool IsUpdateCache( OP::ADDRESS at ) const
            {
                bool res = false;

                for ( SIZE_T i=0; !res && i<m_updateCache.Num(); ++i )
                {
                    if ( m_updateCache[i]==at )
                    {
                        res = true;
                    }
                }

                return res;
            }

            //!
            void AddUpdateCache( OP::ADDRESS at )
            {
                if ( !IsUpdateCache(at) )
                {
                    m_updateCache.Add( at );
                }
            }
        };

        //! Location in the m_byteCode of the beginning of each operation
		TArray<uint32> m_opAddress;

        //! Byte-coded representation of the program, using variable-sized op data.
		TArray<uint8> m_byteCode;

        //!
		TArray<FState> m_states;

		//! Data for every rom.
		TArray<FRomData> m_roms;
	
		/** Loaded roms worth tracking (only images and meshes for now). Stores the rom's data type.*/
		TSparseArray<uint8> LoadedMemTrackedRoms;

		//! Constant image mip data: the first is the index in m_roms for each image mip or -1 if it is always loaded.
		TArray<TPair<int32, Ptr<const Image>>> ConstantImageLODs;

		//! Constant image mip chain indices: ranges in this array are defined in FImageLODRange and the indices here refer to ConstantImageLODs.
		TArray<uint32> m_constantImageLODIndices;

		//! Constant image data.
		TArray<FImageLODRange> m_constantImages;

        //! Constant mesh data: the first is the index in m_roms for each mesh or -1 if it is always loaded.
		TArray<TPair<int32, Ptr<const Mesh>>> ConstantMeshes;

		//! Constant ExtensionData
		TArray<FExtensionDataConstant> m_constantExtensionData;

        //! Constant string data
		TArray<FString> m_constantStrings;

        //! Constant layout data
		TArray<Ptr<const Layout>> m_constantLayouts;

        //! Constant projectors
		TArray<FProjector> m_constantProjectors;

        //! Constant matrices, usually used for transforms
		TArray<FMatrix44f> m_constantMatrices;

		//! Constant shapes
		TArray<FShape> m_constantShapes;

        //! Constant curves
		TArray<Curve> m_constantCurves;

        //! Constant skeletons
		TArray<Ptr<const Skeleton>> m_constantSkeletons;

		//! Constant Physics Bodies
		TArray<Ptr<const PhysicsBody>> m_constantPhysicsBodies;

        //! Parameters of the model.
        //! The value stored here is the default value.
		TArray<FParameterDesc> m_parameters;

        //! Ranges for iteration of the model operations.
		TArray<FRangeDesc> m_ranges;

        //! List of parameter lists. These are used in several places, like storing the
        //! pregenerated list of parameters influencing a resource.
        //! The parameter lists are sorted.
		TArray<TArray<uint16>> m_parameterLists;

#if WITH_EDITOR
		//! State of the program. True unless the streamed resources were destroyed,
		//! which could happen in the editor after recompiling the CO.
		bool bIsValid = true;
#endif
        //!
        void Serialise( OutputArchive& arch ) const
        {
            arch << m_opAddress;
            arch << m_byteCode;
            arch << m_states;
			arch << m_roms;
			arch << ConstantImageLODs;
			arch << m_constantImageLODIndices;
			arch << m_constantImages;
			arch << ConstantMeshes;
			arch << m_constantExtensionData;
			arch << m_constantStrings;
            arch << m_constantLayouts;
            arch << m_constantProjectors;
			arch << m_constantMatrices;
			arch << m_constantShapes;
            arch << m_constantCurves;
            arch << m_constantSkeletons;
			arch << m_constantPhysicsBodies;
            arch << m_parameters;
            arch << m_ranges;
            arch << m_parameterLists;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            arch >> m_opAddress;
            arch >> m_byteCode;
            arch >> m_states;
			arch >> m_roms;
			arch >> ConstantImageLODs;
			arch >> m_constantImageLODIndices;
			arch >> m_constantImages;
			arch >> ConstantMeshes;
			arch >> m_constantExtensionData;
			arch >> m_constantStrings;
            arch >> m_constantLayouts;
            arch >> m_constantProjectors;
			arch >> m_constantMatrices;
			arch >> m_constantShapes;
            arch >> m_constantCurves;
            arch >> m_constantSkeletons;
			arch >> m_constantPhysicsBodies;
            arch >> m_parameters;
            arch >> m_ranges;
            arch >> m_parameterLists;
        }

        //! Debug method that sanity-checks the program with a variety of tests.
        void Check();

        //! Debug method that logs the top used instruction types.
        void LogHistogram() const;

		//! Return true if the given ROM is loaded.
		FORCEINLINE bool IsRomLoaded(int32 RomIndex) const
		{
			switch (m_roms[RomIndex].ResourceType)
			{
				case DT_IMAGE: 
					return ConstantImageLODs[m_roms[RomIndex].ResourceIndex].Value.get() != nullptr;
				case DT_MESH: 
					return ConstantMeshes[m_roms[RomIndex].ResourceIndex].Value.get() != nullptr;
				default:
					check(false);
					break;
			}

			return false;
		}

		/** Unload a rom resource. Return the size of the unloaded rom.*/
		FORCEINLINE int32 UnloadRom(int32 RomIndex)
		{
			int32 RomSize = 0;

			if (DebugRom && (DebugRomAll||RomIndex == DebugRomIndex))
				UE_LOG(LogMutableCore, Log, TEXT("Unloading rom %d."), RomIndex);

			if (LoadedMemTrackedRoms.IsValidIndex(RomIndex))
			{
				LoadedMemTrackedRoms.RemoveAt(RomIndex);
			}

			switch (m_roms[RomIndex].ResourceType)
			{
			case DT_IMAGE:
				if (ConstantImageLODs[m_roms[RomIndex].ResourceIndex].Value)
				{
					RomSize = ConstantImageLODs[m_roms[RomIndex].ResourceIndex].Value->GetDataSize();
					ConstantImageLODs[m_roms[RomIndex].ResourceIndex].Value = nullptr;
				}
				break;
			case DT_MESH:
				if (ConstantMeshes[m_roms[RomIndex].ResourceIndex].Value)
				{
					RomSize = ConstantMeshes[m_roms[RomIndex].ResourceIndex].Value->GetDataSize();
					ConstantMeshes[m_roms[RomIndex].ResourceIndex].Value = nullptr;
				}
				break;
			default:
				check(false);
				break;
			}

			return RomSize;
		}

		FORCEINLINE void SetMeshRomValue(int32 RomIndex, const Ptr<Mesh>& Value)
		{
			check(m_roms[RomIndex].ResourceType == DT_MESH);
			
			LoadedMemTrackedRoms.EmplaceAt(RomIndex, (uint8)m_roms[RomIndex].ResourceType);
			ConstantMeshes[m_roms[RomIndex].ResourceIndex].Value = Value;
		}

		FORCEINLINE void SetImageRomValue(int32 RomIndex, const Ptr<Image>& Value)
		{
			check(m_roms[RomIndex].ResourceType == DT_IMAGE);
			
			LoadedMemTrackedRoms.EmplaceAt(RomIndex, (uint8)m_roms[RomIndex].ResourceType);
			ConstantImageLODs[m_roms[RomIndex].ResourceIndex].Value = Value;
		}


		int32 AddConstant(Ptr<const Mesh> pMesh)
		{
			// Uniques needs to be ensured outside
			return ConstantMeshes.Add(TPair<int32, Ptr<const Mesh>>( -1, pMesh.get() ));
		}

		OP::ADDRESS AddConstant(Ptr<const ExtensionData> Data)
		{
			// Ensure unique
			for (int32 Index = 0; Index < m_constantExtensionData.Num(); Index++)
			{
				const ExtensionData* Candidate = m_constantExtensionData[Index].Data.get();
				if (*Candidate == *Data)
				{
					return Index;
				}
			}

			FExtensionDataConstant& NewConstant = m_constantExtensionData.AddDefaulted_GetRef();
			NewConstant.Data = Data;

			// The data is assumed to be loaded during compilation
			NewConstant.LoadState =
				Data->Origin == ExtensionData::EOrigin::ConstantAlwaysLoaded
				? FExtensionDataConstant::ELoadState::AlwaysLoaded
				: FExtensionDataConstant::ELoadState::CurrentlyLoaded;

			return m_constantExtensionData.Num() - 1;
		}

		OP::ADDRESS AddConstant( Ptr<const Layout> pLayout )
        {
            // Ensure unique
            for ( SIZE_T i=0; i<m_constantLayouts.Num(); ++i)
            {
                if (m_constantLayouts[i]==pLayout)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantLayouts.Num() );
            m_constantLayouts.Add( pLayout );
            return index;
        }

        OP::ADDRESS AddConstant( Ptr<const Skeleton> pSkeleton )
        {
            // Ensure unique
            for ( SIZE_T i=0; i<m_constantSkeletons.Num(); ++i)
            {
                if ( m_constantSkeletons[i]==pSkeleton
                     ||
                     *m_constantSkeletons[i]==*pSkeleton
                     )
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantSkeletons.Num() );
            m_constantSkeletons.Add( pSkeleton );
            return index;
        }

        OP::ADDRESS AddConstant( Ptr<const PhysicsBody> pPhysicsBody )
        {
            // Ensure unique
            for ( SIZE_T i=0; i<m_constantPhysicsBodies.Num(); ++i)
            {
                if ( m_constantPhysicsBodies[i]==pPhysicsBody
                     ||
                     *m_constantPhysicsBodies[i]==*pPhysicsBody
                     )
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantPhysicsBodies.Num() );
            m_constantPhysicsBodies.Add( pPhysicsBody );
            return index;
        }

        OP::ADDRESS AddConstant( const FString& str )
        {            
            // Ensure unique
            for ( SIZE_T i=0; i<m_constantStrings.Num(); ++i)
            {
                if (m_constantStrings[i]==str)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantStrings.Num() );
            m_constantStrings.Add( str );
            return index;
        }

        OP::ADDRESS AddConstant( const FMatrix44f& m )
        {
            // Ensure unique
            for ( SIZE_T i=0; i<m_constantMatrices.Num(); ++i)
            {
                if (m_constantMatrices[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantMatrices.Num() );
            m_constantMatrices.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const FShape& m )
        {
            // Ensure unique
            for ( SIZE_T i=0; i<m_constantShapes.Num(); ++i)
            {
                if (m_constantShapes[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantShapes.Num() );
            m_constantShapes.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const FProjector& m )
        {
            // Ensure unique
            for ( SIZE_T i=0; i<m_constantProjectors.Num(); ++i)
            {
                if (m_constantProjectors[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantProjectors.Num() );
            m_constantProjectors.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const Curve& m )
        {
            // Ensure unique
//            for ( SIZE_T i=0; i<m_constantCurves.Num(); ++i)
//            {
//                if (m_constantCurves[i]==m)
//                {
//                    return (OP::ADDRESS)i;
//                }
//            }

            OP::ADDRESS index = OP::ADDRESS( m_constantCurves.Num() );
            m_constantCurves.Add( m );
            return index;
        }

        //! Get a constant image, assuming it is fully loaded. The image constant will be composed with lodaded mips if necessary.
		template <typename CreateImageFunc>
        void GetConstant( int32 ConstantIndex, ImagePtrConst& res, int32 MipsToSkip, const CreateImageFunc& CreateImage) const
        {
			int32 ReallySkippedLODs = FMath::Min(m_constantImages[ConstantIndex].LODCount - 1, MipsToSkip);
			int32 FirstLODIndexIndex = m_constantImages[ConstantIndex].FirstIndex;
			int32 ResultLODIndexIndex = FirstLODIndexIndex + ReallySkippedLODs;
			int32 FinalLODs = m_constantImages[ConstantIndex].LODCount - ReallySkippedLODs;
			check(FinalLODs > 0);

			// Get the first mip
			int32 ResultLODIndex = m_constantImageLODIndices[ResultLODIndexIndex];
			Ptr<const Image> CurrentMip = ConstantImageLODs[ResultLODIndex].Value;
			check(CurrentMip);
				
			// Shortcut if we only want one mip
			if (FinalLODs == 1)
			{
				res = CurrentMip;
				return;
			}

			// Compose the result image
			{
				MUTABLE_CPUPROFILER_SCOPE(ComposeConstantImage);

				Ptr<Image> Result = CreateImage(CurrentMip->GetSizeX(), CurrentMip->GetSizeY(), FinalLODs, CurrentMip->GetFormat(), EInitializationType::NotInitialized);
				Result->m_flags = CurrentMip->m_flags;

				// Some non-block pixel formats require separate memory size calculation
				if (Result->DataStorage.IsEmpty())
				{
					for (int32 LOD = 0; LOD < FinalLODs; ++LOD)
					{
						int32 LODIndex = m_constantImageLODIndices[ResultLODIndexIndex + LOD];
						int32 MipSizeBytes = ConstantImageLODs[LODIndex].Value->GetLODDataSize(0);
						Result->DataStorage.ResizeLOD(LOD, MipSizeBytes);
					}
				}

				for (int32 LOD = 0; LOD < FinalLODs; ++LOD)
				{
					check(CurrentMip->GetLODCount() == 1);
					check(CurrentMip->GetFormat() == Result->GetFormat());

					TArrayView<uint8> ResultLODView = Result->DataStorage.GetLOD(LOD);
					TArrayView<const uint8> CurrentMipView = CurrentMip->DataStorage.GetLOD(0);
					
					check(CurrentMipView.Num() == ResultLODView.Num());

					FMemory::Memcpy(ResultLODView.GetData(), CurrentMipView.GetData(), ResultLODView.Num());

					if (LOD + 1 < FinalLODs)
					{
						ResultLODIndex = m_constantImageLODIndices[ResultLODIndexIndex + LOD + 1];
						CurrentMip = ConstantImageLODs[ResultLODIndex].Value;
						check(CurrentMip);
					}
				}

				res = Result;
			}
		}

        void GetConstant(int32 ConstantIndex, MeshPtrConst& res) const
        {
			res = ConstantMeshes[ConstantIndex].Value;
		}

		void GetExtensionDataConstant(int32 ConstantIndex, ExtensionDataPtrConst& Result) const
		{
			const FExtensionDataConstant& Constant = m_constantExtensionData[ConstantIndex];

			check(Constant.LoadState != FExtensionDataConstant::ELoadState::Unloaded);
			check(Constant.Data.get());

			Result = Constant.Data;
		}

        inline OP_TYPE GetOpType( OP::ADDRESS at ) const
        {
            if (at>=OP::ADDRESS(m_opAddress.Num())) return OP_TYPE::NONE;

            OP_TYPE result;
            uint64 byteCodeAddress = m_opAddress[at];
			FMemory::Memcpy( &result, &m_byteCode[byteCodeAddress], sizeof(OP_TYPE) );
            check( result<OP_TYPE::COUNT);
            return result;
        }

        template<typename ARGS>
        inline const ARGS GetOpArgs( OP::ADDRESS at ) const
        {
            ARGS result;
            uint64 byteCodeAddress = m_opAddress[at];
            byteCodeAddress += sizeof(OP_TYPE);
			FMemory::Memcpy( &result, &m_byteCode[byteCodeAddress], sizeof(ARGS));
            return result;
        }

        inline const uint8* GetOpArgsPointer( OP::ADDRESS at ) const
        {
            uint64 byteCodeAddress = m_opAddress[at];
            byteCodeAddress += sizeof(OP_TYPE);
            const uint8* pData = (const uint8*)&m_byteCode[byteCodeAddress];
            return pData;
        }

        inline uint8* GetOpArgsPointer( OP::ADDRESS at )
        {
            uint64 byteCodeAddress = m_opAddress[at];
            byteCodeAddress += sizeof(OP_TYPE);
            uint8* pData = (uint8*)&m_byteCode[byteCodeAddress];
            return pData;
        }
    };


    //!
    class MUTABLERUNTIME_API Model::Private
    {
    public:

        //!
        FProgram m_program;


    	void UnloadRoms();


        //!
        void Serialise( OutputArchive& arch ) const
        {
            uint32 ver = 0;
            arch << ver;

            arch << m_program;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32 ver;
            arch >> ver;
            check(ver==0);

            arch >> m_program;
        }

    };


    //!
    class MUTABLERUNTIME_API ModelParametersGenerator::Private
    {
    public:

        //!
        TSharedPtr<const Model> m_pModel;
        Ptr<System> m_pSystem;

        //! Number of possible instances
        int64 m_instanceCount;

		/** Value used for scalar paramters that control multidemnsional sizes. 
		* TODO: Make it an option.
		*/
		int32 DefaultRangeDimension = 8;
    };

}
