# Copyright Epic Games, Inc. All Rights Reserved.

cmake_minimum_required(VERSION 3.5.2)

# CMake script to build ICU
project(icu)

option(BUILD_ICU_MONOLITHIC "Build ICU as a single monolithic library, rather than as several separate libraries" ON)
option(BUILD_ICU_STATIC "Build ICU as a static library, rather than as a shared/dynamic library" ON)

set(ICU_MAJOR_VERSION 64)
set(ICU_SRC_ROOT ../source)
set(ICU_BIN_ROOT ${CMAKE_BINARY_DIR}/../)

# Building ICU as a static library?
if (${BUILD_ICU_STATIC})
	set(ICU_LIBRARY_TYPE STATIC)
	add_definitions(-DU_STATIC_IMPLEMENTATION)
else()
	set(ICU_LIBRARY_TYPE SHARED)
	set(ICU_LIBRARY_SUFFIX ${ICU_MAJOR_VERSION})
	if (${BUILD_ICU_MONOLITHIC})
		add_definitions(-DU_COMBINED_IMPLEMENTATION)
	endif()
endif()

# Configure the library names
set(ICU_LIBRARY_NAME icu${ICU_LIBRARY_SUFFIX})
set(ICU_DT_LIBRARY_NAME icudt${ICU_LIBRARY_SUFFIX})
set(ICU_UC_LIBRARY_NAME icuuc${ICU_LIBRARY_SUFFIX})
set(ICU_IN_LIBRARY_NAME icuin${ICU_LIBRARY_SUFFIX})
set(ICU_IO_LIBRARY_NAME icuio${ICU_LIBRARY_SUFFIX})

# Put our build output at the same level as this CMake file, so we can easily delete the temporary files and only check-in the final libs
if (NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ICU_BIN_ROOT})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ICU_BIN_ROOT})
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ICU_BIN_ROOT})
endif()

# Disable CRT warnings on Windows
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

# Disable internal ICU deprecation warnings
add_definitions(-DU_ATTRIBUTE_DEPRECATED=)

# Disable general file I/O since we rely on u_setDataFileFunctions instead, which works even when file I/O is disabled
add_definitions(-DUCONFIG_NO_FILE_IO)

# Include the Unicode Common and Internationalization headers
include_directories(${ICU_SRC_ROOT}/common ${ICU_SRC_ROOT}/i18n)

# Force MSVC to treat source files as UTF-8 encoded (ICU files are UTF-8 encoded, but they lack a BOM which MSVC needs to treat them as such)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Use the stub data file since we load the data at runtime rather than compiling it into the library
set(ICU_DT_SOURCES 
	stubdata.cpp
	)
set(TEMP_LIST "")
foreach(SOURCE_FILE ${ICU_DT_SOURCES})
	list(APPEND TEMP_LIST ${ICU_SRC_ROOT}/stubdata/${SOURCE_FILE})
endforeach(SOURCE_FILE)
set(ICU_DT_SOURCES ${TEMP_LIST})
set(TEMP_LIST "")

