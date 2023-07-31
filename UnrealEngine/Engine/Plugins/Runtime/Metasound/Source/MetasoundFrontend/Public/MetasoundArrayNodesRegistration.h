// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundArrayNodes.h"
#include "MetasoundArrayShuffleNode.h"
#include "MetasoundArrayRandomNode.h"
#include "MetasoundNodeRegistrationMacro.h"

#include <type_traits>

namespace Metasound
{
	template<typename ... ElementType>
	struct TEnableArrayNodes
	{
		static constexpr bool Value = true;
	};

	namespace MetasoundArrayNodesPrivate
	{
		// TArrayNodeSupport acts as a configuration sturct to determine whether
		// a particular TArrayNode can be instantiated for a specific ArrayType.
		//
		// Some ArrayNodes require that the array elements have certain properties
		// such as default element constructors, element copy constructors, etc.
		template<typename ArrayType>
		struct TArrayNodeSupport
		{
		private:
			using ElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;

			static constexpr bool bIsElementParsableAndAssignable = TIsParsable<ElementType>::Value && std::is_copy_assignable<ElementType>::value;

			static constexpr bool bEnabled = TEnableArrayNodes<ElementType>::Value;

		public:
			
			// Array num is supported for all array types.
			static constexpr bool bIsArrayNumSupported = bEnabled;

			// Element must be default parsable to create get operator because a
			// value must be returned even if the index is invalid. Also values are
			// assigned by copy.
			static constexpr bool bIsArrayGetSupported = bEnabled && bIsElementParsableAndAssignable;

			// Element must be copy assignable to set the value.
			static constexpr bool bIsArraySetSupported = bEnabled && std::is_copy_assignable<ElementType>::value && std::is_copy_constructible<ElementType>::value;

			// Elements must be copy constructible
			static constexpr bool bIsArrayConcatSupported = bEnabled && std::is_copy_constructible<ElementType>::value;

			// Elements must be copy constructible
			static constexpr bool bIsArraySubsetSupported = bEnabled && std::is_copy_constructible<ElementType>::value;

			// Array shuffle is supported for all types that get is supported for.
			static constexpr bool bIsArrayShuffleSupported = bEnabled && bIsElementParsableAndAssignable;

			// Random get is supported for all types that get is supported for.
			static constexpr bool bIsArrayRandomGetSupported = bEnabled && bIsElementParsableAndAssignable;
		};

		template<typename ArrayType, typename std::enable_if<TArrayNodeSupport<ArrayType>::bIsArrayGetSupported, bool>::type = true>
		bool RegisterArrayGetNode()
		{
			using FNodeType = typename Metasound::TArrayGetNode<ArrayType>;
			return RegisterNodeWithFrontend<FNodeType>();
		}

		template<typename ArrayType, typename std::enable_if<!TArrayNodeSupport<ArrayType>::bIsArrayGetSupported, bool>::type = true>
		bool RegisterArrayGetNode()
		{
			// No op if not supported
			return true;
		}

		template<typename ArrayType, typename std::enable_if<TArrayNodeSupport<ArrayType>::bIsArraySetSupported, bool>::type = true>
		bool RegisterArraySetNode()
		{
			using FNodeType = typename Metasound::TArraySetNode<ArrayType>;

			static_assert(TArrayNodeSupport<ArrayType>::bIsArraySetSupported, "TArraySetNode<> is not supported by array type");

			return RegisterNodeWithFrontend<FNodeType>();
		}

		template<typename ArrayType, typename std::enable_if<!TArrayNodeSupport<ArrayType>::bIsArraySetSupported, bool>::type = true>
		bool RegisterArraySetNode()
		{
			// No op if not supported
			return true;
		}

		template<typename ArrayType, typename std::enable_if<TArrayNodeSupport<ArrayType>::bIsArraySubsetSupported, bool>::type = true>
		bool RegisterArraySubsetNode()
		{
			using FNodeType = typename Metasound::TArraySubsetNode<ArrayType>;

			static_assert(TArrayNodeSupport<ArrayType>::bIsArraySubsetSupported, "TArraySubsetNode<> is not supported by array type");

			return RegisterNodeWithFrontend<FNodeType>();
		}

