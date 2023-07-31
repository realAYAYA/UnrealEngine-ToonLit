# RemoteSession
A plugin for Unreal that allows one instance to act as a thin-client (rendering and input) to a second instance

## Setup

* Enable the Remote Session plugin in your project
* Start your project with -game or use Play In New Editor window (Play In Viewport is not currently supported)
* Start the RemoteSessionApp on your mobile device and enter the IP address of the machine running the game


## Config Options

There are a number of options that can be configured by putting one or more of the following in your DefaultEngine.ini under [/Script/RemoteSession.RemoteSessionSettings]

The values shown below are the current defaults. If you do not want to change from the defaults then I recommend not adding the line to your ini file incase they change in the future.

    [/Script/RemoteSession.RemoteSessionSettings]
    ; Port that the host will listen on
	HostPort = 2049
	; Whether RemoteSession is functional in a shipping build
	bAllowInShipping = false
	; Does PIE automatically start hosting a session?
	bAutoHostWithPIE = true
	; Does launching a game automatically host a session?
	bAutoHostWithGame = true
	; Image quality (1-100)
	ImageQuality = 80
	; Framerate of from the host (will be min of this and the actual game)
	FrameRate = 60
	; Restrict to these channels. If empty all registered channels are available
	+AllowedChannels="ChannelName"
	; Don't allow these channels to be used
	+DeniedChannels="ChannelName"


Framerate and Quality can be adjusted at runtime via the remote.framerate and remote.quality cvars.


## API Details

Remote Session uses OSC over TCP (4-byte packet with the size, followed by the OSC packet).

## Core API

### Initiating a Connection

To connect to the UE instance open a connection to the Remote Session Port (by default 2049).

The UE instance will send a 'Hello' message with the following format -

    Address: /RS.Hello
    tag: s
    param: VersionString
    Example: "1.1.0"

Your connection should send a message with the same format to /RS.Hello with a version string. (Note versions are currently divided purely between the 1.x.x legacy protocol which is not documented here, and the 1.1.0 protocol).

Once the host has received your version the connection is establishd.

### Channel Selection

Immediately after the connection is established the host will send a list of available channels.

    Address: /RS.ChannelList
    tag: i,s,s (repeats)
    param: ChannelCount, ChannelName, ChannelMode, ChannelName, ChannelMode
    Example: 2,"FRemoteSessionFrameBufferChannel", "Read", "FRemoteSessionInputChannel", "Write"

The client should save this list and use it to enable channels by sending the following message to the host

    Address: /RS.ChangeChannel
    tag: s,s,i
    param: ChannelName, ChannelMode, 0/1
    Example: "FRemoteSessionFrameBufferChannel", "Read", 1

    (Note: Rather than use the OSC T/F boolean tag we explicitly pass 1/0 in the argument data)


### FRemoteSessionFrameBufferChannel Channel API

Once a connection to the FRemoteSessionFrameBufferChannel channel is established via the ChangeChannel API the client will receive a stream of images that represent the framebuffer.

    Address: /Screen
    tag: i,i,b,i
    param: Width, Height, Data, ImageNum
    Example: 800,600,<blob>,45

    Note -
    * The length of the data is in the first four bytes of the blob as per the OSC spec.
    * Data is JPEG encoded.
    * ImageNum is simply an always-incrementing number.

### FRemoteSessionInputChannel Channel API

    Address: /MessageHandler/OnTouchStarted
    tag: b
    param: Data
    The first four bytes of data is the size, then iittle endian f,f,i,i (X,Y,ID,Force)

    Address: /MessageHandler/OnTouchMoved
    tag: b
    param: Data
    The first four bytes of data is the size, then little endian f,f,i,i (X,Y,ID,Force)

    Address: /MessageHandler/OnTouchEnded
    tag: b
    param: Data
    The first four bytes of data is the size, then little endian f,f,i,i (X,Y,ID,Force)

## Tips

You can launch the RemoteSessionApp as either a host or client for testing. These commands will launch a host and a client in small windows with a visible log window

    Engine\Binaries\Win64\UE4Editor.exe RemoteSessionApp -game -windowed -resx=800 -log -remote.host
    Engine\Binaries\Win64\UE4Editor.exe RemoteSessionApp -game -windowed -resx=800 -log
    

Turn on verbose logging for BackChannel and Remote Session to get detailed information about what is happening. 
* Setting the mode to VeryVerbose for BackChannel will include info about packets being sent and received.
* Setting the mode to VeryVerbose for RemoteSession will log info about encoding/decoding times, amount of data buffered etc

Example

    Engine\Binaries\Win64\UE4Editor.exe RemoteSessionApp -game -windowed -resx=800 -log -remote.host -logcmds="logbackchannel verbose, logremotesession verbose"



