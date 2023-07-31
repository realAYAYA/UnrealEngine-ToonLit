// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCAddress.h"

#include "Audio/AudioAddressPattern.h"


namespace OSC
{
	const FString BundleTag = TEXT("#bundle");
	const FString PathSeparator = TEXT("/");
} // namespace OSC


FOSCAddress::FOSCAddress()
	: bIsValidPattern(false)
	, bIsValidPath(false)
	, Hash(GetTypeHash(GetFullPath()))
{
}

FOSCAddress::FOSCAddress(const FString& InValue)
	: bIsValidPattern(false)
	, bIsValidPath(false)
{
	InValue.ParseIntoArray(Containers, *OSC::PathSeparator, true);
	if (Containers.Num() > 0)
	{
		Method = Containers.Pop();
	}

	CacheAggregates();
}

void FOSCAddress::CacheAggregates()
{
	const bool bInvalidateSeparator = false;
	const FString FullPath = GetFullPath();

	Hash = GetTypeHash(FullPath);
	bIsValidPath = FAudioAddressPattern::IsValidPath(FullPath, bInvalidateSeparator);
	bIsValidPattern = FAudioAddressPattern::IsValidPattern(Containers, Method);
}

bool FOSCAddress::Matches(const FOSCAddress& InAddress) const
{
	if (IsValidPattern() && InAddress.IsValidPath())
	{
		return FAudioAddressPattern::PartsMatch(GetFullPath(), InAddress.GetFullPath());
	}

	return false;
}

bool FOSCAddress::IsValidPattern() const
{
	return bIsValidPattern;
}

bool FOSCAddress::IsValidPath() const
{
	return bIsValidPath;
}

void FOSCAddress::PushContainer(const FString& Container)
{
	if (Container.Contains(OSC::PathSeparator))
	{
		UE_LOG(LogOSC, Warning, TEXT("Failed to push container on OSCAddress. "
			"Cannot contain OSC path separator '%s'."), *OSC::PathSeparator);
		return;
	}

	Containers.Push(Container);
	CacheAggregates();
}

void FOSCAddress::PushContainers(const TArray<FString>& InContainers)
{
	for (const FString& Container : InContainers)
	{
		if (Container.Contains(OSC::PathSeparator))
		{
			UE_LOG(LogOSC, Warning, TEXT("Failed to push containers on OSCAddress. "
				"Cannot contain OSC path separator '%s'."), *OSC::PathSeparator);
			return;
		}
	}

	for (const FString& Container : InContainers)
	{
		Containers.Push(Container);
	}

	CacheAggregates();
}

FString FOSCAddress::PopContainer()
{
	FString Popped;
	if (Containers.Num() > 0)
	{
		Popped = Containers.Pop(false /* bAllowShrinking */);
		Hash = GetTypeHash(GetFullPath());
	}

	return Popped;
}

TArray<FString> FOSCAddress::PopContainers(int32 InNumContainers)
{
	TArray<FString> Popped;
	if (InNumContainers <= 0 || Containers.Num() == 0)
	{
		return Popped;
	}

	int32 Removed = 0;
	for (int32 i = Containers.Num() - 1; i >= 0; --i)
	{
		if (Removed > InNumContainers)
		{
			break;
		}
		++Removed;
		Popped.Add(Containers.Pop(false /* bAllowShrinking */));
	}

	Hash = GetTypeHash(GetFullPath());
	return Popped;
}

void FOSCAddress::RemoveContainers(int32 InIndex, int32 InCount)
{
	if (InIndex >= 0 && InCount > 0)
	{
		if (InIndex + InCount < Containers.Num())
		{
			Containers.RemoveAt(InIndex, InCount);
		}
	}
}

void FOSCAddress::ClearContainers()
{
	Containers.Reset();
	CacheAggregates();
}

const FString& FOSCAddress::GetMethod() const
{
	return Method;
}

void FOSCAddress::SetMethod(const FString& InMethod)
{
	if (InMethod.IsEmpty())
	{
		UE_LOG(LogOSC, Warning, TEXT("Failed to set OSCAddress method. "
			"'InMethod' cannot be empty string."));
		return;
	}

	if (InMethod.Contains(OSC::PathSeparator))
	{
		UE_LOG(LogOSC, Warning, TEXT("Failed to set OSCAddress method. "
			"Cannot contain OSC path separator '%s'."), *OSC::PathSeparator);
		return;
	}

	Method = InMethod;
	CacheAggregates();
}

FString FOSCAddress::GetContainerPath() const
{
	return OSC::PathSeparator + FString::Join(Containers, *OSC::PathSeparator);
}

FString FOSCAddress::GetContainer(int32 Index) const
{
	if (Index >= 0 && Index < Containers.Num())
	{
		return Containers[Index];
	}

	return FString();
}

void FOSCAddress::GetContainers(TArray<FString>& OutContainers) const
{
	OutContainers = Containers;
}

FString FOSCAddress::GetFullPath() const
{
	if (Containers.Num() == 0)
	{
		return OSC::PathSeparator + Method;
	}

	return GetContainerPath() + OSC::PathSeparator + Method;
}
