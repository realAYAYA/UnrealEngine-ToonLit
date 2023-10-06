##################################################################################
## MIT License
##
## Copyright (c) 2019 Vicon Motion Systems Ltd
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to deal
## in the Software without restriction, including without limitation the rights
## to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
## copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included in all
## copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.
##################################################################################

from ....vicon_core_api.vicon_core_api import SchemaServices
from enum import Enum


class ETimecodeStandard(Enum):
    """Supported timecode standards.

    Enum Values:
        ETimecodeNone: Invalid value representing no timecode standard
        ETimecode24: 24 fps (FILM)
        ETimecode24Drift: 24 fps but actually running at (24/1.001)fps - hence drifting from wall clock (NTSC compatible FILM)
        ETimecode25: 25 fps (PAL)
        ETimecode30: 30 fps
        ETimecode30Drift: 30 fps but actually running at (30/1.001)fps hence drifting from wall clock (NTSC)
        ETimecode30Drop: 30 fps but actually running at (30/1.001)fps with periodic correction to wall clock (NTSC Drop) [still drifts very slightly!]
    """
    ETimecodeNone = -1
    ETimecode24 = 0
    ETimecode24Drift = 1
    ETimecode25 = 2
    ETimecode30 = 3
    ETimecode30Drift = 4
    ETimecode30Drop = 5




SchemaServices.register_json_schema(ETimecodeStandard,"""{"Type": "Enum32", "TypeName": "ETimecodeStandard", "EnumValues": [["TimecodeNone", -1], ["Timecode24", 0], ["Timecode24Drift",
                                                         1], ["Timecode25", 2], ["Timecode30", 3], ["Timecode30Drift", 4], ["Timecode30Drop", 5]]}""")

