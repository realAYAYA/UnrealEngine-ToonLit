// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;
using System.IO;
using System.Drawing;
using System.Collections.Generic;
using System.Globalization;
using SolidWorks.Interop.sldworks;

namespace DatasmithSolidworks
{
	[ComVisible(false)]
	public class FMaterial
	{
		private static List<Tuple<string, string>> SpecialFinishes = new List<Tuple<string, string>>();
		private static List<Tuple<string, string>> SpecialDiffuses = new List<Tuple<string, string>>();
		private static Dictionary<string, EMaterialType> MaterialTypes = null;

		public enum EMaterialType
		{
			TYPE_UNKNOWN,
			TYPE_FABRIC,
			TYPE_GLASS,
			TYPE_LIGHTS,
			TYPE_METAL,
			TYPE_MISC,
			TYPE_ORGANIC,
			TYPE_PAINTED,
			TYPE_PLASTIC,
			TYPE_RUBBER,
			TYPE_STONE,
			TYPE_METALLICPAINT,
			TYPE_LIGHTWEIGHT
		}

		public enum EMappingType
		{
			TYPE_UNSPECIFIED = -1,
			TYPE_SURFACE = 0,
			TYPE_PROJECTION = 1,
			TYPE_SPHERICAL = 2,
			TYPE_CYLINDRICAL = 3,
			TYPE_AUTOMATIC = 4
		}

