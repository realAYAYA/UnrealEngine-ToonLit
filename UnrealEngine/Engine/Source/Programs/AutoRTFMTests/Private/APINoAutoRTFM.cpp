// Copyright Epic Games, Inc. All Rights Reserved.

#include "API.h"

int NoAutoRTFM::DoSomethingC(int I)
{
    return I + 13;
}

int NoAutoRTFM::DoSomethingInTransactionC(int I, void*)
{
    return I + 42;
}

int NoAutoRTFM::DoSomethingCpp(int I)
{
    return I + 13;
}

int NoAutoRTFM::DoSomethingInTransactionCpp(int I, void*)
{
    return I + 42;
}

