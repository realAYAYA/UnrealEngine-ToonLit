// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttachToolkit, const TSharedRef<IToolkit>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDetachToolkit, const TSharedRef<IToolkit>&);

/** Persona asset editor toolkit wrapper, used to auto inject the persona editor mode manager */
class PERSONA_API FPersonaAssetEditorToolkit : public FWorkflowCentricApplication
{
public:
	/** FAssetEditorToolkit interface  */
	virtual void CreateEditorModeManager() override;

	// IToolkitHost Interface
	virtual void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit) override;

	// Returns the currently hosted toolkit. Can be invalid if no toolkit is being hosted.
	TSharedPtr<IToolkit> GetHostedToolkit() const { return HostedToolkit; }
	FOnAttachToolkit& GetOnAttachToolkit() { return OnAttachToolkit; }
	FOnDetachToolkit& GetOnDetachToolkit() { return OnDetachToolkit; }
	
protected:
	virtual void OnEditorModeIdChanged(const FEditorModeID& ModeChangedID, bool bIsEnteringMode);
	
protected:
	FOnAttachToolkit OnAttachToolkit;
	FOnDetachToolkit OnDetachToolkit;
	
	// The toolkit we're currently hosting.
	TSharedPtr<IToolkit> HostedToolkit;
		
};