# List of source files to compile taken from common/Makefile.in
set(ICU_UC_SOURCES 
	errorcode.cpp putil.cpp umath.cpp utypes.cpp uinvchar.cpp umutex.cpp ucln_cmn.cpp
	uinit.cpp uobject.cpp cmemory.cpp charstr.cpp cstr.cpp
	udata.cpp ucmndata.cpp udatamem.cpp umapfile.cpp udataswp.cpp utrie_swap.cpp ucol_swp.cpp utrace.cpp
	uhash.cpp uhash_us.cpp uenum.cpp ustrenum.cpp uvector.cpp ustack.cpp uvectr32.cpp uvectr64.cpp
	ucnv.cpp ucnv_bld.cpp ucnv_cnv.cpp ucnv_io.cpp ucnv_cb.cpp ucnv_err.cpp ucnvlat1.cpp
	ucnv_u7.cpp ucnv_u8.cpp ucnv_u16.cpp ucnv_u32.cpp ucnvscsu.cpp ucnvbocu.cpp
	ucnv_ext.cpp ucnvmbcs.cpp ucnv2022.cpp ucnvhz.cpp ucnv_lmb.cpp ucnvisci.cpp ucnvdisp.cpp ucnv_set.cpp ucnv_ct.cpp
	resource.cpp uresbund.cpp ures_cnv.cpp uresdata.cpp resbund.cpp resbund_cnv.cpp
	ucurr.cpp
	localebuilder.cpp
	messagepattern.cpp ucat.cpp locmap.cpp uloc.cpp locid.cpp locutil.cpp locavailable.cpp locdispnames.cpp locdspnm.cpp loclikely.cpp locresdata.cpp
	bytestream.cpp stringpiece.cpp bytesinkutil.cpp
	stringtriebuilder.cpp bytestriebuilder.cpp
	bytestrie.cpp bytestrieiterator.cpp
	ucharstrie.cpp ucharstriebuilder.cpp ucharstrieiterator.cpp
	dictionarydata.cpp
	edits.cpp
	appendable.cpp ustr_cnv.cpp unistr_cnv.cpp unistr.cpp unistr_case.cpp unistr_props.cpp
	utf_impl.cpp ustring.cpp ustrcase.cpp ucasemap.cpp ucasemap_titlecase_brkiter.cpp cstring.cpp ustrfmt.cpp ustrtrns.cpp ustr_wcs.cpp utext.cpp
	unistr_case_locale.cpp ustrcase_locale.cpp unistr_titlecase_brkiter.cpp ustr_titlecase_brkiter.cpp
	normalizer2impl.cpp normalizer2.cpp filterednormalizer2.cpp normlzr.cpp unorm.cpp unormcmp.cpp loadednormalizer2impl.cpp
	chariter.cpp schriter.cpp uchriter.cpp uiter.cpp
	patternprops.cpp uchar.cpp uprops.cpp ucase.cpp propname.cpp ubidi_props.cpp characterproperties.cpp
	ubidi.cpp ubidiwrt.cpp ubidiln.cpp ushape.cpp
	uscript.cpp uscript_props.cpp usc_impl.cpp unames.cpp
	utrie.cpp utrie2.cpp utrie2_builder.cpp ucptrie.cpp umutablecptrie.cpp
	bmpset.cpp unisetspan.cpp uset_props.cpp uniset_props.cpp uniset_closure.cpp uset.cpp uniset.cpp usetiter.cpp ruleiter.cpp caniter.cpp unifilt.cpp unifunct.cpp
	uarrsort.cpp brkiter.cpp ubrk.cpp brkeng.cpp dictbe.cpp filteredbrk.cpp
	rbbi.cpp rbbidata.cpp rbbinode.cpp rbbirb.cpp rbbiscan.cpp rbbisetb.cpp rbbistbl.cpp rbbitblb.cpp rbbi_cache.cpp
	serv.cpp servnotf.cpp servls.cpp servlk.cpp servlkf.cpp servrbf.cpp servslkf.cpp
	uidna.cpp usprep.cpp uts46.cpp punycode.cpp
	util.cpp util_props.cpp parsepos.cpp locbased.cpp cwchar.cpp wintz.cpp dtintrv.cpp ucnvsel.cpp propsvec.cpp
	ulist.cpp uloc_tag.cpp icudataver.cpp icuplug.cpp
	sharedobject.cpp simpleformatter.cpp unifiedcache.cpp uloc_keytype.cpp
	ubiditransform.cpp
	pluralmap.cpp
	static_unicode_sets.cpp
	)
set(TEMP_LIST "")
foreach(SOURCE_FILE ${ICU_UC_SOURCES})
	list(APPEND TEMP_LIST ${ICU_SRC_ROOT}/common/${SOURCE_FILE})
endforeach(SOURCE_FILE)
set(ICU_UC_SOURCES ${TEMP_LIST})
set(TEMP_LIST "")
	