		template<typename ArrayType, typename std::enable_if<!TArrayNodeSupport<ArrayType>::bIsArraySubsetSupported, bool>::type = true>
		bool RegisterArraySubsetNode()
		{
			// No op if not supported
			return true;
		}

		template<typename ArrayType, typename std::enable_if<TArrayNodeSupport<ArrayType>::bIsArrayConcatSupported, bool>::type = true>
		bool RegisterArrayConcatNode()
		{
			using FNodeType = typename Metasound::TArrayConcatNode<ArrayType>;

			static_assert(TArrayNodeSupport<ArrayType>::bIsArrayConcatSupported, "TArrayConcatNode<> is not supported by array type");

			return RegisterNodeWithFrontend<FNodeType>();
		}

		template<typename ArrayType, typename std::enable_if<!TArrayNodeSupport<ArrayType>::bIsArrayConcatSupported, bool>::type = true>
		bool RegisterArrayConcatNode()
		{
			// No op if not supported
			return true;
		}

		template<typename ArrayType, typename std::enable_if<TArrayNodeSupport<ArrayType>::bIsArrayNumSupported, bool>::type = true>
		bool RegisterArrayNumNode()
		{
			return ensureAlways(RegisterNodeWithFrontend<Metasound::TArrayNumNode<ArrayType>>());
		}

		template<typename ArrayType, typename std::enable_if<!TArrayNodeSupport<ArrayType>::bIsArrayNumSupported, bool>::type = true>
		bool RegisterArrayNumNode()
		{
			// No op if not supported
			return true;
		}

		template<typename ArrayType, typename std::enable_if<TArrayNodeSupport<ArrayType>::bIsArrayShuffleSupported, bool>::type = true>
		bool RegisterArrayShuffleNode()
		{
			using FNodeType = typename Metasound::TArrayShuffleNode<ArrayType>;
			return RegisterNodeWithFrontend<FNodeType>();
		}

		template<typename ArrayType, typename std::enable_if<!TArrayNodeSupport<ArrayType>::bIsArrayShuffleSupported, bool>::type = true>
		bool RegisterArrayShuffleNode()
		{
			// No op if not supported
			return true;
		}

		template<typename ArrayType, typename std::enable_if<TArrayNodeSupport<ArrayType>::bIsArrayRandomGetSupported, bool>::type = true>
		bool RegisterArrayRandomGetNode()
		{
			using FNodeType = typename Metasound::TArrayRandomGetNode<ArrayType>;
			return RegisterNodeWithFrontend<FNodeType>();
		}

		template<typename ArrayType, typename std::enable_if<!TArrayNodeSupport<ArrayType>::bIsArrayRandomGetSupported, bool>::type = true>
		bool RegisterArrayRandomGetNode()
		{
			// No op if not supported
			return true;
		}
	}

	/** Registers all available array nodes which can be instantiated for the given
	 * ArrayType. Some nodes cannot be instantiated due to limitations of the 
	 * array elements.
	 */
	template<typename ArrayType>
	bool RegisterArrayNodes()
	{
		using namespace MetasoundArrayNodesPrivate;

		bool bSuccess = RegisterArrayNumNode<ArrayType>();
		bSuccess = bSuccess && RegisterArrayGetNode<ArrayType>();
		bSuccess = bSuccess && RegisterArraySetNode<ArrayType>();
		bSuccess = bSuccess && RegisterArraySubsetNode<ArrayType>();
		bSuccess = bSuccess && RegisterArrayConcatNode<ArrayType>();
		bSuccess = bSuccess && RegisterArrayShuffleNode<ArrayType>();
		bSuccess = bSuccess && RegisterArrayRandomGetNode<ArrayType>();
		return bSuccess;
	}
}
