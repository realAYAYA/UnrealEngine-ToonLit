// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "WebSocketNetworkingDelegates.h"
#include <string>

/**
 * Represents a directory on disk that can be mapped to a url route and served.
 * 
 * For example we may wish to map `C:/www/Public` to `/` of our webserver.
 **/
class WEBSOCKETNETWORKING_API FWebSocketHttpMount
{
public:

	/**
	 * Sets the absolute path on disk to the directory we wish to serve.
	 * @param InPathOnDisk The absolute path to the directory.
	 */
	void SetPathOnDisk(const FString& InPathOnDisk) { PathOnDisk = std::string(TCHAR_TO_ANSI(*InPathOnDisk)); }
	
	/**
	 * Sets The web url path to use for this mount, e.g. /images .
	 * @param InWebPath The web url path we will map the directory to, e.g. /images.
	 */
	void SetWebPath(const FString& InWebPath) { WebPath = std::string(TCHAR_TO_ANSI(*InWebPath)); }

	/**
	 * Sets a file to serve when the root web path is requested.
	 * @param InDefaultFile The file to serve when the root web path is request, e.g. index.html.
	 */
	void SetDefaultFile(const FString& InDefaultFile) { DefaultFile = std::string(TCHAR_TO_ANSI(*InDefaultFile)); }

	const char* GetPathOnDisk() { return PathOnDisk.c_str(); }
	const char* GetWebPath() { return WebPath.c_str(); }
	const char* GetDefaultFile() { return DefaultFile.c_str(); }
	bool HasDefaultFile() { return DefaultFile.empty(); }

private:

	// Note: The below members are std::string purposefully as lws requires char* strings and FString may not be char*.

	/* The absolute path on disk to directory we wish to serve. */
	std::string PathOnDisk;

	/* The web url path to use for this mount, e.g. /images */
	std::string WebPath;

	/* When the root of the `WebPath` is requested, without a file, we can serve this file, e.g. index.html */
	std::string DefaultFile = "index.html";
};

class WEBSOCKETNETWORKING_API IWebSocketServer
{
public:
	virtual ~IWebSocketServer() {}

	/**
	 * Extends the websocket server to additionally serve static content from on-disk directories using HTTP.
	 * These HTTP requests will use the same port as the one specified for websocket use in `Init`.
	 * Note: This function will not start the webserver, call `Init` to start the websocket/webserver.
	 * @param DirectoriesToServe The directories for the Webserver to serve.
	 */
	virtual void EnableHTTPServer(TArray<FWebSocketHttpMount> DirectoriesToServe) = 0;

	/**
	 * Initialize the server and start listening for messages.
	 * @param Port the port to handle websockets messages on.
	 * @param ClientConnectedCallback the handler called when a client has connected.
	 * @return whether the initialization was successful.
	 */
	virtual bool Init(uint32 Port, FWebSocketClientConnectedCallBack ClientConnectedCallback) = 0;

	/** Tick the server. */
	virtual void Tick() = 0;

	/** Describe this libwebsocket server */
	virtual FString Info() = 0;
};