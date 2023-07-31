// Copyright Epic Games, Inc. All Rights Reserved.

import { JobState, LabelOutcome, LabelState } from "../backend/Api";
import dashboard, {StatusColor} from "../backend/Dashboard";

export const getJobStateColor = (state: JobState): string => {

    const colors = dashboard.getStatusColors();

    if (state === JobState.Waiting) {
        return colors.get(StatusColor.Waiting)!;
    }

    if (state === JobState.Running) {
        return colors.get(StatusColor.Running)!;
    }


    return colors.get(StatusColor.Unspecified)!;

}

export const getLabelColor = (state: LabelState | undefined, outcome: LabelOutcome | undefined): { primaryColor: string, secondaryColor?: string } => {

    if (!state || !outcome) {
        return {
            primaryColor: "#000000"
        }
    }

    const colors = dashboard.getStatusColors();

    if (state === LabelState.Complete) {
        switch (outcome!) {
            case LabelOutcome.Failure:
                return { primaryColor: colors.get(StatusColor.Failure)! };
            case LabelOutcome.Success:
                return { primaryColor: colors.get(StatusColor.Success)! };
            case LabelOutcome.Warnings:
                return { primaryColor: colors.get(StatusColor.Warnings)! };
            default:
                return { primaryColor: colors.get(StatusColor.Unspecified)! };
        }
    }

    if (outcome === LabelOutcome.Failure) {
        return { primaryColor: colors.get(StatusColor.Running)!, secondaryColor: colors.get(StatusColor.Failure)! };
    }

    if (outcome === LabelOutcome.Warnings) {
        return { primaryColor: colors.get(StatusColor.Running)!, secondaryColor: colors.get(StatusColor.Warnings)! };
    }

    return {
        primaryColor: colors.get(StatusColor.Running)!
    }
};

