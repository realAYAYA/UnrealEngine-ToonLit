// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "Units/Math/RigUnit_MathBool.h"
#include "Units/RigUnitTest.h"

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_MathBoolNot)
{
	Unit.Value = true;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.Value = false;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	return true;
}

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_MathBoolAnd)
{
	Unit.A = false;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = false;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	return true;
}

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_MathBoolNand)
{
	Unit.A = false;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	Unit.A = false;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	return true;
}

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_MathBoolOr)
{
	Unit.A = false;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = false;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	return true;

}

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_MathBoolEquals)
{
	Unit.A = false;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	Unit.A = false;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	return true;
}

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_MathBoolNotEquals)
{
	Unit.A = false;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	Unit.A = false;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = false;
	Execute();
	AddErrorIfFalse(Unit.Result == true, TEXT("unexpected result"));
	Unit.A = true;
	Unit.B = true;
	Execute();
	AddErrorIfFalse(Unit.Result == false, TEXT("unexpected result"));
	return true;
}

#endif