		public static void InitializeMaterialTypes()
		{
			if (MaterialTypes == null)
			{
				MaterialTypes = new Dictionary<string, EMaterialType>();

				MaterialTypes.Add("carpetcolor1", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("carpetcolor2", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("carpetcolor3", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("carpetcolor4", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("carpetcolor5", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("clothburgundycotton", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("clothburlap", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("clothcanvas", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("clothbeigecotton", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("clothbluecotton", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("cotton white 2d", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("clothgreycotton", EMaterialType.TYPE_FABRIC);
				MaterialTypes.Add("blueglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("brownglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("clearglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("clearglasspv", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("greenglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("mirror", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("reflectiveblueglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("reflectiveclearglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("reflectivegreenglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("frostedglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("glassfibre", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("sandblastedglass", EMaterialType.TYPE_GLASS);
				MaterialTypes.Add("area_light", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("blue_backlit_lcd", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("green_backlit_lcd", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("amber_led", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("blue_led", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("green_led", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("red_led", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("white_led", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("yellow_led", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("blue_neon_tube", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("green_neon_tube", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("red_neon_tube", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("white_neon_tube", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("yellow_neon_tube", EMaterialType.TYPE_LIGHTS);
				MaterialTypes.Add("aluminumtreadplate", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("blueanodizedaluminum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedaluminum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedaluminum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castaluminum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattealuminum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedaluminum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedaluminum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishaluminum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedbrass", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedbrass", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castbrass", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattebrass", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedbrass", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedbrass", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishbrass", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedbronze", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedbronze", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castbronze", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("manganesebronze", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattebronze", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedbronze", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedbronze", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishbronze", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedchromium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedchrome", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("chromiumplatecast", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("chromiumplate", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattechrome", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedchrome", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishchrome", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedcopper", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedcopper", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castcopper", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattecopper", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedcopper", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedcopper", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishcopper", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("wroughtcopper", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedgalvanized", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("plaingalvanized", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("shinygalvanized", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattegold", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedgold", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishgold", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castiron", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("matteiron", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastediron", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("wroughtiron", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedlead", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castlead", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattelead", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedlead", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedmagnesium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedmagnesium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castmagnesium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattemagnesium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedmagnesium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satin finish magnesium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushednickel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishednickel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castnickel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattenickel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishednickel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastednickel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishnickel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("matteplatinum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedplatinum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishplatinum", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattesilver", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedsilver", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishsilver", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedsteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedsteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedcarbonsteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castcarbonsteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("caststainlesssteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("chainlinksteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("machinedsteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattesteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedsteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedsteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishstainlesssteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("stainlesssteelknurled", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("stainlesssteeltreadplate", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("wroughtstainlesssteel", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedtitanium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedtitanium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("casttitanium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattetitanium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedtitanium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishtitanium", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedtungsten", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("casttungsten", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattetungsten", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedtungsten", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishtungsten", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("brushedzinc", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("burnishedzinc", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("castzinc", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("mattezinc", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("polishedzinc", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("sandblastedzinc", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("satinfinishzinc", EMaterialType.TYPE_METAL);
				MaterialTypes.Add("screwthread", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("cartoonsketch", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("blue_slate_floor", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("checker_floor_bright", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("scene_factory_floor", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("floorgrime", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("floorwhite", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("imperfect-floor", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("wallgrime", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("wallwhite", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("transparentfloor", EMaterialType.TYPE_MISC);
				MaterialTypes.Add("grass", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("leather", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("sand", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("skin", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("sponge", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("clearsky", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("heavyclouds", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("lightclouds", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("waterheavyripple", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("waterslightripple", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("waterstill", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedash", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishash", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedash", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedbeech", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishbeech", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedbeech", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedbirch", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishbirch", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedbirch", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedcherry", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishcherry", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedcherry", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("hardwoodfloor3", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("hardwoodfloor2", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("hardwoodfloor", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("laminatefloor", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("laminatefloor2", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("laminatefloor3", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedmahogany", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishmahogany", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedmahogany", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedmaple", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishmaple", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedmaple", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedoak", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishoak", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedoak", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("orientedstrandboard", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedpine", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishpine", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedpine", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedrosewood", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishrosewood", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedrosewood", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedsatinwood", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedsatinwood", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishsatinwood ", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedspruce", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedspruce", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishspruce", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedteak", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishteak", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedteak", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("polishedwalnut", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("satinfinishwalnut", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("unfinishedwalnut", EMaterialType.TYPE_ORGANIC);
				MaterialTypes.Add("blackcarpaint", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("candyappleredcarpainthq", EMaterialType.TYPE_METALLICPAINT);
				MaterialTypes.Add("glossbluecarpainthq", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("glossredcarpainthq", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("metalliccoolgreycarpainthq", EMaterialType.TYPE_METALLICPAINT);
				MaterialTypes.Add("metallicgoldcarpainthq", EMaterialType.TYPE_METALLICPAINT);
				MaterialTypes.Add("metallicwarmgreycarpainthq", EMaterialType.TYPE_METALLICPAINT);
				MaterialTypes.Add("sienacarpainthq", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("steelgreycarpainthq", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("whitecarpaint", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("aluminumpowdercoat", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("darkpowdercoat", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("blackspraypaint", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("redspraypaint", EMaterialType.TYPE_PAINTED);
				MaterialTypes.Add("frostedplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("polycarbonateplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("polypropyleneplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("translucentplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("carbonfiberaramidfabric", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("carbonfiberdesignfabric", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("carbonfiberdyneemaplain", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("carbonfiberepoxy", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("carbonfiberinlayunidirectional", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("largesparkerosionplasticblue", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("sparkerosionplasticblue", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("beigehighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("blackhighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("bluehighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("creamhighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("darkgreyhighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("greenhighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("lightgreyhighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("redhighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("whitehighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("yellowhighglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("beigelowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("blacklowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("bluelowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("creamlowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("darkgreylowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("greenlowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("lightgreylowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("redlowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("whitelowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("yellowlowglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("beigemediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("blackmediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("bluemediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("creammediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("darkgreymediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("greenmediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("lightgreymediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("redmediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("whitemediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("yellowmediumglossplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("circularmeshplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("diamondmeshplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("bluedimpledplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("blueknurledplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("bluetreadplateplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("beigesatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("blacksatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("bluesatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("creamsatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("darkgreysatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("greensatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("lightgreysatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("redsatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("whitesatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("yellowsatinfinishplastic", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11000", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11010", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11020", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11030", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11040", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11050", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11060", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11070", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11080", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11090", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11100", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11110", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11120", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11130", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11140", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11150", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11155", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11200", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11205", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11215", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11230", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11235", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11240", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11245", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("plasticmt11250", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("PlasticPolystyrene", EMaterialType.TYPE_PLASTIC);
				MaterialTypes.Add("glossyrubber", EMaterialType.TYPE_RUBBER);
				MaterialTypes.Add("matterubber", EMaterialType.TYPE_RUBBER);
				MaterialTypes.Add("perforatedrubber", EMaterialType.TYPE_RUBBER);
				MaterialTypes.Add("texturedrubber", EMaterialType.TYPE_RUBBER);
				MaterialTypes.Add("tiretreadrubber", EMaterialType.TYPE_RUBBER);
				MaterialTypes.Add("redceramictile", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("cobblestone", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("cobblestone1", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("cobblestone2", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("cobblestone3", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("Scene_Cobblestone", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("floortile1", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("floortile2", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("floortile3", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("floortile4", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("floortile5", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("floortile6", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("floortile7", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("granite", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("limestone", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("bluemarble", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("darkmarble", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("beigemarble", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("greenmarble", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("pinkmarble", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("redsandstone", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("sandstone", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("tansandstone", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("slate", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("fakebrick", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("firebrick", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("flemishbrick", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("oldenglishbrick2", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("oldenglishbrick", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("weatheredbrick", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("pavingasphalt", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("pavingdarkconcrete", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("pavinglightconcrete", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("pavingstone", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("pavingredconcrete", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("pavingwetconcrete", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("bonechina", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("ceramic", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("earthenware", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("porcelain", EMaterialType.TYPE_STONE);
				MaterialTypes.Add("stoneware", EMaterialType.TYPE_STONE);
			}
		}

		public static EMaterialType GetMaterialType(string InShaderName)
		{
			if (InShaderName == null)
			{
				return EMaterialType.TYPE_LIGHTWEIGHT;
			}
			
			EMaterialType Type;
			InShaderName = InShaderName.ToLower();
			if (!MaterialTypes.TryGetValue(InShaderName, out Type))
			{
				if (InShaderName.IndexOf("plastic") >= 0) // legacy plastics
				{
					Type = EMaterialType.TYPE_PLASTIC;
				}
				else
				{
					Type = EMaterialType.TYPE_UNKNOWN;
				}
			}
			return Type;
		}

		private static bool IsBlack(Color InColor)
		{
			int C = InColor.ToArgb();
			int CMA = C & 0x00ffffff;
			return CMA == 0;
		}

		private static bool IsBlack(int InColor)
		{
			int CMA = InColor & 0x00ffffff;
			return CMA == 0;
		}

		public IRenderMaterial Source { get; private set; }
		public IAppearanceSetting Appearance { get; private set; }

		public int ID { get; private set; } = -1;

		public string Name;
		public string ShaderName;
		public string FileName;
		public string BumpTextureFileName;
		public string Texture;

		// material data
		public long Type = 0;
		public long BumpMap = 0;
		public double BumpAmplitude = 0.0;
		public double Emission = 0.0;
		public double Glossy = 0.0;
		public double IOR = 0.0;
		public long ColorForm = 0;
		public Color PrimaryColor = Color.FromArgb(0xff, 0x7f, 0x7f, 0x7f);
		public Color SecondaryColor = Color.FromArgb(0xff, 0, 0, 0);
		public Color TertiaryColor = Color.FromArgb(0xff, 0, 0, 0);
		public double Reflectivity = 0.0;
		public double RotationAngle = 0.0;
		public double SpecularSpread = 0.0;
		public double Specular = 0.0;
		public double Roughness = 1.0;
		public double MetallicRoughness = 1.0;
		public long SpecularColor = 0;
		public double Transparency = 0.0;
		public double Translucency = 0.0;
		public double Width = 0.0;
		public double Height = 0.0;
		public double XPos = 0.0;
		public double YPos = 0.0;
		public double Rotation = 0.0;
		public FVec3 CenterPoint;
		public FVec3 UDirection;
		public FVec3 VDirection;
		public double Direction1RotationAngle = 0.0;
		public double Direction2RotationAngle = 0.0;
		public bool MirrorVertical = false;
		public bool MirrorHorizontal = false;
		public EMappingType UVMappingType = EMappingType.TYPE_UNSPECIFIED;
		public long ProjectionReference = 0;
		public double Diffuse = 1.0;
		public bool BlurryReflections = false;

		public FMaterial()
		{
		}

		public FMaterial(RenderMaterial InRenderMat, IModelDocExtension InExt)
		{
			if (InRenderMat != null)
			{
				Source = InRenderMat;

				Type = Source.IlluminationShaderType;
				FileName = Source.FileName;

				ID = MaterialUtils.GetMaterialID(InRenderMat);

				Name = Path.GetFileNameWithoutExtension(FileName) + "<" + ID + ">";

				BumpMap = Source.BumpMap;
				BumpAmplitude = Source.BumpAmplitude;
				BumpTextureFileName = Source.BumpTextureFilename;

				Emission = Source.Emission;

				// fix emission inferred from ambient factor
				if (Emission > 0.0)
				{
					if ((Name.IndexOf("LED") == -1) &&
						(Name.IndexOf("Light") == -1) &&
						(Name.IndexOf("LCD") == -1) &&
						(Name.IndexOf("Neon") == -1))
					{
						Emission = 0.0;
					}
				}

				ColorForm = Source.ColorForm;

				// swRenderMaterialColorFormsColor_Undefined
				// swRenderMaterialColorFormsImage
				// swRenderMaterialColorFormsOne_Color
				// swRenderMaterialColorFormsTwo_Colors
				// swRenderMaterialColorFormsThree_Colors

				Glossy = Source.Glossy;
				Roughness = Source.Roughness;
				MetallicRoughness = Source.MetallicRoughness;
				Reflectivity = Source.Reflectivity;
				Specular = Source.Specular;
				SpecularColor = Source.SpecularColor;
				RotationAngle = Source.RotationAngle;

				PrimaryColor = MaterialUtils.ConvertColor(Source.PrimaryColor);
				SecondaryColor = MaterialUtils.ConvertColor(Source.SecondaryColor);
				TertiaryColor = MaterialUtils.ConvertColor(Source.TertiaryColor);

				double CX, CY, CZ;
				Source.GetCenterPoint2(out CX, out CY, out CZ);
				CenterPoint = new FVec3((float)CX, (float)CY, (float)CZ);

				double UDirx, YDiry, UDirz;
				Source.GetUDirection2(out UDirx, out YDiry, out UDirz);
				UDirection = new FVec3((float)UDirx, (float)YDiry, (float)UDirz);

				double VDirx, VDiry, VDirz;
				Source.GetVDirection2(out VDirx, out VDiry, out VDirz);
				VDirection = new FVec3((float)VDirx, (float)VDiry, (float)VDirz);

				Direction1RotationAngle = Source.Direction1RotationAngle;
				Direction2RotationAngle = Source.Direction2RotationAngle;

				Transparency = Source.Transparency;
				Translucency = Source.Translucency;
				IOR = Source.IndexOfRefraction;

				Texture = Source.TextureFilename;

				Width = Source.Width;
				Height = Source.Height;
				XPos = Source.XPosition;
				YPos = Source.YPosition;
				Rotation = Source.RotationAngle;
				MirrorVertical = Source.HeightMirror;
				MirrorHorizontal = Source.WidthMirror;

				if (Source.MappingType == 0)
				{
					UVMappingType = EMappingType.TYPE_SURFACE;
				}
				else if (Source.MappingType == 1)
				{
					UVMappingType = EMappingType.TYPE_PROJECTION;
				}
				else if (Source.MappingType == 2)
				{
					UVMappingType = EMappingType.TYPE_SPHERICAL;
				}
				else if (Source.MappingType == 3)
				{
					UVMappingType = EMappingType.TYPE_CYLINDRICAL;
				}
				else if (Source.MappingType == 4)
				{
					UVMappingType = EMappingType.TYPE_AUTOMATIC;
				}

				ProjectionReference = Source.ProjectionReference;

				bool bIsBlack = IsBlack(PrimaryColor);

				if (!string.IsNullOrEmpty(FileName))
				{
					if (!File.Exists(FileName)) // if the file is moved from its original location, the saved path might still refer to the old one
					{
						string DocPath = Path.GetDirectoryName(InExt.Document.GetPathName());
						string Fname = Path.GetFileName(FileName);
						FileName = Path.Combine(DocPath, Fname);
					}

					if (File.Exists(FileName))
					{
						string FileData = File.ReadAllText(FileName);
						if (!string.IsNullOrEmpty(FileData))
						{
							var DataSize = FileData.Length;
							ShaderName = FindP2MProperty(FileData, "\"sw_shader\"");

							if (bIsBlack)
							{
								var Col1 = FindP2MProperty(FileData, "\"col1\"");
								if (!string.IsNullOrEmpty(Col1))
								{
									string[] Coords = Col1.Split(',');
									if (Coords.Length == 3)
									{
										byte R = (byte)(float.Parse(Coords[0]) * 255f);
										byte G = (byte)(float.Parse(Coords[0]) * 255f);
										byte B = (byte)(float.Parse(Coords[0]) * 255f);
										if (R != 0 || G != 0 || B != 0)
										{
											PrimaryColor = Color.FromArgb(0xff, R, G, B);
										}
									}
								}
							}

							// could be in a parameter
							string TexturePath = FindP2MPropertyString(FileData, "\"bumpTexture\"");
							if (!string.IsNullOrEmpty(TexturePath))
							{
								BumpTextureFileName = Path.Combine(new string[] { MaterialUtils.GetDataPath(), TexturePath.Replace('/', '\\') });
							}
							else
							{
								int Pos = FileData.IndexOf("color texture \"bump_file_texture\" ");
								if (Pos != -1)
								{
									int Pos1 = FileData.IndexOf('"', Pos);
									if (Pos1 != -1)
									{
										Pos1 = FileData.IndexOf('"', Pos1 + 1);
										if (Pos1 != -1)
										{
											Pos1 = FileData.IndexOf('"', Pos1 + 1);
											if (Pos1 != -1)
											{
												int Pos2 = FileData.IndexOf('"', Pos1 + 1);
												if (Pos2 != -1)
												{
													BumpTextureFileName = Path.Combine(new string[] { MaterialUtils.GetDataPath(), FileData.Substring(Pos1 + 1, Pos2 - Pos1 - 1).Replace('/', '\\') });
												}
											}
										}
									}
								}
							}
						}
					}
				}

				//
				if (!string.IsNullOrEmpty(Texture))
				{
					if (SpecialDiffuses.Count == 0)
					{
						SpecialDiffuses.Add(new Tuple<string, string>("ash", "organic\\wood\\ash\\polished ash.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("beech", "organic\\wood\\beech\\polished beech.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("birch", "organic\\wood\\birch\\polished birch.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("cherry", "organic\\wood\\cherry\\polished cherry.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("maple", "organic\\wood\\maple\\polished maple.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("oak", "organic\\wood\\oak\\polished oak.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("pine", "organic\\wood\\pine\\polished pine.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("rosewood", "organic\\wood\\rosewood\\polished rosewood.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("satinwood", "organic\\wood\\satinwood\\satinwood.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("spruce", "organic\\wood\\spruce\\spruce.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("teak", "organic\\wood\\teak\\polished teak.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("blue marble", "stone\\architectural\\marble\\marble blue.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("blue rough shiny marble", "stone\\architectural\\marble\\marble blue.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("blue shiny marble", "stone\\architectural\\marble\\marble blue.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("blue vein marble", "stone\\architectural\\marble\\marble blue vein.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("green marble", "stone\\architectural\\marble\\marble green.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("green vein marble", "stone\\architectural\\marble\\marble green vein.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("pink marble", "stone\\architectural\\marble\\pink marble.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("pink vein marble", "stone\\architectural\\marble\\marble pink vein.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("sandstone", "stone\\architectural\\sandstone\\sandstone.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("slate", "stone\\architectural\\slate\\slate.jpg"));
						SpecialDiffuses.Add(new Tuple<string, string>("slate", "stone\\brick\\fire brick.jpg"));
					}
					foreach (var KV in SpecialDiffuses)
					{
						CompareInfo SampleCInfo = CultureInfo.InvariantCulture.CompareInfo;
						if (SampleCInfo.IndexOf(Name, KV.Item1, CompareOptions.IgnoreCase) >= 0)
						{
							Texture = Path.Combine(new string[] { MaterialUtils.GetDataPath(), "Images", "textures", KV.Item2 });
							break;
						}
					}
				}

				if (!string.IsNullOrEmpty(Texture))
				{
					PrimaryColor = Color.FromArgb(0xff, 0xff, 0xff, 0xff);
				}
			
				if (!string.IsNullOrEmpty(BumpTextureFileName))
				{
					if (SpecialFinishes.Count == 0)
					{
						SpecialFinishes.Add(new Tuple<string, string>("burnished", "burnished_n.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("burlap", "burlap_n.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("brushed", "_brushed.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("scratch", "_scratch_n.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("knurled", "knurled.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("spruce", "spruce_n.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("frosted", "plasticmt11040_normalmap.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("sandblasted", "sandblasted.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("satin", "satinwood_n.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("powdercoat", "powdercoat_n.dds"));
						SpecialFinishes.Add(new Tuple<string, string>("polypropylene", "..\\..\\textures\\plastic\\bumpy\\plasticpolypropylene_bump.jpg"));
					}
					foreach (var KV in SpecialFinishes)
					{
						CompareInfo SampleCInfo = CultureInfo.InvariantCulture.CompareInfo;
						if (SampleCInfo.IndexOf(Name, KV.Item1, CompareOptions.IgnoreCase) >= 0)
						{
							BumpTextureFileName = Path.Combine(new string[] { MaterialUtils.GetDataPath(), "Images", "shaders", "surfacefinish", KV.Item2 });
							break;
						}
					}
				}

				// Try to get the legacy appearance to fish up hidden properties such as BlurryReflections and Specular Spread
				//
				if (InRenderMat.GetEntitiesCount() > 0)
				{
					object Entity = InRenderMat.GetEntities()[0];
					DisplayStateSetting Settings = null;

					if (Entity is ModelDoc2)
					{
						ModelDoc2 Doc = Entity as ModelDoc2;
						InExt = Doc.Extension;
						Component2 Root = Doc.ConfigurationManager.ActiveConfiguration.GetRootComponent3(false);

						if (Root != null)
						{
							Settings = InExt.GetDisplayStateSetting(1); // 1 = swThisDisplayState
							Settings.Entities = new Component2[1] { Root };
						}
					}
					else
					{
						Settings = InExt.GetDisplayStateSetting(1); // 1 = swThisDisplayState
						if (Entity is Face2)
						{
							Settings.Entities = new Face2[1] { Entity as Face2 };
						}
						else if (Entity is Feature)
						{
							Settings.Entities = new Feature[1] { Entity as Feature };
						}
						else if (Entity is Body2)
						{
							Settings.Entities = new Body2[1] { Entity as Body2 };
						}
						else if (Entity is PartDoc)
						{
							Settings.Entities = new PartDoc[1] { Entity as PartDoc };
						}
						else if (Entity is Component2)
						{
							Settings.Entities = new Component2[1] { Entity as Component2 };
						}
					}

					if (Settings != null && Settings.Entities != null)
					{
						object[] Appearances = null;
						try
						{
							Appearances = InExt.DisplayStateSpecMaterialPropertyValues[Settings] as object[];
						}
						catch
						{
						}
						if (Appearances != null && Appearances.Length > 0)
						{
							SetAppearance(Appearances[0] as IAppearanceSetting);
						}
					}
				}
			}
		}

		public void SetAppearance(IAppearanceSetting InSetting)
		{
			if (InSetting != null)
			{
				Appearance = InSetting;

				double ADiffuse, AEmission, AReflectivity, ASpecular, ASpecularSpread, ATransparency;
				int LSpecularColor, LPrimaryColor;

				BlurryReflections = Appearance.BlurryReflection;

				LPrimaryColor = Appearance.Color;
				ADiffuse = Appearance.Diffuse;
				AEmission = Appearance.Luminous;
				AReflectivity = Appearance.Reflection;
				ASpecular = Appearance.Specular;
				LSpecularColor = Appearance.SpecularColor;
				ASpecularSpread = Appearance.SpecularSpread;
				ATransparency = Appearance.Transparent;

				Diffuse = ADiffuse;
				Emission = AEmission;
				Reflectivity = AReflectivity;
				Specular = ASpecular;
				SpecularSpread = ASpecularSpread;
				Transparency = ATransparency;

				if (IsBlack(PrimaryColor) && !IsBlack(LPrimaryColor) && string.IsNullOrEmpty(Texture))
				{
					PrimaryColor = MaterialUtils.ConvertColor(LPrimaryColor);
				}
				
				SpecularColor = LSpecularColor;
			}
		}

		string FindP2MProperty(string InFile, string InPropertyName)
		{
			string Result = "";
			int Pos = InFile.IndexOf(InPropertyName);
			if (Pos >= 0)
			{
				int Len = InFile.Length;
				Pos += InPropertyName.Length;

				while (Pos != Len && (InFile[Pos] == ' ' || InFile[Pos] == '\t' || InFile[Pos] == '\r' || InFile[Pos] == '\n'))
				{
					Pos++;
				}

				while (Pos != Len && InFile[Pos] != ' ' && InFile[Pos] != '\t' && InFile[Pos] != '\r' && InFile[Pos] != '\n')
				{
					Result += InFile[Pos++];
				}
			}
			return Result;
		}

		string FindP2MPropertyString(string InFile, string InPropertyName)
		{
			string Result = "";
			int Pos = InFile.IndexOf(InPropertyName);
			if (Pos >= 0)
			{
				int Len = InFile.Length;
				Pos += InPropertyName.Length;
				while (Pos != Len && InFile[Pos] != '\"')
				{
					Pos++;
				}
				if (Pos != Len)
				{
					Pos++;
				}
				while (Pos != Len && InFile[Pos] != '\"')
				{
					Result += InFile[Pos++];
				}
			}
			return Result;
		}

		public override string ToString()
		{
			return Name;
		}

		public FUVPlane GetUVPlane(FVec3 UDir, FVec3 VDir)
		{
			FUVPlane UVPlane = new FUVPlane();

			if (!MathUtils.Equals(RotationAngle, 0.0))
			{
				FVec3 VecZ = FVec3.Cross(UDir, VDir);
				FMatrix4 RotateMatrix = FMatrix4.FromRotationAxisAngle(VecZ, (float)RotationAngle);
				UVPlane.UDirection = (RotateMatrix.TransformVector(UDir)).Normalized();
				UVPlane.VDirection = (RotateMatrix.TransformVector(VDir)).Normalized();
			}
			else
			{
				UVPlane.UDirection = UDir;
				UVPlane.VDirection = VDir;
			}
			UVPlane.Offset = new FVec3(
				XPos * UDir.X + YPos * VDir.X - CenterPoint.X,
				XPos * UDir.Y + YPos * VDir.Y - CenterPoint.Y,
				XPos * UDir.Z + YPos * VDir.Z - CenterPoint.Z);

			UVPlane.Normal = new FVec3() - FVec3.Cross(UVPlane.UDirection, UVPlane.VDirection);

			return UVPlane;
		}

		public List<FUVPlane> ComputeUVPlanes()
		{
			List<FUVPlane> Planes = new List<FUVPlane>();

			switch (UVMappingType)
			{
				case FMaterial.EMappingType.TYPE_SPHERICAL:
				case FMaterial.EMappingType.TYPE_PROJECTION:
				{
					Planes.Add(GetUVPlane(UDirection, VDirection));
				}
				break;
				case FMaterial.EMappingType.TYPE_AUTOMATIC:
				case FMaterial.EMappingType.TYPE_CYLINDRICAL:
				{
					FVec3 cross = new FVec3(0f, 0f, 0f) - FVec3.Cross(UDirection, VDirection);
					Planes.Add(GetUVPlane(UDirection, VDirection));
					Planes.Add(GetUVPlane(UDirection, cross));
					Planes.Add(GetUVPlane(VDirection, cross));
					Planes.Add(GetUVPlane(new FVec3() - UDirection, cross));
					Planes.Add(GetUVPlane(new FVec3() - VDirection, cross));
					Planes.Add(GetUVPlane(UDirection, new FVec3() - VDirection));
				}
				break;
			}

			return Planes;
		}

		public FUVPlane GetTexturePlane(List<FUVPlane> InPlanes, FVec3 InVertexNormal)
		{
			int PlaneIdx = -1;
			switch (UVMappingType)
			{
				case FMaterial.EMappingType.TYPE_SPHERICAL:
				case FMaterial.EMappingType.TYPE_PROJECTION: PlaneIdx = 0; break;
				case FMaterial.EMappingType.TYPE_AUTOMATIC:
				case FMaterial.EMappingType.TYPE_CYLINDRICAL:
				{
					double Max = double.MinValue;
					for (var Idx = 0; Idx < InPlanes.Count; Idx++)
					{
						FVec3 BoxPlaneVector = InPlanes[Idx].Normal;
						double Dot = FVec3.Dot(InVertexNormal, BoxPlaneVector);
						if (Dot > Max)
						{
							Max = Dot;
							PlaneIdx = Idx;
						}
					}
					break;
				}
			}
			return (PlaneIdx >= 0) ? InPlanes[PlaneIdx] : null;
		}

		public FVec3 RotateVectorByXY(FVec3 InVec, FVec3 InModelCenter)
		{
			FMatrix4 Matrix = null;
			FVec3 CPoint = InModelCenter;

			Matrix = FMatrix4.RotateVectorByAxis(CPoint, Matrix, FVec3.XAxis, Direction1RotationAngle);
			Matrix = FMatrix4.RotateVectorByAxis(CPoint, Matrix, FVec3.YAxis, Direction2RotationAngle);

			if (Matrix != null)
			{
				InVec = (Matrix.TransformPoint(InVec)).Normalized();
			}

			return InVec;
		}

		public FVec2 ComputeNormalAtan2(FVec3 InVertex, FVec3 InModelCenter)
		{
			FVec3 Normal = InVertex - InModelCenter;
			FVec3 UVCross = FVec3.Cross(new FVec3() - UDirection, VDirection);
			FVec3 UVNormal = new FVec3(-FVec3.Dot(Normal, UDirection), FVec3.Dot(Normal, VDirection), FVec3.Dot(Normal, UVCross));
			return new FVec2(((float)Math.Atan2(UVNormal.Z, UVNormal.X) / (Math.PI * 2)), UVNormal.Y);
		}

		public FVec2 ComputeVertexUV(FUVPlane InPlane, FVec3 InVertex)
		{
			float FlipU = MirrorHorizontal ? -1f : 1f;
			float FlipV = MirrorVertical ? -1f : 1f;
			float U = (float)((FVec3.Dot((InVertex + InPlane.Offset), InPlane.UDirection) * FlipU) / Width);
			float V = (float)((FVec3.Dot((InVertex + InPlane.Offset), InPlane.VDirection) * FlipV) / Height);
			return new FVec2(U, V);
		}

		public static bool AreTheSame(FMaterial InMat1, FMaterial InMat2, bool InNameMustAlsoBeSame)
		{
			if (InNameMustAlsoBeSame) if (InMat1.Name != InMat2.Name) return false;

			if (InMat1.ShaderName != InMat2.ShaderName) return false;
			if (InMat1.FileName != InMat2.FileName) return false;
			if (InMat1.BumpTextureFileName != InMat2.BumpTextureFileName) return false;
			if (InMat1.Texture != InMat2.Texture) return false;
			if (InMat1.Type != InMat2.Type) return false;
			if (InMat1.BumpMap != InMat2.BumpMap) return false;
			if (!MathUtils.Equals(InMat1.BumpAmplitude, InMat2.BumpAmplitude)) return false;
			if (!MathUtils.Equals(InMat1.Emission, InMat2.Emission)) return false;
			if (!MathUtils.Equals(InMat1.Glossy, InMat2.Glossy)) return false;
			if (!MathUtils.Equals(InMat1.IOR, InMat2.IOR)) return false;
			if (InMat1.ColorForm != InMat2.ColorForm) return false;
			if (!MathUtils.Equals(InMat1.PrimaryColor, InMat2.PrimaryColor)) return false;
			if (!MathUtils.Equals(InMat1.SecondaryColor, InMat2.SecondaryColor)) return false;
			if (!MathUtils.Equals(InMat1.TertiaryColor, InMat2.TertiaryColor)) return false;
			if (!MathUtils.Equals(InMat1.Reflectivity, InMat2.Reflectivity)) return false;
			if (!MathUtils.Equals(InMat1.RotationAngle, InMat2.RotationAngle)) return false;
			if (!MathUtils.Equals(InMat1.SpecularSpread, InMat2.SpecularSpread)) return false;
			if (!MathUtils.Equals(InMat1.Specular, InMat2.Specular)) return false;
			if (!MathUtils.Equals(InMat1.Roughness, InMat2.Roughness)) return false;
			if (!MathUtils.Equals(InMat1.MetallicRoughness, InMat2.MetallicRoughness)) return false;
			if (InMat1.SpecularColor != InMat2.SpecularColor) return false;
			if (!MathUtils.Equals(InMat1.Transparency, InMat2.Transparency)) return false;
			if (!MathUtils.Equals(InMat1.Translucency, InMat2.Translucency)) return false;

			// Only take into account UV mapping differences if there are actual textures assigned
			if (InMat1.Texture.Length > 0 || InMat1.BumpTextureFileName.Length > 0)
			{
				if (!MathUtils.Equals(InMat1.Width, InMat2.Width)) return false;
				if (!MathUtils.Equals(InMat1.Height, InMat2.Height)) return false;
				if (!MathUtils.Equals(InMat1.XPos, InMat2.XPos)) return false;
				if (!MathUtils.Equals(InMat1.YPos, InMat2.YPos)) return false;
				if (!MathUtils.Equals(InMat1.Rotation, InMat2.Rotation)) return false;
				if (!MathUtils.Equals(InMat1.CenterPoint, InMat2.CenterPoint)) return false;
				if (!MathUtils.Equals(InMat1.UDirection, InMat2.UDirection)) return false;
				if (!MathUtils.Equals(InMat1.VDirection, InMat2.VDirection)) return false;
				if (!MathUtils.Equals(InMat1.Direction1RotationAngle, InMat2.Direction1RotationAngle)) return false;
				if (!MathUtils.Equals(InMat1.Direction2RotationAngle, InMat2.Direction2RotationAngle)) return false;
				if (InMat1.MirrorVertical != InMat2.MirrorVertical) return false;
				if (InMat1.MirrorHorizontal != InMat2.MirrorHorizontal) return false;
				if (InMat1.UVMappingType != InMat2.UVMappingType) return false;
			}

			if (InMat1.ProjectionReference != InMat2.ProjectionReference) return false;
			if (!MathUtils.Equals(InMat1.Diffuse, InMat2.Diffuse)) return false;
			if (InMat1.BlurryReflections != InMat2.BlurryReflections) return false;

			return true;
		}
	}
}
