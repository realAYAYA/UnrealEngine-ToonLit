#!/usr/bin/python
"""FS2 Rest Client

Executes commands against an FS2 unit at the specified URL.
See the demo() for examples of how to use the client.
Supports FS2 firmware revisions 0.0 and greater.

Module usage:

$ python
>>> from aja.embedded.rest.fs2 import *
>>> client = Client('http://YourFS2')
>>> client.getFirmwareVersion()
'4.0.0.9'


Commandline Usage: fs2.py [options]

Options:
       -u | --url (URL of FS2 unit)
       -h | --help (this message)

Examples:
       fs2.py -u 90.0.6.6

Copyright (C) 2012 AJA Video Systems, Inc.
"""

__author__ = "Support <support@aja.com>"
__version__ = "rc1"
__date__ = "Tue Mar 13 14:17:51 PDT 2012"
__copyright__ = "Copyright (C) 2009 AJA Video Systems, Inc."
__license__ = "Proprietary"

import sys
import threading
import urllib.request, urllib.parse, urllib.error, urllib.request, urllib.error, urllib.parse
import json
import re # "now you have two problems" -- jwz

from .base import *

class Client(BaseClient):
    __author__ = "Support <support@aja.com>"
    __version__ = "$Revision: 16 $"
    __date__ = "$Date: 2012-03-16 12:56:46 -0700 (Fri, 16 Mar 2012) $"
    __copyright__ = "Copyright (C) 2009 AJA Video Systems, Inc."
    __license__ = "Proprietary"
    __doc__ = """
This class understands the FS2 REST api.
Quickstart:

$ python
>>> from aja.embedded.rest.fs2 import *
>>> client = Client('http://YourFS2')
>>> client.getFirmwareVersion()
'1.1.1.1'
"""
    __success = 200

    def __init__(self, url, cacheRawParameters=True):
        try:
            BaseClient.__init__(self,
                                url=url,
                                supportedFirmwareVersion = "1.0.0.0",
                                versionParam = "eParamID_SoftwareVersion",
                                cacheRawParameters=cacheRawParameters)
        except UnsupportedFirmwareVersionError:
            print("UnsupportedFirmwareVersionError")
            raise UnsupportedFirmwareVersionError
        except UnresponsiveTargetError:
            print("UnresponsiveTargetError in FS2 Client constructor")
            raise UnresponsiveTargetError
        except Exception as e:
            print("Error in Client constructor")
            print(e)
            raise UnresponsiveTargetError

    def setParameter(self, param, value):
        """ Set a single parameter value. """
        result = (None, "")
        f = None
        params = urllib.parse.urlencode({'paramName':param, 'newValue' : value})
        try:
            f = urllib.request.urlopen(self.url + '/config', params, timeout=5)
            result = (f.getcode(), f.read())
            f.close()
        except:
            if f is not None:
                f.close()
            raise UnresponsiveTargetError

        return result

    def getRawParameter(self, param_id):
        """ Returns (http error code, a json string containing all the information about the param including its current value) """
        result = (None, "")
        f = None
        try:
            f = urllib.request.urlopen(self.url + '/config?action=get&paramid=' + param_id, timeout=5)
            result = (f.getcode(), f.read())
            f.close()

        except:
            #A Target device did not respond... perhaps it's powered off, or param_id is invalid?
            if f is not None:
                f.close()
            #print("Exception in getRawParameter(" + param_id + ")")  # Suppress the noise... this exception is expected when calling is_alive() from wait_for_reboot()
            raise UnresponsiveTargetError

        return result

    def getValue(self, response):
        """
        ?
        """
        result = None
        options = self.asPython(response)
        result = options["value"]
        
        return result

    def getParameter(self, param_id):
        """ Returns (the http error code, the value of the param_id or an empty string if none or not found). """
        try:
            (code, response) = self.getRawParameter(param_id)
            result = (code, "")
            if code == self.__success:
                #response_value = (self.getSelected(response))['text']   # This is how KiPro does it... doesn't work for FS2
                response_value = self.getValue(response) # getValue() has disappeared
                result = (code, response_value)   
            else:
                print(('getParameter(' + param_id + ') failed;  http code = ' + str(code)))

        # Unnecessary exception fondling
        except UnresponsiveTargetError:
            raise UnresponsiveTargetError
        except:
            print("exception in getParameter()")
            raise
            
        return result


def usage():
    print(__doc__)

def demo(url):
    """ Demonstrates how to use the client and grabs some useful information. """
    client = Client(url)

    print("Beginning demo. Pausing between actions for readability.")

    from time import sleep
    sleep (2)

    print("Current firmware version is: ", client.getFirmwareVersion())

    sleep(2)

    print()
    print("These are all the parameters which are visible to the API")
    print()

    sleep(2)

    readableParameters = client.getReadableParameters()
    for param in readableParameters:
        for param_id, description in list(param.items()):
            print("%s: %s" % (param_id, description))

    sleep(2)

    print()
    print("These are all of the parameters which can possibly be set.")
    print()

    sleep(2)

    writeableParams = client.getWriteableParameters()
    for param in writeableParams:
        for param_id, description in list(param.items()):
            print("%s: %s" % (param_id, description))


def main(argv):
    try:
        opts, args = getopt.getopt(argv, "u:h", ["url=", "help"])
    except getopt.GetoptError:
        usage()
        sys.exit(2)

    url = None
    # defaults
    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage()
            sys.exit()
        elif opt in ("-u", "--url"):
            url = arg

        if url is None:
            usage()
            sys.exit()
        else:
            demo(url)


if __name__ == "__main__":
    import getopt
    main(sys.argv[1:])
