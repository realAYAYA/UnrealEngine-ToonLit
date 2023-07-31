// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"

namespace Metasound
{
	/** A MetaSound Variable contains a data reference's prior and current value.
	 *
	 * @tparam DataType - Underlying data type of data reference.
	 */
	template<typename DataType>
	struct TVariable
	{
		using FWriteReference = TDataWriteReference<DataType>;
		using FReadReference = TDataReadReference<DataType>;

		TVariable() = delete;
		TVariable(TVariable&&) = default;
		TVariable(const TVariable&) = default;


		/** Create a delayed variable.
		 *
		 * @param InDelayedReference - A writable reference which will hold the delayed
		 *                           version of the data reference.
		 */
		TVariable(FWriteReference InDelayedReference)
		: DelayedDataReference(InDelayedReference)
		, DataReference(InDelayedReference)
		{
		}

		void CopyReferencedData()
		{
			*DelayedDataReference = *DataReference;
		}

		void SetDataReference(FReadReference InDataReference)
		{
			DataReference = InDataReference;
		}

		/** Get the current data reference. */
		FReadReference GetDataReference() const
		{
			return DataReference;
		}

		/** Get the delayed data reference */
		FReadReference GetDelayedDataReference() const
		{
			return DelayedDataReference;
		}

	private:

		FWriteReference DelayedDataReference;
		FReadReference DataReference;
	};

	/** Template to determine if data type is a variable. */
	template<typename DataType>
	struct TIsVariable
	{
		static constexpr bool Value = false;
	};

	/** Template specialization to determine if data type is a variable. */
	template<typename UnderlyingDataType>
	struct TIsVariable<TVariable<UnderlyingDataType>>
	{
		static constexpr bool Value = true;
	};
}

