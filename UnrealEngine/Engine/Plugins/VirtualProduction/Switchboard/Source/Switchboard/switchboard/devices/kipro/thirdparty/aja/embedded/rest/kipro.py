#!/usr/bin/python
"""Ki-Pro Rest Client

Executes commands against a Ki-Pro family unit at the specified URL.
See the demo() for examples of how to use the client.
Supports Ki-Pro firmware revisions 3.0 and greater.

Module usage:

$ python
>>> from aja.embedded.rest.import kipro
>>> client = kipro.Client('http://YourKiPro')
>>> client.getFirmwareVersion()
'4.0.0.9'
>>> client.record()
>>> client.play()
>>> client.getWriteableParameters()


Commandline Usage: rest.py [options]

Options:
       -u | --url (URL of Ki-Pro unit)
       -h | --help (this message)

Examples:
       rest.py -u 90.0.6.6

Copyright (C) 2012 AJA Video Systems, Inc.
"""

__author__ = "Support <support@aja.com>"
__version__ = "rc1"
__date__ = "Tue Mar 13 14:17:51 PDT 2012"
__copyright__ = "Copyright (C) 2009 AJA Video Systems, Inc."
__license__ = "Proprietary"

from .base import *

class Client(BaseClient):
    __author__ = "Support <support@aja.com>"
    __version__ = "$Revision: 44 $"
    __date__ = "$Date: 2012-11-06 13:34:08 -0800 (Tue, 06 Nov 2012) $"
    __copyright__ = "Copyright (C) 2009 AJA Video Systems, Inc."
    __license__ = "Proprietary"
    __doc__ = """
This class understands the kipro REST api.
Quickstart:

$ python
>>> from aja.embedded.rest.kipro import *
>>> client = Client('http://YourKiPro')
>>> client.getFirmwareVersion()
'4.0.0.9'
>>> client.record()
>>> client.play()
>>> client.getWriteableParameters()
"""
    __success = 200

    def __init__(self, url, cacheRawParameters=True):
        try:
            BaseClient.__init__(self,
                                url=url,
                                supportedFirmwareVersion = "3.0.0",
                                versionParam = "eParamID_SWVersion",
                                cacheRawParameters=cacheRawParameters)
        except UnsupportedFirmwareVersionError:
            print("UnsupportedFirmwareVersionError")
            raise UnsupportedFirmwareVersionError
        except UnresponsiveTargetError as e:
            print("UnresponsiveTargetError in kipro Client constructor")
            print(e)
            raise UnresponsiveTargetError
        except Exception as e:
            print("Error in Client constructor")
            print(e)
            raise UnresponsiveTargetError

    def getTimecodeWithSynchronousCall(self):
        """
        Get the current timecode value. Most aplications will want to connect() then run wait_for_config_events in a while()
        loop in its own thread. See TimecodeListener for a convenient way to do that.
        WARNING: This call can take a long time. It will not return until timecode changes or is updated with the same
        value (like when stop is pressed).
        """
        connection = self.connect()
        timecode = None
        done = False
        while not done:
            events = self.waitForConfigEvents(connection)
            for event in events:
                if (event["param_id"] == "eParamID_DisplayTimecode"):
                    timecode = event["str_value"]
                    done = True
                    break
        return timecode

    def getTransporterState(self):
        """
        Convenience method for getting the current transporter state.
        Returns (state_value, human_readable_string)
        """
        (httpcode, response) = self.getRawParameter("eParamID_TransportState")
        result = ("","")
        if httpcode == self.__success:
            selected = self.getSelected(response)
            result = (int(selected['value']), selected['text'])
        return result


    def sendTransportCommandByDescription(self, term):
        """
        Compares term to the descriptions of valid transport commands sends that command if found.
        """
        settings = self.getValidSettingsForParameter("eParamID_TransportCommand")
        recordValue = 0
        for (value, description) in settings:
            if term == description:
                recordValue = value
                break
        (httpcode, response) = self.setParameter("eParamID_TransportCommand", recordValue)
        if httpcode == self.__success:
            selected = self.getSelected(response)

    def stop(self):
        """
        Stop a record or pause playback (send twice to stop playback)
        """
        self.sendTransportCommandByDescription('Stop Command')

    def record(self):
        """
        Convenience method to start the Transporter recording.
        """
        self.sendTransportCommandByDescription("Record Command")

    def play(self):
        """
        Convenience method to start playback.
        """
        self.sendTransportCommandByDescription("Play Command")

    def playReverse(self):
        """
        Convenience method to playback in reverse.
        """
        self.sendTransportCommandByDescription("Play Reverse Command")

    def nextClip(self):
        """
        Go to the next clip in the current playlist.
        """
        self.sendTransportCommandByDescription("Next Clip")
    
    def previousClip(self):
        """
        Go to the previous clip in the current playlist.
        """
        self.sendTransportCommandByDescription("Previous Clip")

    def fastForward(self):
        """
        Advance quickly while playing.
        """
        self.sendTransportCommandByDescription("Fast Forward")

    def fastReverse(self):
        """
        Playing in reverse quickly.
        """
        self.sendTransportCommandByDescription("Fast Reverse")

    def singleStepForward(self):
        """
        Move one frame forward.
        """
        self.sendTransportCommandByDescription("Single Step Forward")

    def singleStepReverse(self):
        """
        Move one frame backward.
        """
        self.sendTransportCommandByDescription("Single Step Reverse")

    def getPlaylists(self):
        """
        Get the current playlists.
        """
        code = 0
        response = ""
        f = None
        try:
            f = urllib.request.urlopen(self.url + '/clips?action=get_playlists', timeout=5)
            (code, response) = (f.getcode(), f.read())
            f.close()
        except:
            if f is not None:
                f.close()
            raise

        playlists = []
        if code == self.__success:
            top = self.asPython(response)
            entries = top["playlists"]
            for entry in entries:
                playlists.append(entry['playlist'])

        return playlists

    def getCurrentClipName(self):
        """ Convenience method to get the current clip. """
        result = ""
        (code, response) = self.getParameter('eParamID_CurrentClip')
        if code == self.__success:
            result = response

        return result

    def goToClip(self, clipName):
        """ This selects a clip as the current clip."""
        self.setParameter('eParamID_GoToClip', clipName)

    def cueToTimecode(self, timecode):
        """
        Cue to the supplied timecode in the current clip.
        Warning: Wraps if you use a timecode beyond the end of the clip.
        """
        self.setParameter('eParamID_CueToTimecode', timecode)
        self.sendTransportCommandByDescription("Cue")


