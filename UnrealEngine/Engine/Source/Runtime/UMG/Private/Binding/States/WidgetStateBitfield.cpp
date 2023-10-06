// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Binding/States/WidgetStateBitfield.h"

#include "Binding/States/WidgetStateSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetStateBitfield)

FWidgetStateBitfield::FWidgetStateBitfield()
{
}

FWidgetStateBitfield::FWidgetStateBitfield(const FName InStateName)
{
	SetBinaryStateSlow(InStateName, true);
}

FWidgetStateBitfield::FWidgetStateBitfield(const FName InStateName, const uint8 InValue)
{
	SetEnumStateSlow(InStateName, InValue);
}

FWidgetStateBitfield FWidgetStateBitfield::operator~() const
{
	FWidgetStateBitfield Temp = *this;
	Temp.NegateStates();
	return Temp;
}

FWidgetStateBitfield FWidgetStateBitfield::Intersect(const FWidgetStateBitfield& Rhs) const
{
	FWidgetStateBitfield Temp;
	Temp.BinaryStates = BinaryStates & Rhs.BinaryStates;
	Temp.IsEnumStateNotAllowed = IsEnumStateUsed & Rhs.IsEnumStateUsed;

	// See'IsEnumStateUsed' in header, we want to track enum state usage additively even with '&'
	Temp.IsEnumStateUsed = IsEnumStateUsed | Rhs.IsEnumStateUsed;

	// Only set enum states if all RHS binary state tests passed, else enum pass can shadow binary failures
	bool bBinaryStatesPassed = !Rhs.BinaryStates || Temp.BinaryStates;
	if (bBinaryStatesPassed && Temp.IsEnumStateUsed != 0)
	{
		for (uint8 EnumStateIndex = 0; EnumStateIndex < EnumStates.Num(); EnumStateIndex++)
		{
			bool bIsStateUsed = Temp.IsEnumStateUsed & (1 << EnumStateIndex);

			if (bIsStateUsed)
			{
				bool bIsNotAllowedLhs = IsEnumStateNotAllowed & (1 << EnumStateIndex);
				bool bIsNotAllowedRhs = Rhs.IsEnumStateNotAllowed & (1 << EnumStateIndex);
				bool bIsAllowanceSame = bIsNotAllowedLhs == bIsNotAllowedRhs;

				if (bIsAllowanceSame)
				{
					// If the RHS doesn't use this enum state, carry over the LHS enum state set
					// Allows for tests against bitfields where RHS doesn't use LHS enum states
					// to pass (Ex: {checked, hovered} & {hovered} should pass).

					bool bRhsNotUsed = (Rhs.IsEnumStateUsed & (1 << EnumStateIndex)) == 0;

					if (bRhsNotUsed)
					{
						Temp.EnumStates[EnumStateIndex] = EnumStates[EnumStateIndex];
					}
					else
					{
						Temp.EnumStates[EnumStateIndex] = EnumStates[EnumStateIndex].Intersect(Rhs.EnumStates[EnumStateIndex]);
					}
				}
				else if (bIsNotAllowedRhs)
				{
					Temp.EnumStates[EnumStateIndex] = EnumStates[EnumStateIndex].Difference(Rhs.EnumStates[EnumStateIndex]);
				}
				else if (bIsNotAllowedLhs)
				{
					Temp.EnumStates[EnumStateIndex] = Rhs.EnumStates[EnumStateIndex].Difference(EnumStates[EnumStateIndex]);
				}
				else
				{
					checkf(false, TEXT("Should not be able to reach, all allowance combinations covered above"));
				}
			}
		}
	}

	return Temp;
}

