// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClassProxy.h"
#include "Logging/StructuredLog.h"
#include "Param/AnimNextClassExtensionLibrary.h"
#include "Param/ParamTypeHandle.h"
#include "EngineLogs.h"

namespace UE::AnimNext
{

FClassProxy::FClassProxy(const UClass* InClass)
	: Class(InClass)
{
	// Add any additional extension libraries that extend this class first
	{
		// We allow only native static 'accessor wrapper' functions (one object param and the return value)
		auto CheckSignature = [](const UFunction* InFunction, const UClass* InProxiedClass)
		{
			if(!InFunction->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_Static | FUNC_Native))
			{
				return false;
			}

			if(InFunction->NumParms != 2)
			{
				return false;
			}
		
			int32 ParamIndex = 0;
			for(TFieldIterator<FProperty> It(InFunction); It && (It->PropertyFlags & CPF_Parm); ++It, ++ParamIndex)
			{
				// Check parameter is an object of the expected class
				if(ParamIndex == 0)
				{
					FObjectProperty* ObjectProperty = CastField<FObjectProperty>(*It);
					if(ObjectProperty == nullptr)
					{ 
						return false;
					}
				
					if(!ObjectProperty->PropertyClass->IsChildOf(InProxiedClass))
					{
						return false;
					}
				}

				// Check return value
				if(ParamIndex == 1)
				{
					if((It->PropertyFlags & CPF_ReturnParm) == 0)
					{
						return false;
					}
				}
			}
			return true;
		};

		TArray<UClass*> Classes;
		GetDerivedClasses(UAnimNextClassExtensionLibrary::StaticClass(), Classes);
		for(UClass* ProxyClass : Classes)
		{
			UAnimNextClassExtensionLibrary* CDO = ProxyClass->GetDefaultObject<UAnimNextClassExtensionLibrary>();
			const UClass* ExtendedClass = CDO->GetSupportedClass();
			if(InClass->IsChildOf(ExtendedClass))
			{
				for(TFieldIterator<UFunction> It(ProxyClass); It; ++It)
				{
					UFunction* Function = *It;
					if(CheckSignature(Function, ExtendedClass))
					{
						const FProperty* ReturnProperty = Function->GetReturnProperty();
						FParamTypeHandle TypeHandle = FParamTypeHandle::FromProperty(ReturnProperty);
						if(TypeHandle.IsValid())
						{
							FString FunctionName = Function->GetName();

							// Remove common prefixes.
							// TODO: explicit mapping or regex transformations?
							FunctionName.RemoveFromStart(TEXT("K2_"));
							FunctionName.RemoveFromStart(TEXT("Get"));

							FClassProxyParameter Parameter;
							Parameter.AccessType = EClassProxyParameterAccessType::HoistedFunction;
							Parameter.ClassParameterName = *FunctionName;
							Parameter.Function = Function;
							Parameter.Type = TypeHandle.GetType();
#if WITH_EDITOR
							Parameter.Tooltip = Function->GetToolTipText();
							Parameter.bThreadSafe = Function->HasMetaData("BlueprintThreadSafe");
#endif
							ParameterNameMap.Add(Parameter.ClassParameterName, Parameters.Add(Parameter));
						}
					}
				}
			}
		}
	}

	// Add functions as the next priority (extensions have already been added above so will take priority with duplicate names)
	for(TFieldIterator<UFunction> It(InClass, EFieldIterationFlags::IncludeSuper | EFieldIterationFlags::IncludeInterfaces); It; ++It)
	{
		UFunction* Function = *It;

		// We add only 'accessor' functions (no params apart from the return value) that have valid return types
		const FProperty* ReturnProperty = Function->GetReturnProperty();
		if(ReturnProperty != nullptr && Function->NumParms == 1 && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			FParamTypeHandle TypeHandle = FParamTypeHandle::FromProperty(ReturnProperty);
			if(TypeHandle.IsValid())
			{
				FString FunctionName = Function->GetName();

				// Remove common prefixes.
				// TODO: explicit mapping or regex transformations?
				FunctionName.RemoveFromStart(TEXT("K2_"));
				FunctionName.RemoveFromStart(TEXT("Get"));

				FName ParameterName = *FunctionName;
				if(!ParameterNameMap.Contains(ParameterName))
				{
					FClassProxyParameter Parameter;
					Parameter.AccessType = EClassProxyParameterAccessType::AccessorFunction;
					Parameter.ClassParameterName = *FunctionName;
					Parameter.Function = Function;
					Parameter.Type = TypeHandle.GetType();
#if WITH_EDITOR
					Parameter.Tooltip = Function->GetToolTipText();
					Parameter.bThreadSafe = Function->HasMetaData("BlueprintThreadSafe");
#endif
					ParameterNameMap.Add(Parameter.ClassParameterName, Parameters.Add(Parameter));
				}
			}
		}
	}

	// Finally add properties (accessors and extensions have already been added above so will take priority with duplicate names)
	for(TFieldIterator<FProperty> It(InClass, EFieldIterationFlags::IncludeSuper | EFieldIterationFlags::IncludeInterfaces); It; ++It)
	{
		FProperty* Property = *It;
		if(Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			FParamTypeHandle TypeHandle = FParamTypeHandle::FromProperty(Property);
			if(TypeHandle.IsValid())
			{
				FString PropertyName = Property->GetName();
				if(TypeHandle.GetParameterType() == FParamTypeHandle::EParamType::Bool)
				{
					PropertyName.RemoveFromStart(TEXT("b"));
				}

				FName ParameterName = *PropertyName;
				if(!ParameterNameMap.Contains(ParameterName))
				{
					FClassProxyParameter Parameter;
					Parameter.AccessType = EClassProxyParameterAccessType::Property;
					Parameter.ClassParameterName = ParameterName;
					Parameter.Property = Property;
					Parameter.Type = TypeHandle.GetType();
#if WITH_EDITOR
					Parameter.Tooltip = Property->GetToolTipText();
					Parameter.bThreadSafe = false;
#endif
					ParameterNameMap.Add(Parameter.ClassParameterName, Parameters.Add(Parameter));
				}
			}
		}
	}
}

}
