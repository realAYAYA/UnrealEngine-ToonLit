// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

 /* Dependencies
 *****************************************************************************/


 /* Interfaces
 *****************************************************************************/




#define FACTORY(ReturnType, Type, ...) \
class Type##Factory \
{ \
public: \
	static ReturnType Create(__VA_ARGS__); \
}; 

#define IFACTORY(ReturnType, Type, ...) \
class Type##Factory \
{ \
public: \
	virtual ReturnType Create(__VA_ARGS__) = 0; \
};

