#-******************************************************************************
# Copyright (c) 2012,
#  Sony Pictures Imageworks Inc. and
#  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# *       Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# *       Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
# *       Neither the name of Sony Pictures Imageworks, nor
# Industrial Light & Magic, nor the names of their contributors may be used
# to endorse or promote products derived from this software without specific
# prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#-******************************************************************************

import unittest
from imath import *
from alembic.AbcCoreAbstract import *
from alembic.Abc import *
from alembic.AbcGeom import *
from nurbsData import *


class NurbsSurfaceTest(unittest.TestCase):
    def testNurbsExport1(self):
        """write an oarchive with a nurb in it"""

        myNurbs = ONuPatch(
            OArchive( 'nurbs1.abc' ).getTop(), 'nurbs_surface' )

        myNurbsSchema = myNurbs.getSchema()

        nurbsSamp = ONuPatchSchemaSample (
            P, nu, nv, uOrder, vOrder, uKnot, vKnot )

        nurbsSamp.setTrimCurve( trim_nLoops, trim_nCurves, trim_n, trim_order,
            trim_knot, trim_min, trim_max, trim_u, trim_v, trim_w )

        myNurbsSchema.set( nurbsSamp )

    def testNurbsExport2(self):
        """same as example 1 but without the trim curves"""

        myNurbs = ONuPatch(
            OArchive( 'nurbs2.abc' ).getTop(), 'nurbs_surface_noTrim' )

        myNurbsSchema = myNurbs.getSchema()

        nurbsSamp = ONuPatchSchemaSample (
            P, nu, nv, uOrder, vOrder, uKnot, vKnot )

        myNurbsSchema.set( nurbsSamp )

    def testNurbsExport3(self):
        """same as example 1 but without the trim curves, with position weights"""

        myNurbs = ONuPatch( OArchive( 'nurbs3.abc' ).getTop(),
                        'nurbs_surface_withW' )

        myNurbsSchema = myNurbs.getSchema()

        nurbsSamp = ONuPatchSchemaSample (
            P, nu, nv, uOrder, vOrder, uKnot, vKnot )

        nurbsSamp.setPositionWeights( Pw )

        myNurbsSchema.set( nurbsSamp )

    def testNurbsImport1(self):
        """read an iarchive with a nurb in it"""

        myNurbs = INuPatch(
            IArchive( 'nurbs1.abc' ).getTop(), 'nurbs_surface' )
        nurbsSchema = myNurbs.getSchema()

        nurbsSamp = nurbsSchema.getValue()

        self.assertEqual(nurbsSamp.getSelfBounds().min(), V3d( 0.0, 0.0, -3.0 ))
        self.assertEqual(nurbsSamp.getSelfBounds().max(), V3d( 3.0, 3.0,  3.0 ))

        self.assertEqual(nurbsSamp.getTrimNumLoops(), 1)
        self.assertEqual(len( nurbsSamp.getTrimOrders() ), 1)
        self.assertTrue(nurbsSamp.hasTrimCurve())
        self.assertTrue(nurbsSchema.isConstant())

    def testNurbsImport2(self):
        """same as example 1 but without the trim curves"""

        myNurbs = INuPatch( IArchive( 'nurbs2.abc' ).getTop(),
                        'nurbs_surface_noTrim' )
        nurbsSchema = myNurbs.getSchema()

        nurbsSamp = nurbsSchema.getValue()

        self.assertEqual(nurbsSamp.getSelfBounds().min(), V3d(0.0, 0.0, -3.0))
        self.assertEqual(nurbsSamp.getSelfBounds().max(), V3d(3.0, 3.0,  3.0))

        self.assertEqual(nurbsSamp.getTrimNumLoops(), 0)
        self.assertFalse(nurbsSamp.hasTrimCurve())
        self.assertFalse(nurbsSamp.getPositionWeights())
        self.assertTrue(nurbsSchema.isConstant())

    def testNurbsImport3(self):
        """same as example 1 but without the trim curves, with position weights"""

        myNurbs = INuPatch( IArchive( 'nurbs3.abc' ).getTop(),
                        'nurbs_surface_withW' )
        nurbsSchema = myNurbs.getSchema()

        nurbsSamp = nurbsSchema.getValue()

        self.assertEqual(nurbsSamp.getSelfBounds().min(), V3d( 0.0, 0.0, -3.0 ))
        self.assertEqual(nurbsSamp.getSelfBounds().max(), V3d( 3.0, 3.0,  3.0 ))

        self.assertEqual(nurbsSamp.getTrimNumLoops(), 0)
        self.assertFalse(nurbsSamp.hasTrimCurve())
        self.assertEqual(len( nurbsSamp.getPositionWeights() ), len( P ))
        self.assertTrue(nurbsSchema.isConstant())

