// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "Chaos/SimCallbackInput.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "UObject/WeakObjectPtr.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Templates/UniquePtr.h"
#include "ChaosUserDataPTStats.h"

/*

	Chaos User Data PT
	==================

	The idea behind this tool is to provide a generic way of associating
	custom data with physics particles, which is write-only from the game
	thread, and read-only from the physics thread.

	This comes in handy when physical interactions at the per-contact level
	need to be affected by gameplay properties.

	A TUserDataManagerPT is a sim callback which is templated on a type TUserData. It
	owns an instance of TUserData for each particle which has been used as an
	argument to SetData_GT.

	In order to use a TUserDataManagerPT, it will need to be created using the chaos
	solver's CreateAndRegisterSimCallbackObject_External method. This
	library does not natively provide a method of accessing the
	appropriate TUserDataManagerPT instance from the physics thread, but this can
	be achieved in a number of ways - it is left up to the game to decide
	how to do this for flexibility.

*/

namespace Chaos
{
	// TUserDataManagerPTInput
	//
	// Input is a collection of pointers to new and updated userdata objects
	// to be sent to the physics thread
	template <typename TUserData>
	struct TUserDataManagerPTInput : public FSimCallbackInput
	{
		// Map of particle unique indices to user data ptrs
		// 
		// NOTE: This is marked mutable because the userdata objects must be
		// moved to the internal array after making it
		// OnPreSimulate_Internal, but input objects are const in that
		// context. This might be frowned on, but since TUserDataManagerPTInput is a
		// class which is only used internally to TUserDataManagerPT, an argument
		// could be made either way.
		mutable TMap<FUniqueIdx, TUniquePtr<TUserData>> UserDataToAdd;

		// Set of particle unique indices for which to remove user data
		TSet<FUniqueIdx> UserDataToRemove;

		// Flag for clearing all data
		bool bClear = false;

		// Monotonically increasing identifier for the input object. Each
		// newly constructed input will store and increment this;
		int32 Identifier = -1;

		void Reset()
		{
			UserDataToAdd.Reset();
			UserDataToRemove.Reset();
			bClear = false;
			Identifier = -1;
		}
	};

