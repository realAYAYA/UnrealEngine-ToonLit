#!/usr/bin/python
"""Kumo Rest Client

Executes commands against a Ki-Pro family unit at the specified URL.
See the demo() for examples of how to use the client.
Supports Kumo firmware revisions 0.0 and greater.

Module usage:

$ python
>>> from aja.embedded.rest.kumo import *
>>> client = Client('http://YourKumo')
>>> client.getFirmwareVersion()
'1.3.0.0'


Commandline Usage: kumo.py [options]

Options:
       -u | --url (URL of Kumo unit)
       -h | --help (this message)

Examples:
       kumo.py -u 90.0.6.6

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
This class understands the kumo REST api.
Quickstart:

$ python
>>> from aja.embedded.rest.kumo import *
>>> client = Client('http://YourKumo')
>>> client.getFirmwareVersion()
'1.0.0.0'
"""
    __success = 200

    def __init__(self, url, cacheRawParameters=True):
        try:
            BaseClient.__init__(self,
                                url=url,
                                supportedFirmwareVersion = "1.0.0.0",
                                versionParam = "eParamID_SWVersion",
                                cacheRawParameters=cacheRawParameters)
        except UnsupportedFirmwareVersionError:
            print("UnsupportedFirmwareVersionError")
            raise UnsupportedFirmwareVersionError
        except UnresponsiveTargetError:
            print("UnresponsiveTargetError in kipro Client constructor")
            raise UnresponsiveTargetError
        except:
            print("Error in Client constructor")
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
            f = urllib.request.urlopen(self.url + '/options?action=get&paramid=' + param_id + '&configid=0', timeout=5)
            result = (f.getcode(), f.read())
            f.close()

        except:
            if f is not None:
                f.close()
            print(('getRawParameter failed;  http code = ' + str(result)))
            raise

        return result

    def getParameter(self, param_id):
        """ Returns (the http error code, the value of the param_id or an empty string if none or not found). """
        try:
            (code, response) = self.getRawParameter(param_id)
            result = (code, "")
            if code == self.__success:
                response_value = self.getValue(response)
                #print('getParameter() : response_value = ' + response_value)
                result = (code, response_value)   
            else:
                print(('getParameter(' + param_id + ') failed;  http code = ' + str(code)))
        # Unnecessary exception fondling
        except UnresponsiveTargetError:
            raise UnresponsiveTargetError
        except:
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


    def run(self):
        c = Client(self.url)
        connection = c.connect()
        if connection:
            while not self.__stop:
                events = c.waitForConfigEvents(connection)
                for event in events:
                    if (event["param_id"] == "eParamID_DisplayTimecode"):
                        self.__setTimecode(event["str_value"])
                        break
            print("Listener stopping.")
        else:
            print("Failed to connect to", self.url)

    def stop(self):
        """ Tell the listener to stop listening and the thread to exit. """
        self.__stop = True


if __name__ == "__main__":
    import getopt
    main(sys.argv[1:])
