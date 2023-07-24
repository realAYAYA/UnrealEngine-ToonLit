// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_CV_C
#define _SYMS_META_CV_C
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1150
SYMS_API SYMS_Language
syms_cv_base_language_from_cv_language(SYMS_CvLanguage v){
SYMS_Language result = SYMS_Language_Null;
switch (v){
default: break;
case SYMS_CvLanguage_C: result = SYMS_Language_C; break;
case SYMS_CvLanguage_CXX: result = SYMS_Language_CPlusPlus; break;
case SYMS_CvLanguage_FORTRAN: result = SYMS_Language_Fortran; break;
case SYMS_CvLanguage_MASM: result = SYMS_Language_MASM; break;
case SYMS_CvLanguage_PASCAL: result = SYMS_Language_Pascal; break;
case SYMS_CvLanguage_BASIC: result = SYMS_Language_Basic; break;
case SYMS_CvLanguage_COBOL: result = SYMS_Language_Cobol; break;
case SYMS_CvLanguage_LINK: result = SYMS_Language_Link; break;
case SYMS_CvLanguage_CVTRES: result = SYMS_Language_CVTRES; break;
case SYMS_CvLanguage_CVTPGD: result = SYMS_Language_CVTPGD; break;
case SYMS_CvLanguage_CSHARP: result = SYMS_Language_CSharp; break;
case SYMS_CvLanguage_VB: result = SYMS_Language_VisualBasic; break;
case SYMS_CvLanguage_ILASM: result = SYMS_Language_ILASM; break;
case SYMS_CvLanguage_JAVA: result = SYMS_Language_Java; break;
case SYMS_CvLanguage_JSCRIPT: result = SYMS_Language_JavaScript; break;
case SYMS_CvLanguage_MSIL: result = SYMS_Language_MSIL; break;
case SYMS_CvLanguage_HLSL: result = SYMS_Language_HLSL; break;
}
return(result);
}
SYMS_API SYMS_MemVisibility
syms_mem_visibility_from_member_access(SYMS_CvMemberAccess v){
SYMS_MemVisibility result = SYMS_MemVisibility_Null;
switch (v){
default: break;
case SYMS_CvMemberAccess_NULL: result = SYMS_MemVisibility_Null; break;
case SYMS_CvMemberAccess_PRIVATE: result = SYMS_MemVisibility_Private; break;
case SYMS_CvMemberAccess_PROTECTED: result = SYMS_MemVisibility_Protected; break;
case SYMS_CvMemberAccess_PUBLIC: result = SYMS_MemVisibility_Public; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1607
#endif