def usage():
    print(__doc__)

def demo(url):
    """ Demonstrates how to use the client and grabs some useful information. """
    client = Client(url)

    print("Beginning demo. Pausing between actions for readability.")

    from time import sleep
    sleep (2)

    print(("Current firmware version is: ", client.getFirmwareVersion()))

    sleep(2)

    (value, description) = client.getTransporterState()
    print(("The current transporter state is: ", description))

    sleep(2)

    print("These are all the parameters which are visible to the API")

    sleep(2)

    readableParameters = client.getReadableParameters()
    for param in readableParameters:
        for param_id, description in list(param.items()):
            print(("%s: %s" % (param_id, description)))

    sleep(2)

    print("These are all of the parameters which can possibly be set.")

    sleep(2)

    writeableParams = client.getWriteableParameters()
    for param in writeableParams:
        for param_id, description in list(param.items()):
            print(("%s: %s" % (param_id, description)))

    sleep(2)

    print("Valid settings for the eParamID_TransportCommand parameter:")

    sleep(2)

    settings = client.getValidSettingsForParameter('eParamID_TransportCommand')
    for (value, description) in settings:
        print(('  a value of %s means "%s"' % (value, description)))


    # Uncomment the following to see examples of record, playback etc. in action.
    # #to start a record on the current media
    # client.record()

    # sleep(10)

    # #to stop record
    # client.stop()

    # sleep(10)

    # #play
    # client.play()

    # sleep(10)

    # #pause playback
    # client.stop()

    # sleep(10)
    # #stop playback (called it twice, this matches the button behavior)
    # client.stop()

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

class TimecodeListener(threading.Thread):
    __author__ = "Support <support@aja.com>"
    __version__ = "$Revision: 44 $"
    __date__ = "$Date: 2012-11-06 13:34:08 -0800 (Tue, 06 Nov 2012) $"
    __copyright__ = "Copyright (C) 2009 AJA Video Systems, Inc."
    __license__ = "Proprietary"
    __doc__ = """
    This listener creates a connection to a ki-pro unit and listens for timecode event updates.
    WARNING: Timecode events may not occur every frame. If you need a frame accurate timecode, consider using
    RS422 or setting timecode as a record trigger.

    quickstart:

    python$
      >>> from aja.embedded.rest.kipro import *
      >>> l = TimecodeListener('http://YourKiPro')
      >>> l.start()
      >>> print l.getTimecode()
    """

    def __init__(self, url):
        """
        Create a TimecodeListener.
        Use start() to start it listening.
        """
        super(TimecodeListener, self).__init__()
        self.url = url
        self.__timecode = ""
        self.__stop = False
        self.__lock = threading.RLock()

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
            print(("Failed to connect to", self.url))

    def stop(self):
        """ Tell the listener to stop listening and the thread to exit. """
        self.__stop = True

    def __setTimecode(self, timecode):
        """ Threadsafe. """
        with self.__lock:
            self.__timecode = timecode

    def getTimecode(self):
        """ Thread safe. """
        with self.__lock:
            return self.__timecode

if __name__ == "__main__":
    import getopt
    main(sys.argv[1:])
