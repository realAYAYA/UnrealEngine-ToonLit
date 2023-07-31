// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "User/SocialUser.h"

DECLARE_DELEGATE_RetVal_OneParam(bool, FOnCustomIsInteractionAvailable, const USocialUser&);

/**
 * Link between the class-polymorphism-based interaction handle and the static template-polymorphism-based interactions.
 * Implementation detail, automatically set up and utilized in the DECLARE_SOCIAL_INTERACTION macros above
 */
class ISocialInteractionWrapper
{
public:
	virtual ~ISocialInteractionWrapper() {}

	virtual FName GetInteractionName() const = 0;
	virtual FText GetDisplayName(const USocialUser& User) const = 0;
	virtual FString GetSlashCommandToken() const = 0;

	virtual bool IsAvailable(const USocialUser& User) const = 0;
	virtual void ExecuteInteraction(USocialUser& User) const = 0;
	virtual void ExecuteInteractionWithContext(USocialUser& User, const TMap<FString, FString>& AnalyticsContext) const = 0;
};

template <typename InteractionT>
class TSocialInteractionWrapper : public ISocialInteractionWrapper
{
public:
	virtual FName GetInteractionName() const override final { return InteractionT::GetInteractionName(); }
	virtual FText GetDisplayName(const USocialUser& User) const override final { return InteractionT::GetDisplayName(User); }
	virtual FString GetSlashCommandToken() const override final { return InteractionT::GetSlashCommandToken(); }

	virtual bool IsAvailable(const USocialUser& User) const override final { return InteractionT::IsAvailable(User); }
	virtual void ExecuteInteraction(USocialUser& User) const override final { InteractionT::ExecuteInteraction(User); }
	virtual void ExecuteInteractionWithContext(USocialUser& User, const TMap<FString, FString>& AnalyticsContext) const override final { InteractionT::ExecuteInteractionWithContext(User, AnalyticsContext); };

private:
	friend InteractionT;
	TSocialInteractionWrapper() {}

	FSocialInteractionHandle GetHandle() const { return FSocialInteractionHandle(*this); }
};

// Helper macros for declaring a social interaction class
// Establishes boilerplate behavior and declares all functions the user is required to provide

#define DECLARE_SOCIAL_INTERACTION_EXPORT(APIMacro, InteractionName)	\
class APIMacro FSocialInteraction_##InteractionName	\
{	\
public:	\
	static FSocialInteractionHandle GetHandle()	\
	{	\
		static const TSocialInteractionWrapper<FSocialInteraction_##InteractionName> InteractionWrapper;	\
		return InteractionWrapper.GetHandle();	\
	}	\
	static FName GetInteractionName()  \
	{ \
		return #InteractionName; \
	} \
	\
	static FText GetDisplayName(const USocialUser& User);	\
	static FString GetSlashCommandToken();	\
	\
	static bool IsAvailable(const USocialUser& User)	\
	{	\
		if (CanExecute(User))	\
		{	\
			return OnCustomIsInteractionAvailable().IsBound() ? OnCustomIsInteractionAvailable().Execute(User) : true;	\
		}	\
		return false;	\
	}	\
	\
	static void ExecuteInteractionWithContext(USocialUser& User, const TMap<FString, FString>& AnalyticsContext)	\
	{	\
		User.WithContext(AnalyticsContext, [](USocialUser& InUser) {	\
			ExecuteInteraction(InUser);	\
		});	\
	}	\
	\
	static void ExecuteInteraction(USocialUser& User);	\
	\
	static FOnCustomIsInteractionAvailable& OnCustomIsInteractionAvailable()	\
	{	\
		static FOnCustomIsInteractionAvailable CustomAvailabilityCheckDelegate;	\
		return CustomAvailabilityCheckDelegate;	\
	}	\
private: \
	static bool CanExecute(const USocialUser& User); \
}

#define DECLARE_SOCIAL_INTERACTION(InteractionName) DECLARE_SOCIAL_INTERACTION_EXPORT(, InteractionName)