FWidgetStateBitfield FWidgetStateBitfield::Union(const FWidgetStateBitfield& Rhs) const
{
	FWidgetStateBitfield Temp;
	Temp.BinaryStates = BinaryStates | Rhs.BinaryStates;
	Temp.IsEnumStateUsed = IsEnumStateUsed | Rhs.IsEnumStateUsed;

	// See'IsEnumStateNotAllowed' in header, we wain to maintain the allowed set so shrink the not allowed
	Temp.IsEnumStateNotAllowed = IsEnumStateNotAllowed & Rhs.IsEnumStateNotAllowed;

	if (Temp.IsEnumStateUsed != 0)
	{
		for (uint8 EnumStateIndex = 0; EnumStateIndex < EnumStates.Num(); EnumStateIndex++)
		{
			bool bIsStateUsed = Temp.IsEnumStateUsed & (1 << EnumStateIndex);
			bool bIsStateAllowedUsed = Temp.IsEnumStateUsed & (1 << EnumStateIndex);
			if (bIsStateUsed)
			{
				bool bIsNotAllowedLhs = IsEnumStateNotAllowed & (1 << EnumStateIndex);
				bool bIsNotAllowedRhs = Rhs.IsEnumStateNotAllowed & (1 << EnumStateIndex);
				bool bIsAllowanceSame = bIsNotAllowedLhs == bIsNotAllowedRhs;

				if (bIsAllowanceSame)
				{
					Temp.EnumStates[EnumStateIndex] = EnumStates[EnumStateIndex].Union(Rhs.EnumStates[EnumStateIndex]);
				}
				else if (bIsNotAllowedRhs)
				{
					Temp.EnumStates[EnumStateIndex] = EnumStates[EnumStateIndex];
				}
				else if (bIsNotAllowedLhs)
				{
					Temp.EnumStates[EnumStateIndex] = Rhs.EnumStates[EnumStateIndex];
				}
				else
				{
					checkf(false, TEXT("Should not be able to reach, all allowance combinations covered above"));
				}
			}
		}
	}

	return Temp;
}

FWidgetStateBitfield::operator bool() const
{
	// We have to check 'IsEnumStateUsed' here since otherwise a completely empty statefield would pass
	return (BinaryStates != 0 || IsEnumStateUsed != 0) && !HasEmptyUsedEnumStates();
}

bool FWidgetStateBitfield::HasBinaryStates() const
{
	return BinaryStates != 0;
}

bool FWidgetStateBitfield::HasEnumStates() const
{
	return IsEnumStateUsed != 0;
}