# List of source files to compile taken from i18n/Makefile.in
set(ICU_IN_SOURCES 
	ucln_in.cpp
	fmtable.cpp format.cpp msgfmt.cpp umsg.cpp numfmt.cpp unum.cpp decimfmt.cpp dcfmtsym.cpp
	fmtable_cnv.cpp
	choicfmt.cpp datefmt.cpp smpdtfmt.cpp reldtfmt.cpp dtfmtsym.cpp udat.cpp dtptngen.cpp udatpg.cpp
	nfrs.cpp nfrule.cpp nfsubs.cpp rbnf.cpp numsys.cpp unumsys.cpp ucsdet.cpp
	ucal.cpp calendar.cpp gregocal.cpp timezone.cpp simpletz.cpp olsontz.cpp
	astro.cpp taiwncal.cpp buddhcal.cpp persncal.cpp islamcal.cpp japancal.cpp gregoimp.cpp hebrwcal.cpp
	indiancal.cpp chnsecal.cpp cecal.cpp coptccal.cpp dangical.cpp ethpccal.cpp
	coleitr.cpp coll.cpp sortkey.cpp bocsu.cpp ucoleitr.cpp
	ucol.cpp ucol_res.cpp ucol_sit.cpp
	collation.cpp collationsettings.cpp collationdata.cpp collationtailoring.cpp
	collationdatareader.cpp collationdatawriter.cpp collationfcd.cpp
	collationiterator.cpp utf16collationiterator.cpp utf8collationiterator.cpp uitercollationiterator.cpp
	collationsets.cpp
	collationcompare.cpp collationfastlatin.cpp collationkeys.cpp rulebasedcollator.cpp collationroot.cpp
	collationrootelements.cpp collationdatabuilder.cpp
	collationweights.cpp collationruleparser.cpp collationbuilder.cpp collationfastlatinbuilder.cpp
	listformatter.cpp ulistformatter.cpp
	strmatch.cpp usearch.cpp search.cpp stsearch.cpp
	translit.cpp utrans.cpp esctrn.cpp unesctrn.cpp funcrepl.cpp strrepl.cpp tridpars.cpp
	cpdtrans.cpp rbt.cpp rbt_data.cpp rbt_pars.cpp rbt_rule.cpp rbt_set.cpp
	nultrans.cpp remtrans.cpp casetrn.cpp titletrn.cpp tolowtrn.cpp toupptrn.cpp anytrans.cpp
	name2uni.cpp uni2name.cpp nortrans.cpp quant.cpp transreg.cpp brktrans.cpp
	regexcmp.cpp rematch.cpp repattrn.cpp regexst.cpp regextxt.cpp regeximp.cpp uregex.cpp uregexc.cpp
	ulocdata.cpp measfmt.cpp currfmt.cpp curramt.cpp currunit.cpp measure.cpp utmscale.cpp
	csdetect.cpp csmatch.cpp csr2022.cpp csrecog.cpp csrmbcs.cpp csrsbcs.cpp csrucode.cpp csrutf8.cpp inputext.cpp
	wintzimpl.cpp windtfmt.cpp winnmfmt.cpp basictz.cpp dtrule.cpp rbtz.cpp tzrule.cpp tztrans.cpp vtzone.cpp zonemeta.cpp
	standardplural.cpp upluralrules.cpp plurrule.cpp plurfmt.cpp selfmt.cpp dtitvfmt.cpp dtitvinf.cpp udateintervalformat.cpp
	tmunit.cpp tmutamt.cpp tmutfmt.cpp currpinf.cpp
	uspoof.cpp uspoof_impl.cpp uspoof_build.cpp uspoof_conf.cpp smpdtfst.cpp
	ztrans.cpp zrule.cpp vzone.cpp fphdlimp.cpp fpositer.cpp ufieldpositer.cpp
	decNumber.cpp decContext.cpp alphaindex.cpp tznames.cpp tznames_impl.cpp tzgnames.cpp
	tzfmt.cpp compactdecimalformat.cpp gender.cpp region.cpp scriptset.cpp
	uregion.cpp reldatefmt.cpp quantityformatter.cpp measunit.cpp
	sharedbreakiterator.cpp scientificnumberformatter.cpp dayperiodrules.cpp nounit.cpp
	number_affixutils.cpp number_compact.cpp number_decimalquantity.cpp
	number_decimfmtprops.cpp number_fluent.cpp number_formatimpl.cpp number_grouping.cpp
	number_integerwidth.cpp number_longnames.cpp number_modifiers.cpp number_notation.cpp number_output.cpp
	number_padding.cpp number_patternmodifier.cpp number_patternstring.cpp
	number_rounding.cpp number_scientific.cpp number_stringbuilder.cpp number_utils.cpp number_asformat.cpp
	number_mapper.cpp number_multiplier.cpp number_currencysymbols.cpp number_skeletons.cpp number_capi.cpp
	double-conversion.cpp double-conversion-bignum-dtoa.cpp double-conversion-bignum.cpp
	double-conversion-cached-powers.cpp double-conversion-diy-fp.cpp
	double-conversion-fast-dtoa.cpp double-conversion-strtod.cpp
	numparse_stringsegment.cpp numparse_parsednumber.cpp numparse_impl.cpp
	numparse_symbols.cpp numparse_decimal.cpp numparse_scientific.cpp numparse_currency.cpp
	numparse_affixes.cpp numparse_compositions.cpp numparse_validators.cpp
	numrange_fluent.cpp numrange_impl.cpp
	erarules.cpp formattedvalue.cpp formattedval_iterimpl.cpp formattedval_sbimpl.cpp
	)
set(TEMP_LIST "")
foreach(SOURCE_FILE ${ICU_IN_SOURCES})
	list(APPEND TEMP_LIST ${ICU_SRC_ROOT}/i18n/${SOURCE_FILE})
