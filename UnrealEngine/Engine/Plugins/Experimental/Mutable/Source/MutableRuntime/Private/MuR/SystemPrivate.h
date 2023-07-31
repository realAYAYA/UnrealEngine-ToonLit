// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/System.h"

#include "MuR/Settings.h"
#include "MuR/Operations.h"
#include "MuR/ImagePrivate.h"
#include "MuR/MutableString.h"
#include "MuR/MeshPrivate.h"
#include "MuR/InstancePrivate.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{

	//! Reference-counted colour to be stored in the cache
	class Colour : public RefCounted
	{
	public:
		Colour(vec4f v = vec4f()) : m_colour(v) {}
		vec4f m_colour;
	};
	typedef Ptr<Colour> ColourPtr;


	//! Reference-counted bool to be stored in the cache
	class Bool : public RefCounted
	{
	public:
		Bool(bool v = false) : m_value(v) {}
		bool m_value;
	};
	typedef Ptr<Bool> BoolPtr;


	//! Reference-counted scalar to be stored in the cache
	class Scalar : public RefCounted
	{
	public:
		Scalar(float v = 0.0f) : m_value(v) {}
		float m_value;
	};
	typedef Ptr<Scalar> ScalarPtr;


	//! Reference-counted scalar to be stored in the cache
	class Int : public RefCounted
	{
	public:
		Int(int32 v = 0) : m_value(v) {}
		int32 m_value;
	};
	typedef Ptr<Int> IntPtr;


	//! Reference-counted scalar to be stored in the cache
	class Projector : public RefCounted
	{
	public:
		PROJECTOR m_value;
	};
	typedef Ptr<Projector> ProjectorPtr;


	//! ExecutinIndex stores the location inside all ranges of the execution of a specific
	//! operation. The first integer on each pair is the dimension/range index in the program
	//! array of ranges, and the second integer is the value inside that range.
	//! The vector order is undefined.
	class ExecutionIndex : public  TArray<TPair<int32, int32>>
	{
	public:
		//! Set or add a value to the index
		void SetFromModelRangeIndex(uint16 rangeIndex, int rangeValue)
		{
			auto Index = IndexOfByPredicate([=](const ElementType& v) { return v.Key >= rangeIndex; });
			if (Index != INDEX_NONE && (*this)[Index].Key == rangeIndex)
			{
				// Update
				(*this)[Index].Value = rangeValue;
			}
			else
			{
				// Add new
				Push(ElementType(rangeIndex, rangeValue));
			}
		}

		//! Get the value of the index from the range index in the model.
		int GetFromModelRangeIndex(int modelRangeIndex) const
		{
			for (const auto& e : *this)
			{
				if (e.Key == modelRangeIndex)
				{
					return e.Value;
				}
			}
			return 0;
		}
	};


	struct SCHEDULED_OP
	{
		SCHEDULED_OP()
		{
			stage = 0;
			Type = EType::Full;
		}

		SCHEDULED_OP(OP::ADDRESS _at,
			const SCHEDULED_OP& opTemplate,
			uint8 _stage = 0,
			uint32 _customState = 0)
		{
			at = _at;
			executionOptions = opTemplate.executionOptions;
			executionIndex = opTemplate.executionIndex;
			stage = _stage;
			customState = _customState;
			Type = opTemplate.Type;
		}

		static inline SCHEDULED_OP FromOpAndOptions(OP::ADDRESS _at,
			const SCHEDULED_OP& opTemplate,
			uint8 _executionOptions)
		{
			SCHEDULED_OP r;
			r.at = _at;
			r.executionOptions = _executionOptions;
			r.executionIndex = opTemplate.executionIndex;
			r.stage = 0;
			r.customState = opTemplate.customState;
			r.Type = opTemplate.Type;
			return r;
		}

		//! Address of the operation
		OP::ADDRESS at = 0;

		//! Additional custom state data that the operation can store. This is usually used to pass information
		//! between execution stages of an operation.
		uint32 customState = 0;

		//! Index of the operation execution: This is used for iteration of different ranges.
		//! It is an index into the CodeRunner::GetMemory()::m_rangeIndex vector.
		//! executionIndex 0 is always used for empty ExecutionIndex, which is the most common
		//! one.
		uint16 executionIndex = 0;

		//! Additional execution options. Set externally to this op, it usually alters the result.
		//! For example, this is used to keep track of the mipmaps to skip in image operations.
		uint8 executionOptions = 0;

		//! Internal stage of the operation.
		//! Stage 0 is usually scheduling of children, and 1 is execution. Some instructions
		//! may have more steges to schedule children that are optional for execution, etc.
		uint8 stage : 4;

		//!
		enum class EType : uint8
		{
			//! Execute the operation to calculate the full result
			Full,

			//! Execute the operation to obtain the descriptor of an image.
			ImageDesc
		};
		EType Type : 4;
	};

	inline uint32 GetTypeHash(const SCHEDULED_OP& op)
	{
		return HashCombine(::GetTypeHash(op.at), HashCombine(::GetTypeHash(op.stage), ::GetTypeHash(op.executionIndex)));
	}


	//! A cache address is the operation plus the context of execution (iteration indices, etc...).
	struct CACHE_ADDRESS
	{
		OP::ADDRESS at = 0;
		uint16 executionIndex = 0;
		uint8 executionOptions = 0;
		SCHEDULED_OP::EType Type = SCHEDULED_OP::EType::Full;

		CACHE_ADDRESS() {}

		CACHE_ADDRESS(OP::ADDRESS _at, uint16 _executionIndex, uint8 _executionOptions)
		{
			at = _at;
			executionIndex = _executionIndex;
			executionOptions = _executionOptions;
		}

		CACHE_ADDRESS(OP::ADDRESS _at, const SCHEDULED_OP& item)
		{
			at = _at;
			executionIndex = item.executionIndex;
			executionOptions = item.executionOptions;
			Type = item.Type;
		}

		CACHE_ADDRESS(const SCHEDULED_OP& item)
		{
			at = item.at;
			executionIndex = item.executionIndex;
			executionOptions = item.executionOptions;
			Type = item.Type;
		}
	};


	inline uint32 GetTypeHash(const CACHE_ADDRESS& a)
	{
		return HashCombine(::GetTypeHash(a.at), a.executionIndex);
	}


	//! Container that stores data per executable code operation (indexed by address and execution
	//! index).
	template<class DATA>
	class CodeContainer
	{
	public:

		void resize(size_t s)
		{
			m_index0.SetNum(s);
			FMemory::Memzero(m_index0.GetData(), s * sizeof(DATA));
		}

		uint32 size_code() const
		{
			return uint32(m_index0.Num());
		}

		void clear()
		{
			m_index0.Empty();
			m_otherIndex.Empty();
		}

		inline void erase(const CACHE_ADDRESS& at)
		{
			if (at.executionIndex == 0 && at.executionOptions == 0)
			{
				m_index0[at.at] = nullptr;
			}
			else
			{
				m_otherIndex.Remove(at);
			}
		}

		inline DATA get(const CACHE_ADDRESS& at) const
		{
			if (at.executionIndex == 0 && at.executionOptions == 0)
			{
				if (at.at < uint32(m_index0.Num()))
				{
					return m_index0[at.at];
				}
				else
				{
					return 0;
				}
			}
			else
			{
				const DATA* it = m_otherIndex.Find(at);
				if (it)
				{
					return *it;
				}
			}
			return 0;
		}

		inline const DATA* get_ptr(const CACHE_ADDRESS& at) const
		{
			if (at.executionIndex == 0 && at.executionOptions == 0)
			{
				if (at.at < uint32(m_index0.Num()))
				{
					return &m_index0[at.at];
				}
				else
				{
					return nullptr;
				}
			}
			else
			{
				const DATA* it = m_otherIndex.Find(at);
				if (it)
				{
					return it;
				}
			}
			return nullptr;
		}

		inline DATA& operator[](const CACHE_ADDRESS& at)
		{
			if (at.executionIndex == 0 && at.executionOptions == 0)
			{
				return m_index0[at.at];
			}
			else
			{
				return m_otherIndex.FindOrAdd(at);
			}
		}

		struct iterator
		{
		private:
			friend class CodeContainer<DATA>;
			const CodeContainer<DATA>* container;
			typename TArray<DATA>::TIterator it0;
			typename TMap<CACHE_ADDRESS, DATA>::TIterator it1;

			iterator(CodeContainer<DATA>* InContainer)
				: container(InContainer)
				, it0(InContainer->m_index0.CreateIterator())
				, it1(InContainer->m_otherIndex.CreateIterator())
			{
			}

		public:

			inline void operator++(int)
			{
				if (it0)
				{
					++it0;
				}
				else
				{
					++it1;
				}
			}

			iterator operator++()
			{
				if (it0)
				{
					++it0;
				}
				else
				{
					++it1;
				}
				return *this;
			}

			inline bool operator!=(const iterator& o) const
			{
				return it0 != o.it0 || it1 != o.it1;
			}

			inline DATA& operator*()
			{
				if (it0)
				{
					return *it0;
				}
				else
				{
					return it1->Value;
				}
			}

			inline CACHE_ADDRESS get_address() const
			{
				if (it0)
				{
					return { uint32_t(it0.GetIndex()), 0, 0};
				}
				else
				{
					return it1->Key;
				}
			}

			inline bool IsValid() const
			{
				return bool(it0) && bool(it1);
			}
		};

		inline iterator begin()
		{
			iterator it(this);
			return it;
		}

	private:
		// For index 0
		TArray<DATA> m_index0;

		// For index>0
		TMap<CACHE_ADDRESS, DATA> m_otherIndex;
	};


	/** Interface for storage of data while Mutable code is being executed. */
	class FProgramCache
	{
	private:

		TArray< ExecutionIndex, TInlineAllocator<4> > m_usedRangeIndices;

		// first value of the pair:
		// 0 : value not valid (not set).
		// 1 : valid, not worth freeing for memory
		// 2 : valid, worth freeing
		CodeContainer< TPair<int32, Ptr<const RefCounted>> > m_resources;

		// TODO
	public:

		//! Addressed with OP::ADDRESS. It is true if value for an image desc is valid.
		TArray<bool> m_descCache;

		//! The number of times an operation will be run for the current build operation.
		//! \todo: this could be optimised by merging in other CodeContainers here.
		CodeContainer<int> m_opHitCount;

	public:


		inline const ExecutionIndex& GetRageIndex(uint32_t i)
		{
			// Make sure we have the default element.
			if (m_usedRangeIndices.IsEmpty())
			{
				m_usedRangeIndices.Push(ExecutionIndex());
			}

			check(i < uint32_t(m_usedRangeIndices.Num()));
			return m_usedRangeIndices[i];
		}

		//!
		inline uint32_t GetRageIndexIndex(const ExecutionIndex& rangeIndex)
		{
			if (rangeIndex.IsEmpty())
			{
				return 0;
			}

			// Make sure we have the default element.
			if (m_usedRangeIndices.IsEmpty())
			{
				m_usedRangeIndices.Push(ExecutionIndex());
			}

			// Look for or add the new element.
			auto ElemIndex = m_usedRangeIndices.Find(rangeIndex);
			if (ElemIndex != INDEX_NONE)
			{
				return ElemIndex;
			}

			m_usedRangeIndices.Push(rangeIndex);
			return uint32_t(m_usedRangeIndices.Num()) - 1;
		}

		void Init(size_t size)
		{
			m_resources.resize(size);
			m_opHitCount.resize(size);
		}

		void SetUnused(CACHE_ADDRESS at)
		{
			//UE_LOG(LogMutableCore, Log, TEXT("memory SetUnused : %5d "), at.at);
			if (m_resources[at].Key >= 2)
			{
				// Keep the result anyway if it doesn't use any memory.
				if (m_resources[at].Value)
				{
					m_resources[at].Value = nullptr;
					m_resources[at].Key = 0;
				}
			}
		}


		bool IsValid(CACHE_ADDRESS at)
		{
			if (at.at == 0) return false;

			// Is it a desc data query?
			if (at.Type == SCHEDULED_OP::EType::ImageDesc)
			{
				if (uint32(m_descCache.Num()) > at.at)
				{
					return m_descCache[at.at];
				}
				else
				{
					return false;
				}
			}

			// It's a full data query.
			auto d = m_resources.get_ptr(at);
			return d && d->Key != 0;
		}


		void ClearCacheLayer0()
		{
			CodeContainer<int>::iterator it = m_opHitCount.begin();
			for (; it.IsValid(); ++it)
			{
				int32 Count = *it;
				if (Count < 3000000)
				{
					SetUnused(it.get_address());
					*it = 0;
				}
			}
		}


		void ClearCacheLayer1()
		{
			CodeContainer<int>::iterator it = m_opHitCount.begin();
			for (; it.IsValid(); ++it)
			{
				m_resources[it.get_address()].Value = nullptr;
				m_resources[it.get_address()].Key = 0;
			}

			m_descCache.SetNum(0);
		}


		void Clear()
		{
			size_t codeSize = m_resources.size_code();
			m_resources.clear();
			m_resources.resize(codeSize);
			m_descCache.SetNum(0);
			m_opHitCount.clear();
			m_opHitCount.resize(codeSize);
		}


		bool GetBool(CACHE_ADDRESS at)
		{
			if (!at.at) return false;
			auto d = m_resources.get_ptr(at);
			if (!d) return false;

			Ptr<const Bool> pResult;
			if (at.at)
			{
				pResult = (const Bool*)d->Value.get();
			}
			return pResult ? pResult->m_value : false;
		}

		float GetScalar(CACHE_ADDRESS at)
		{
			if (!at.at) return 0.0f;
			auto d = m_resources.get_ptr(at);
			if (!d) return 0.0f;

			Ptr<const Scalar> pResult;
			if (at.at)
			{
				pResult = (const Scalar*)d->Value.get();
			}
			return pResult ? pResult->m_value : 0.0f;
		}

		int GetInt(CACHE_ADDRESS at)
		{
			if (!at.at) return 0;
			auto d = m_resources.get_ptr(at);
			if (!d) return 0;

			Ptr<const Int> pResult;
			if (at.at)
			{
				pResult = (const Int*)d->Value.get();
			}
			return pResult ? pResult->m_value : 0;
		}

		vec4f GetColour(CACHE_ADDRESS at)
		{
			if (!at.at) return vec4f();
			auto d = m_resources.get_ptr(at);
			if (!d) return vec4f();

			Ptr<const Colour> pResult;
			if (at.at)
			{
				pResult = (const Colour*)d->Value.get();
			}
			return pResult ? pResult->m_colour : vec4f();
		}

		Ptr<const Projector> GetProjector(CACHE_ADDRESS at)
		{
			if (!at.at) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Projector> pResult;
			if (at.at)
			{
				pResult = (const Projector*)d->Value.get();
			}
			return pResult;
		}

		Ptr<const Instance> GetInstance(CACHE_ADDRESS at)
		{
			if (!at.at) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Instance> pResult;
			if (at.at)
			{
				pResult = (const Instance*)d->Value.get();
			}
			return pResult;
		}


		Ptr<const Layout> GetLayout(CACHE_ADDRESS at)
		{
			if (!at.at) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Layout> pResult;
			if (at.at)
			{
				pResult = (const Layout*)d->Value.get();
			}
			return pResult;
		}

		Ptr<const String> GetString(CACHE_ADDRESS at)
		{
			if (!at.at)
				return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d)
				return nullptr;

			Ptr<const String> pResult;
			if (at.at)
			{
				pResult = (const String*)d->Value.get();
			}
			return pResult;
		}

		void SetBool(CACHE_ADDRESS at, bool v)
		{
			check(at.at < m_resources.size_code());

			Ptr<Bool> pResult = new Bool;
			pResult->m_value = v;
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, pResult);
		}

		void SetValidDesc(CACHE_ADDRESS at)
		{
			check(at.Type == SCHEDULED_OP::EType::ImageDesc);
			check(at.at < uint32(m_descCache.Num()));

			m_descCache[at.at] = true;
		}

		void SetScalar(CACHE_ADDRESS at, float v)
		{
			check(at.at < m_resources.size_code());

			Ptr<Scalar> pResult = new Scalar;
			pResult->m_value = v;
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, pResult);
		}

		void SetInt(CACHE_ADDRESS at, int v)
		{
			check(at.at < m_resources.size_code());

			Ptr<Int> pResult = new Int;
			pResult->m_value = v;
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, pResult);
		}

		void SetColour(CACHE_ADDRESS at, const vec4f& v)
		{
			check(at.at < m_resources.size_code());

			Ptr<Colour> pResult = new Colour;
			pResult->m_colour = v;
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, pResult);
		}

		void SetProjector(CACHE_ADDRESS at, Ptr<const Projector> v)
		{
			check(at.at < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, v);
		}

		void SetInstance(CACHE_ADDRESS at, Ptr<const Instance> v)
		{
			check(at.at < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, v);
		}

		void SetImage(CACHE_ADDRESS at, Ptr<const Image> v)
		{
			check(at.at < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(2, v);
		}

		void SetMesh(CACHE_ADDRESS at, Ptr<const Mesh> v)
		{
			// debug
//            if (v)
//            {
//                v->GetPrivate()->CheckIntegrity();
//            }

			check(at.at < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(2, v);
		}

		void SetLayout(CACHE_ADDRESS at, Ptr<const Layout> v)
		{
			check(at.at < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, v);
		}

		void SetString(CACHE_ADDRESS at, Ptr<const String> v)
		{
			check(at.at < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, v);
		}


		inline void IncreaseHitCount(CACHE_ADDRESS at)
		{
			m_opHitCount[at] += 1;
		}

		inline void SetForceCached(OP::ADDRESS at)
		{
			m_opHitCount[{at, 0, 0}] = 0xffffff;
		}


		Ptr<const Image> GetImage(CACHE_ADDRESS at)
		{
			if (!at.at) return nullptr;
			if (size_t(at.at) >= m_resources.size_code()) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Image> pResult = (const Image*)d->Value.get();

			// We need to decrease the hit count even if the result is null.
			// Lower hit counts means we shouldn't clear the value
			if ( m_opHitCount[at] > 0)
			{
				--m_opHitCount[at];
				if (m_opHitCount[at] <= 0)
				{
					SetUnused(at);
				}
			}

			return pResult;
		}

		Ptr<const Mesh> GetMesh(CACHE_ADDRESS at)
		{
			if (!at.at) return nullptr;
			if (size_t(at.at) >= m_resources.size_code()) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Mesh> pResult = (const Mesh*)d->Value.get();

			// We need to decrease the hitcount even if the result is null.
			// Lower hit counts means we shouldn't clear the value
			if ( m_opHitCount[at] > 0)
			{
				--m_opHitCount[at];

				if (m_opHitCount[at] <= 0)
				{
					SetUnused(at);
				}
			}

			return pResult;
		}
	};


	inline bool operator==(const CACHE_ADDRESS& a, const CACHE_ADDRESS& b)
	{
		return a.at == b.at
			&&
			a.executionIndex == b.executionIndex
			&&
			a.executionOptions == b.executionOptions
			&&
			a.Type == b.Type;
	}

	inline bool operator<(const CACHE_ADDRESS& a, const CACHE_ADDRESS& b)
	{
		if (a.at < b.at) return true;
		if (a.at > b.at) return false;
		if (a.executionIndex < b.executionIndex) return true;
		if (a.executionIndex > b.executionIndex) return false;
		if (a.executionOptions < b.executionOptions) return true;
		if (a.executionOptions > b.executionOptions) return false;
		return a.Type < b.Type;
	}



    //---------------------------------------------------------------------------------------------
    //! Struct to manage models for which we have streamed rom data.
    //---------------------------------------------------------------------------------------------
    struct FModelCache
    {
        struct FModelCacheEntry
        {
            //!
            WeakPtr<Model> m_pModel;

            // Rom streaming management
            TArray< TPair<uint64,uint64> > m_romWeight;
        };

        // Rom streaming management
        uint64 m_romBudget = 0;

        uint64 m_romTick = 0;
        TArray< FModelCacheEntry > m_cachePerModel;

        //! Make sure the cached memory is below the internal budget, even counting with the
        //! passed additional memory.
		uint64 EnsureCacheBelowBudget(uint64 AdditionalMemory, 
			TFunctionRef<bool(const Model*, int)> IsRomLockedFunc = [](const Model*, int) {return false;});

        //!
        void UpdateForLoad( int romIndex, const Model* pModel,
                            TFunctionRef<bool(const Model*,int)> isRomLockedFunc );
        void MarkRomUsed( int romIndex, const Model* pModel );

        //! Private helper
		FModelCacheEntry& GetModelCache( const Model* m );
    };


    //---------------------------------------------------------------------------------------------
    //! Abstract private system interface
    //---------------------------------------------------------------------------------------------
    class System::Private : public Base
    {
    public:

        Private( SettingsPtr pSettings );
        virtual ~Private();

        //-----------------------------------------------------------------------------------------
        //! Own interface
        //-----------------------------------------------------------------------------------------

		/** This method can be used to internally prepare for code execution. */
		MUTABLERUNTIME_API void BeginBuild(const Ptr<const Model>&);
		MUTABLERUNTIME_API void EndBuild();

		MUTABLERUNTIME_API bool BuildBool(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API int BuildInt(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API float BuildScalar(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API void BuildColour(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at, float* pR, float* pG, float* pB) ;
		MUTABLERUNTIME_API Ptr<const String> BuildString(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API Ptr<const Image> BuildImage(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at, int32 MipsToSkip) ;
		MUTABLERUNTIME_API Ptr<const Mesh> BuildMesh(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API Ptr<const Layout> BuildLayout(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
    	MUTABLERUNTIME_API Ptr<const Projector> BuildProjector(const Ptr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		void ClearCache() ;
		void SetStreamingCache(uint64 bytes) ;

        //!
        SettingsPtrConst m_pSettings;

        //! Data streaming interface, if any.
        //! It is owned by this system
        ModelStreamer* m_pStreamInterface;

        //! It is not owned by this system
        ImageParameterGenerator* m_pImageParameterGenerator;

        //! Maximum amount of memory (bytes) if a limit has been set. If 0, it means there is no
        //! limit.
        uint32 m_maxMemory;

        //! Current instances management
        //! @{

		/** Data for an instance that is currently being processed in the mutable system. This means it is
		* between a BeginUpdate and EndUpdate, or during an atmoic operation (like generate a single resource).
		*/
        struct FLiveInstance
        {
            Instance::ID m_instanceID;
            InstancePtrConst m_pInstance;
            ModelPtrConst m_pModel;

            int m_state;
            ParametersPtr m_pOldParameters;

			//! Mask of the parameters that have changed since the last update.
			//! Every bit represents a state parameter.
			uint64 m_updatedParameters = 0;

			//! An entry for every instruction in the program to cache resources (meshes, images) if necessary.
			TSharedPtr<FProgramCache> m_memory;
        };

        TArray<FLiveInstance> m_liveInstances;

		/** Temporary reference to the memory of the current instance being updated. */
		TSharedPtr<FProgramCache> m_memory;

		/** Counter used to generate unique IDs for every new instance created in the system. */
        Instance::ID m_lastInstanceID = 0;

		/** The pointer returned by this function is only valid for the duration of the current mutable operation. */
        inline FLiveInstance* FindLiveInstance( Instance::ID id )
        {
            for ( int32 i=0; i<m_liveInstances.Num(); ++i )
            {
                if (m_liveInstances[i].m_instanceID==id)
                {
                    return &m_liveInstances[i];
                }
            }
            return nullptr;
        }

        //! @}

        //!
        bool CheckUpdatedParameters( const FLiveInstance* pLiveInstance,
			const Ptr<const Parameters>& pParams,
			uint64& OutUpdatedParameters );

        // Profile metrics
        std::atomic<uint64> m_profileMetrics[ size_t(ProfileMetric::_Count) ];


	public:

		void RunCode(const Ptr<const Model>& pModel,
			const Parameters* pParams,
			OP::ADDRESS at, uint32 LODs = System::AllLODs, uint8 executionOptions = 0);

		//!
		void PrepareCache(const Ptr<const Model>& pModel, int state);

		//! Cache related members
		//! @{

		//! Data for each used model.
		FModelCache m_modelCache;

		//! @}

	private:

		/** This flag is turned on when a streaming error or similar happens. Results are not usable.
		* This should only happen in-editor.
		*/
		bool bUnrecoverableError = false;

    };

}
