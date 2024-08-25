// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <stdint.h>

// This header is meant to compile standalone and should therefore NOT depend
// on anything from the Zen tree

//////////////////////////////////////////////////////////////////////////
//
// IMPORTANT: Any development or changes to this header should be made
// in the Zen repository http://github.com/epicgames/zen even if you
// may find the header in the UE tree
//
//////////////////////////////////////////////////////////////////////////

namespace zen {

class TransportConnection;
class TransportPlugin;
class TransportServerConnection;
class TransportServer;

/*************************************************************************

   The following interfaces are implemented on the server side, and instances
   are provided to the plugins.

*************************************************************************/

/** Plugin-server interface for connection

	This is returned by a call to TransportServer::CreateConnectionHandler
	and there should be one instance created per established connection

	The plugin uses this interface to feed data into the server side
	protocol implementation which will parse the incoming messages and
	dispatch to appropriate request handlers and ultimately call into
	TransportConnection functions which write data back to the client
 */
class TransportServerConnection
{
public:
	virtual uint32_t AddRef() const									  = 0;
	virtual uint32_t Release() const								  = 0;
	virtual void	 OnBytesRead(const void* Buffer, size_t DataSize) = 0;
};

/** Server interface

 There will be one instance of this provided by the system to the transport plugin

 The plugin can use this to register new connections

 */
class TransportServer
{
public:
	virtual TransportServerConnection* CreateConnectionHandler(TransportConnection* Connection) = 0;
};

/*************************************************************************

   The following interfaces are to be implemented by transport plugins.

*************************************************************************/

/** Interface which needs to be implemented by a transport plugin

	This is responsible for setting up and running the communication
	for a given transport.

	Once initialized, the plugin should be ready to accept connections
	using its own execution resources (threads, thread pools etc)
 */
class TransportPlugin
{
public:
	virtual uint32_t	AddRef() const											  = 0;
	virtual uint32_t	Release() const											  = 0;
	virtual void		Configure(const char* OptionTag, const char* OptionValue) = 0;
	virtual void		Initialize(TransportServer* ServerInterface)			  = 0;
	virtual void		Shutdown()												  = 0;
	virtual const char* GetDebugName()											  = 0;

	/** Check whether this transport is usable.
	 */
	virtual bool IsAvailable() = 0;
};

/** A transport plugin provider needs to implement this interface

   The plugin should create one instance of this per established
   connection and register it with the TransportServer instance
   CreateConnectionHandler() function. The server will subsequently
   use this interface to write response data back to the client and
   to manage the connection life cycle in general
*/
class TransportConnection
{
public:
	virtual int64_t		WriteBytes(const void* Buffer, size_t DataSize) = 0;
	virtual void		Shutdown(bool Receive, bool Transmit)			= 0;
	virtual void		CloseConnection()								= 0;
	virtual const char* GetDebugName()									= 0;
};

}  // namespace zen

#if defined(_MSC_VER)
#	define DLL_TRANSPORT_API __declspec(dllexport)
#else
#	define DLL_TRANSPORT_API
#endif

extern "C"
{
	DLL_TRANSPORT_API zen::TransportPlugin* CreateTransportPlugin();
}

typedef zen::TransportPlugin* (*PfnCreateTransportPlugin)();
