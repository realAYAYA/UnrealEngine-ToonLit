# Copyright Epic Games, Inc. All Rights Reserved.

#-------------------------------------------------------------------------------
class Tips(object):
    def get_tips(self):
        return (
            "'.p4 mergedown' will sort unresolve files into a separate changelist",
            "Integrate/unshelve changelists between branches with '.p4 cherrypick'",
            "Filter your sync with a .p4sync.txt file. '.p4 sync edit' to quick edit",
            "Clean branches with '.p4 clean'",
            "'p4 clean --dryrun' shows how much disk space would be reclaimed",
        )
