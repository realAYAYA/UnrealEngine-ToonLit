// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <mutex>
#include <set>
#include <string>

// Uncomment this to turn off the logging system.  The macros LogAssert,
// LogError, LogWarning, and LogInformation expand to nothing.  (Do this for
// optimal performance.)
//#define GTE_NO_LOGGER

namespace gte
{

class GTE_IMPEXP Logger
{
public:
    // Construction.  The Logger object is designed to exist only for a
    // single-line call.  A string is generated from the input parameters and
    // is used for reporting.
    Logger(char const* file, char const* function, int line,
        std::string const& message);

    // Notify current listeners about the logged information.
    void Assertion();
    void Error();
    void Warning();
    void Information();

    // Listeners subscribe to Logger to receive message strings.
    class Listener
    {
    public:
        enum
        {
            LISTEN_FOR_NOTHING      = 0x00000000,
            LISTEN_FOR_ASSERTION    = 0x00000001,
            LISTEN_FOR_ERROR        = 0x00000002,
            LISTEN_FOR_WARNING      = 0x00000004,
            LISTEN_FOR_INFORMATION  = 0x00000008,
            LISTEN_FOR_ALL          = 0xFFFFFFFF
        };

        // Construction and destruction.
        virtual ~Listener();
        Listener(int flags = LISTEN_FOR_NOTHING);

        // What the listener wants to hear.
        int GetFlags() const;

        // Handlers for the messages received from the logger.
        void Assertion(std::string const& message);
        void Error(std::string const& message);
        void Warning(std::string const& message);
        void Information(std::string const& message);

    private:
        virtual void Report(std::string const& message);

        int mFlags;
    };

    static void Subscribe(Listener* listener);
    static void Unsubscribe(Listener* listener);

private:
    std::string mMessage;

    static std::mutex msMutex;
    static std::set<Listener*> msListeners;
};

}


#if !defined(GTE_NO_LOGGER)

#define LogAssert(condition, message) \
    if (!(condition)) \
    { \
        gte::Logger(__FILE__, __FUNCTION__, __LINE__, message).Assertion(); \
    }

#define LogError(message) \
    gte::Logger(__FILE__, __FUNCTION__, __LINE__, message).Error()

#define LogWarning(message) \
    gte::Logger(__FILE__, __FUNCTION__, __LINE__, message).Warning()

#define LogInformation(message) \
    gte::Logger(__FILE__, __FUNCTION__, __LINE__, message).Information()

#else

// No logging of assertions, warnings, errors, or information.
#define LogAssert(condition, message)
#define LogError(message)
#define LogWarning(message)
#define LogInformation(message)

#endif

