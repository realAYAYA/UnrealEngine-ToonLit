#!/bin/sh
echo "moving to main"
cd main
echo "creating lang list"
# arguments retrieved from running makefile with debug output for the project
#this creates flite_lang_list.c
../tools/make_lang_list usenglish cmu_indic_lang  cmu_grapheme_lang cmulex cmu_indic_lex cmu_grapheme_lex
echo "creating voice list"
# arguments retrieved from running makefile in debug for the project
#this generates flite_voice_list.c
../tools/make_voice_list cmu_us_kal cmu_time_awb cmu_us_kal16 cmu_us_awb cmu_us_rms cmu_us_slt
echo "moving back to root"
cd ..
