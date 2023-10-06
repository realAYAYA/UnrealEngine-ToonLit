// Copyright Epic Games, Inc. All Rights Reserved.

#include "SettingsSection.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "UObject/Class.h"
#include "UObject/Reload.h"


/* FSettingsSection structors
 *****************************************************************************/

FSettingsSection::FSettingsSection( const ISettingsCategoryRef& InCategory, const FName& InName, const FText& InDisplayName, const FText& InDescription, const TWeakObjectPtr<UObject>& InSettingsObject )
	: Category(InCategory)
	, Description(InDescription)
	, DisplayName(InDisplayName)
	, Name(InName)
	, SettingsObject(InSettingsObject)
{ }


FSettingsSection::FSettingsSection( const ISettingsCategoryRef& InCategory, const FName& InName, const FText& InDisplayName, const FText& InDescription, const TSharedRef<SWidget>& InCustomWidget )
	: Category(InCategory)
	, CustomWidget(InCustomWidget)
	, Description(InDescription)
	, DisplayName(InDisplayName)
	, Name(InName)
{ }

#if WITH_RELOAD
void FSettingsSection::ReinstancingComplete(IReload* Reload)
{
	SettingsObject = Reload->GetReinstancedCDO(SettingsObject.Get(true));
}
#endif

/* ISettingsSection interface
 *****************************************************************************/

bool FSettingsSection::CanEdit() const
{
	if (CanEditDelegate.IsBound())
	{
		return CanEditDelegate.Execute();
	}

	return true;
}


bool FSettingsSection::CanExport() const
{
	return (ExportDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config)));
}


bool FSettingsSection::CanImport() const
{
	return (ImportDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config)));
}


bool FSettingsSection::CanResetDefaults() const
{
	return (ResetDefaultsDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config) && !SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig | CLASS_GlobalUserConfig | CLASS_ProjectUserConfig)));
}


bool FSettingsSection::CanSave() const
{
	return (SaveDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config)));
}


bool FSettingsSection::CanSaveDefaults() const
{
	return (SaveDefaultsDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config) && !SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig | CLASS_GlobalUserConfig | CLASS_ProjectUserConfig)));
}


bool FSettingsSection::Export( const FString& Filename )
{
	if (ExportDelegate.IsBound())
	{
		return ExportDelegate.Execute(Filename);
	}

	if (SettingsObject.IsValid())
	{
		SettingsObject->SaveConfig(CPF_Config, *Filename);

		return true;
	}

	return false;
}


TWeakPtr<ISettingsCategory> FSettingsSection::GetCategory()
{
	return Category;
}


TWeakPtr<SWidget> FSettingsSection::GetCustomWidget() const
{
	return CustomWidget;
}


const FText& FSettingsSection::GetDescription() const
{
	return Description;
}


const FText& FSettingsSection::GetDisplayName() const
{
	return DisplayName;
}


const FName& FSettingsSection::GetName() const
{
	return Name;
}


TWeakObjectPtr<UObject> FSettingsSection::GetSettingsObject() const
{
	return SettingsObject;
}


FText FSettingsSection::GetStatus() const
{
	if (StatusDelegate.IsBound())
	{
		return StatusDelegate.Execute();
	}

	return FText::GetEmpty();
}


bool FSettingsSection::HasDefaultSettingsObject()
{
	if (!SettingsObject.IsValid())
	{
		return false;
	}

	// @todo userconfig: Should we add GlobalUserConfig here?
	return SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig);
}


bool FSettingsSection::Import( const FString& Filename )
{
	if (ImportDelegate.IsBound())
	{
		return ImportDelegate.Execute(Filename);
	}

	if (SettingsObject.IsValid())
	{
		SettingsObject->LoadConfig(SettingsObject->GetClass(), *Filename, UE::LCPF_PropagateToInstances);

		return true;
	}

	return false;	
}


bool FSettingsSection::ResetDefaults()
{
	if (ResetDefaultsDelegate.IsBound())
	{
		return ResetDefaultsDelegate.Execute();
	}

	if (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config) && !SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig | CLASS_GlobalUserConfig | CLASS_ProjectUserConfig))
	{
		FString ConfigName = SettingsObject->GetClass()->GetConfigName();

		GConfig->EmptySection(*SettingsObject->GetClass()->GetPathName(), ConfigName);
		GConfig->Flush(false);

		FConfigContext::ForceReloadIntoGConfig().Load(*FPaths::GetBaseFilename(ConfigName));

		SettingsObject->ReloadConfig(nullptr, nullptr, UE::LCPF_PropagateToInstances|UE::LCPF_PropagateToChildDefaultObjects);

		return true;
	}

	return false;
}

bool FSettingsSection::NotifySectionOnPropertyModified()
{
	bool bShouldSaveChanges = true;
	// Notify the section that it has been modified
	if (ModifiedDelegate.IsBound())
	{
		// return value of FOnModified indicates whether the modifications should be saved.
		bShouldSaveChanges = ModifiedDelegate.Execute();
	}
	return bShouldSaveChanges;
}

bool FSettingsSection::Save()
{
	if (ModifiedDelegate.IsBound() && !ModifiedDelegate.Execute())
	{
		return false;
	}

	if (SaveDelegate.IsBound())
	{
		return SaveDelegate.Execute();
	}

	if (SettingsObject.IsValid())
	{
		if (SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig))
		{
			SettingsObject->TryUpdateDefaultConfigFile();
		}
		else if (SettingsObject->GetClass()->HasAnyClassFlags(CLASS_GlobalUserConfig))
		{
			SettingsObject->UpdateGlobalUserConfigFile();
		}
		else if (SettingsObject->GetClass()->HasAnyClassFlags(CLASS_ProjectUserConfig))
		{
			SettingsObject->UpdateProjectUserConfigFile();
		}
		else
		{
			SettingsObject->SaveConfig();
		}

		return true;
	}

	return false;
}


bool FSettingsSection::SaveDefaults()
{
	if (SaveDefaultsDelegate.IsBound())
	{
		return SaveDefaultsDelegate.Execute();
	}

	if (SettingsObject.IsValid())
	{
		SettingsObject->TryUpdateDefaultConfigFile();
		SettingsObject->ReloadConfig(nullptr, nullptr, UE::LCPF_PropagateToInstances);

		return true;			
	}

	return false;
}

void FSettingsSection::Select() 
{
	if (SelectDelegate.IsBound())
	{
		SelectDelegate.Execute();
	}
}
