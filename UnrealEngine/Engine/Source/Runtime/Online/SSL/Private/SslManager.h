// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_SSL

#include "CoreMinimal.h"
#include "CoreTypes.h"

#include "Interfaces/ISslManager.h"

/**
 * Manager of the ssl library
 */
class SSL_API FSslManager : ISslManager
{
public:

	//~ Begin ISslManager Interface
	virtual bool InitializeSsl() override;
	virtual void ShutdownSsl() override;
	virtual SSL_CTX* CreateSslContext(const FSslContextCreateOptions& CreateOptions) override;
	virtual void DestroySslContext(SSL_CTX* SslContext) override;
	//~ End ISslManager Interface

protected:
	/** Default constructor */
	FSslManager();
	/** Disable copy constructor */
	FSslManager(const FSslManager& Copy) = delete;
	/** SSL ref count */
	int32 InitCount;

	friend class FSslModule;
};

#endif // WITH_SSL