endforeach(SOURCE_FILE)
set(ICU_IN_SOURCES ${TEMP_LIST})
set(TEMP_LIST "")

# List of source files to compile taken from io/Makefile.in
set(ICU_IO_SOURCES 
	locbund.cpp ufile.cpp ufmt_cmn.cpp uprintf.cpp uprntf_p.cpp
	uscanf.cpp uscanf_p.cpp ustdio.cpp sprintf.cpp sscanf.cpp
	ustream.cpp ucln_io.cpp
	)
set(TEMP_LIST "")
foreach(SOURCE_FILE ${ICU_IO_SOURCES})
	list(APPEND TEMP_LIST ${ICU_SRC_ROOT}/io/${SOURCE_FILE})
endforeach(SOURCE_FILE)
set(ICU_IO_SOURCES ${TEMP_LIST})
set(TEMP_LIST "")

# Building ICU as a monolithic library?
if (${BUILD_ICU_MONOLITHIC})
	# Create the icu (Monolithic) library
	add_library(${ICU_LIBRARY_NAME} ${ICU_LIBRARY_TYPE} ${ICU_DT_SOURCES} ${ICU_UC_SOURCES} ${ICU_IN_SOURCES} ${ICU_IO_SOURCES})
	if (${CMAKE_VERSION} VERSION_LESS 3.12)
		set_property(TARGET ${ICU_LIBRARY_NAME} PROPERTY CXX_STANDARD 11)
	else()
		target_compile_features(${ICU_LIBRARY_NAME} PRIVATE cxx_std_11) 
	endif()
	target_compile_definitions(${ICU_LIBRARY_NAME} PRIVATE -DU_COMMON_IMPLEMENTATION -DU_I18N_IMPLEMENTATION -DU_IO_IMPLEMENTATION)
else()
	# Create the icudt (Data) library
	add_library(${ICU_DT_LIBRARY_NAME} ${ICU_LIBRARY_TYPE} ${ICU_DT_SOURCES})
	target_compile_features(${ICU_DT_LIBRARY_NAME} PRIVATE cxx_std_11)

	# Create the icuuc (Unicode Common) library
	add_library(${ICU_UC_LIBRARY_NAME} ${ICU_LIBRARY_TYPE} ${ICU_UC_SOURCES})
	if (${CMAKE_VERSION} VERSION_LESS 3.12)
		set_property(TARGET ${ICU_UC_LIBRARY_NAME} PROPERTY CXX_STANDARD 11)
	else()
		target_compile_features(${ICU_UC_LIBRARY_NAME} PRIVATE cxx_std_11)
	endif()
	target_compile_definitions(${ICU_UC_LIBRARY_NAME} PRIVATE -DU_COMMON_IMPLEMENTATION)
	target_link_libraries(${ICU_UC_LIBRARY_NAME} ${ICU_DT_LIBRARY_NAME})

	# Create the icuin (Internationalization) library
	add_library(${ICU_IN_LIBRARY_NAME} ${ICU_LIBRARY_TYPE} ${ICU_IN_SOURCES})
	if (${CMAKE_VERSION} VERSION_LESS 3.12)
		set_property(TARGET ${ICU_IN_LIBRARY_NAME} PROPERTY CXX_STANDARD 11)
	else()
		target_compile_features(${ICU_IN_LIBRARY_NAME} PRIVATE cxx_std_11)
	endif()
	target_compile_definitions(${ICU_IN_LIBRARY_NAME} PRIVATE -DU_I18N_IMPLEMENTATION)
	target_link_libraries(${ICU_IN_LIBRARY_NAME} ${ICU_UC_LIBRARY_NAME})

	# Create the icuio (Input/Output) library
	add_library(${ICU_IO_LIBRARY_NAME} ${ICU_LIBRARY_TYPE} ${ICU_IO_SOURCES})
	if (${CMAKE_VERSION} VERSION_LESS 3.12)
		set_property(TARGET ${ICU_IO_LIBRARY_NAME} PROPERTY CXX_STANDARD 11)
	else()
		target_compile_features(${ICU_IO_LIBRARY_NAME} PRIVATE cxx_std_11)
	endif()
	target_compile_definitions(${ICU_IO_LIBRARY_NAME} PRIVATE -DU_IO_IMPLEMENTATION)
	target_link_libraries(${ICU_IO_LIBRARY_NAME} ${ICU_UC_LIBRARY_NAME} ${ICU_IN_LIBRARY_NAME})
endif()