bool FWidgetStateBitfield::HasEmptyUsedEnumStates() const
{
	if (IsEnumStateUsed != 0)
	{
		for (uint8 EnumStateIndex = 0; EnumStateIndex < EnumStates.Num(); EnumStateIndex++)
		{
			if (IsEnumStateUsed & (1 << EnumStateIndex))
			{
				if (EnumStates[EnumStateIndex].IsEmpty())
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool FWidgetStateBitfield::HasAnyFlags(const FWidgetStateBitfield& InBitfield) const
{
	return HasAnyBinaryFlags(InBitfield) || HasAnyEnumFlags(InBitfield);
}

bool FWidgetStateBitfield::HasAllFlags(const FWidgetStateBitfield& InBitfield) const
{
	return HasAllBinaryFlags(InBitfield) && HasAllEnumFlags(InBitfield);
}

bool FWidgetStateBitfield::HasAnyBinaryFlags(const FWidgetStateBitfield& InBitfield) const
{
	return (BinaryStates & InBitfield.BinaryStates) != 0;
}

bool FWidgetStateBitfield::HasAllBinaryFlags(const FWidgetStateBitfield& InBitfield) const
{
	return (BinaryStates & InBitfield.BinaryStates) == InBitfield.BinaryStates;
}

bool FWidgetStateBitfield::HasAnyEnumFlags(const FWidgetStateBitfield& InBitfield) const
{
	const uint16 IsEnumStateShared = IsEnumStateUsed & InBitfield.IsEnumStateUsed;

	if (IsEnumStateShared != 0)
	{
		for (uint8 EnumStateIndex = 0; EnumStateIndex < EnumStates.Num(); EnumStateIndex++)
		{
			if (IsEnumStateShared & (1 << EnumStateIndex))
			{
				if (!EnumStates[EnumStateIndex].Intersect(InBitfield.EnumStates[EnumStateIndex]).IsEmpty())
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool FWidgetStateBitfield::HasAllEnumFlags(const FWidgetStateBitfield& InBitfield) const
{
	const uint16 IsEnumStateShared = IsEnumStateUsed & InBitfield.IsEnumStateUsed;

	if (IsEnumStateShared == InBitfield.IsEnumStateUsed)
	{
		for (uint8 EnumStateIndex = 0; EnumStateIndex < EnumStates.Num(); EnumStateIndex++)
		{
			if (InBitfield.IsEnumStateUsed & (1 << EnumStateIndex))
			{
				if (!EnumStates[EnumStateIndex].Includes(InBitfield.EnumStates[EnumStateIndex]))
				{
					return false;
				}
			}
		}

		return true;
	}

	return false;
}

void FWidgetStateBitfield::SetState(const FWidgetStateBitfield& InBitfield)
{
	*this = InBitfield;
}

void FWidgetStateBitfield::NegateStates()
{
	NegateBinaryStates();
	NegateEnumStates();
}

void FWidgetStateBitfield::SetBinaryState(uint8 BinaryStateIndex, bool BinaryStateValue)
{
	BinaryStates = (BinaryStates & ~(static_cast<uint64>(1) << BinaryStateIndex)) | (static_cast<uint64>(BinaryStateValue) << BinaryStateIndex);
}

void FWidgetStateBitfield::SetBinaryState(const FWidgetStateBitfield& BinaryStateBitfield, bool BinaryStateValue)
{
	if (BinaryStateValue)
	{
		BinaryStates = BinaryStates | BinaryStateBitfield.BinaryStates;
	}
	else
	{
		BinaryStates = BinaryStates & ~(BinaryStateBitfield.BinaryStates);
	}
}

void FWidgetStateBitfield::SetBinaryStateSlow(FName BinaryStateName, bool BinaryStateValue)
{
	uint8 BinaryStateIndex = UWidgetStateSettings::Get()->GetBinaryStateIndex(BinaryStateName);
	SetBinaryState(BinaryStateIndex, BinaryStateValue);
}

void FWidgetStateBitfield::NegateBinaryStates()
{
	BinaryStates = ~BinaryStates;
}

void FWidgetStateBitfield::SetEnumState(uint8 EnumStateIndex, uint8 EnumStateValue)
{
	IsEnumStateUsed = IsEnumStateUsed | (1 << EnumStateIndex);
	EnumStates[EnumStateIndex] = { EnumStateValue };
}

void FWidgetStateBitfield::SetEnumState(const FWidgetStateBitfield& EnumStateBitfield)
{
	IsEnumStateUsed |= EnumStateBitfield.IsEnumStateUsed;

	for (uint8 EnumStateIndex = 0; EnumStateIndex < EnumStates.Num(); EnumStateIndex++)
	{
		if (EnumStateBitfield.IsEnumStateUsed & (1 << EnumStateIndex))
		{
			EnumStates[EnumStateIndex] = EnumStateBitfield.EnumStates[EnumStateIndex];
		}
	}
}

void FWidgetStateBitfield::SetEnumStateSlow(FName EnumStateName, uint8 EnumStateValue)
{
	uint8 EnumStateIndex = UWidgetStateSettings::Get()->GetEnumStateIndex(EnumStateName);
	SetEnumState(EnumStateIndex, EnumStateValue);
}

void FWidgetStateBitfield::ClearEnumState(const FWidgetStateBitfield& EnumStateBitfield)
{
	IsEnumStateUsed &= ~EnumStateBitfield.IsEnumStateUsed;

	for (uint8 EnumStateIndex = 0; EnumStateIndex < EnumStates.Num(); EnumStateIndex++)
	{
		if (EnumStateBitfield.IsEnumStateUsed & (1 << EnumStateIndex))
		{
			EnumStates[EnumStateIndex].Empty();
		}
	}
}

void FWidgetStateBitfield::ClearEnumState(uint8 EnumStateIndex)
{
	IsEnumStateUsed = IsEnumStateUsed & ~(1 << EnumStateIndex);
	EnumStates[EnumStateIndex].Empty();
}

void FWidgetStateBitfield::ClearEnumState(FName EnumStateName)
{
	uint8 EnumStateIndex = UWidgetStateSettings::Get()->GetEnumStateIndex(EnumStateName);
	ClearEnumState(EnumStateIndex);
}

void FWidgetStateBitfield::NegateEnumStates()
{
	IsEnumStateNotAllowed = ~IsEnumStateNotAllowed;
}