	// TUserDataManagerPT
	//
	// A chaos callback object which stores and allows access to user data
	// associated with particles on the physics thread.
	//
	// Note that FSimCallbackOutput is the output struct - this carries no
	// data because this is a one-way callback. We use it basically just to
	// marshal data in one direction.
	template <typename TUserData>
	class TUserDataManagerPT : public TSimCallbackObject<
		TUserDataManagerPTInput<TUserData>,
		FSimCallbackNoOutput,
		ESimCallbackOptions::Presimulate | ESimCallbackOptions::ParticleUnregister>
	{
		using TInput = TUserDataManagerPTInput<TUserData>;

	public:

		virtual ~TUserDataManagerPT() { }

		// Add or update user data associated with this particle handle
		bool SetData_GT(const FRigidBodyHandle_External& Handle, const TUserData& UserData)
		{
			SCOPE_CYCLE_COUNTER(STAT_UserDataPT_SetData_GT);

			if (const FPhysicsSolverBase* Solver = this->GetSolver())
			{
				if (TInput* Input = this->GetProducerInputData_External())
				{
					// Add the data to the map to be sent to physics thread
					Input->UserDataToAdd.Emplace(Handle.UniqueIdx(), MakeUnique<TUserData>(UserData));

					// In case it was removed and then added again in the same
					// frame, untrack this particle for data removal
					Input->UserDataToRemove.Remove(Handle.UniqueIdx());

					// If this is a new input, set it's identifier
					if (Input->Identifier == -1)
					{
						Input->Identifier = InputIdentifier_GT++;
					}

					// Successfully queued for add/update
					return true;
				}
			}

			// Failed to queue for add/update
			return false;
		}

		// Remove user data associated with this particle handle
		bool RemoveData_GT(const FRigidBodyHandle_External& Handle)
		{
			SCOPE_CYCLE_COUNTER(STAT_UserDataPT_RemoveData_GT);

			if (this->GetSolver() != nullptr)
			{
				if (TInput* Input = this->GetProducerInputData_External())
				{
					// Track the particle for removal. No point in doing so
					// if we've already marked all data for clearing.
					if (!Input->bClear)
					{
						Input->UserDataToRemove.Add(Handle.UniqueIdx());
					}

					// In case it was added/updated and then removed in the
					// same frame, untrack the add/update
					Input->UserDataToAdd.Remove(Handle.UniqueIdx());

					// Successfully queued for removal
					return true;
				}
			}

			// Failed to queue for removal
			return false;
		}

		// TParticleHandle is generalized here because it can be
		// FRigidBodyHandle_Internal or FGeometryParticleHandle which have
		// the same api...
		template <typename TParticleHandle>
		const TUserData* GetData_PT(const TParticleHandle& Handle) const
		{
			SCOPE_CYCLE_COUNTER(STAT_UserDataPT_GetData_PT);

			const int32 Idx = Handle.UniqueIdx().Idx;
			return UserDataMap_PT.IsValidIndex(Idx) ? UserDataMap_PT[Idx].Get() : nullptr;
		}

		// Clear all data
		bool ClearData_GT()
		{
			if (TInput* Input = this->GetProducerInputData_External())
			{
				// All userdatas to add and remove can be cleared. Any userdatas that
				// are added after this point will still be added.
				Input->UserDataToAdd.Empty();
				Input->UserDataToRemove.Empty();

				// Mark the flag for clear all data
				Input->bClear = true;
			}

			// Failed to queue removal
			return false;
		}

	protected:

		virtual void OnPreSimulate_Internal() override
		{
			if (const TInput* Input = this->GetConsumerInput_Internal())
			{
				// Only proceed if the input has not yet been processed.
				// 
				// It's possible that we'll get multiple presimulate calls
				// with the same input because the same input continues to be
				// provided until a new one is received, so we cache the
				// timestamp of the last processed input to make sure that we
				// don't double-process it.
				if (InputIdentifier_PT != Input->Identifier)
				{
					InputIdentifier_PT = Input->Identifier;

					// Clear all data
					if (Input->bClear)
					{
						SCOPE_CYCLE_COUNTER(STAT_UserDataPT_ClearData_PT);

						// Empty the userdata map
						UserDataMap_PT.Empty();
						UserDataMap_PT.Shrink();
					}

					// Add new data
					if (Input->UserDataToAdd.Num() > 0)
					{
						SCOPE_CYCLE_COUNTER(STAT_UserDataPT_UpdateData_PT);

						// Move all the user data to the internal map
						for (auto& Iter : Input->UserDataToAdd)
						{
							UserDataMap_PT.EmplaceAt(Iter.Key.Idx, MoveTemp(Iter.Value));
						}
					}

					// Remove old data
					if (Input->UserDataToRemove.Num() > 0)
					{
						SCOPE_CYCLE_COUNTER(STAT_UserDataPT_RemoveData_PT);

						// Delete user data that has been removed
						for (const FUniqueIdx Idx : Input->UserDataToRemove)
						{
							if (UserDataMap_PT.IsValidIndex(Idx.Idx))
							{
								UserDataMap_PT.RemoveAt(Idx.Idx);
							}
						}

						// Shrink sparse array if we took elements off the end
						UserDataMap_PT.Shrink();
					}
				}
			}
		}

		virtual void OnParticleUnregistered_Internal(TArray<TTuple<FUniqueIdx, FSingleParticlePhysicsProxy*>>& UnregisteredProxies) override
		{
			if (UnregisteredProxies.Num() > 0)
			{
				// If any particles were removed in this physics tick, check to see if they have
				// userdata that we were tracking and remove those userdata instances.
				for (TTuple<FUniqueIdx, FSingleParticlePhysicsProxy*> Proxy : UnregisteredProxies)
				{
					const FUniqueIdx Idx = Proxy.Get<FUniqueIdx>();
					if (UserDataMap_PT.IsValidIndex(Idx.Idx))
					{
						UserDataMap_PT.RemoveAt(Idx.Idx);
					}
				}

				// Shrink sparse array if we took elements off the end
				UserDataMap_PT.Shrink();
			}
		}

		// Identifier of the next input to be created on the game thread
		int32 InputIdentifier_GT = 0;

		// Identifier of the last input to be consumed on the physics thread
		int32 InputIdentifier_PT = -1;

		// Map of particle unique ids to user data
		TSparseArray<TUniquePtr<TUserData>> UserDataMap_PT;
	};
}
