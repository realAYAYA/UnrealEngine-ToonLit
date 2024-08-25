UEJpegComp is a super compressed jpeg format using oodle as an entropy coder 
backend. It tends to produce smaller files than JPG and is much more hardened
fuzz safety due to using oodle as a backend. 

The decoding output matches libjpegturbo, both when using SLOWDCT (default) 
and when using FASTDCT. UE as of this writing uses FASTDCT.

It was tested by exporting all JPEGs from fortnite and then running them
through a program which converts the JPEGs to UE-JPEGs, then loading those
UE-JPEGs and decoding them. You then compare the original JPEG's decoded
values vs the decoded UE-JPEG's decoded values to make sure they match. As 
of this writing, all JPEGs in fortnite binary match their decoded values.

It was fuzz tested via american-fuzzy-lop (https://lcamtuf.coredump.cx/afl/).

