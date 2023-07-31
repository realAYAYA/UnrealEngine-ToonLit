// Copyright Epic Games, Inc. All Rights Reserved.

import { mergeStyleSets } from "@fluentui/react";
import dashboard from "../backend/Dashboard";
import { modeColors, theme } from "../styles/Styles";

export type LogMetricType = {
    lineHeight: number;
    fontSize: number;
}

export const logMetricNormal = {
    lineHeight: 24,
    fontSize: 11
}

const adjustForDisplayScale = (window as any).safari === undefined && window.devicePixelRatio > 1.25;

export const logMetricSmall = {
    // note, making this any larger limits log line range for overflow
    // make height even to avoid rounding issues
    lineHeight: adjustForDisplayScale ? 14 : 18,
    // don't go below 10pt for legibility
    fontSize: 10
}

export const lineRenderStyleNormal = mergeStyleSets({
    logLine: {
        padding: 0,
        height: logMetricNormal.lineHeight,
        tabSize: "3",
        fontFamily: "Horde Cousine Regular, monospace, monospace",
        fontSize: logMetricNormal.fontSize,
        whiteSpace: "pre-wrap"

    }
});

export const lineRenderStyleSmall = mergeStyleSets({
    logLine: {
        padding: 0,
        height: logMetricSmall.lineHeight,
        tabSize: "3",
        fontFamily: "Horde Cousine Regular, monospace, monospace",
        fontSize: logMetricSmall.fontSize,
        whiteSpace: "pre-wrap"

    }
});

const logStyleBase = mergeStyleSets({
    container: {
        overflow: 'auto',
        height: 'calc(100vh - 292px)',
        marginTop: 8,
    },
    logLine: [
        {
            fontFamily: "Horde Cousine Regular, monospace, monospace"
        }
    ],
    logLineOuter: {
    },
    itemWarning: [
        {
            background: "#FEF8E7"
        }
    ],
    errorButton: {
        backgroundColor: "#EC4C47",
        borderStyle: "hidden",
        selectors: {
            ':link,:visited': {
                color: "#FFFFFF"
            },
            ':active,:hover': {
                color: "#F9F9FB",
                backgroundColor: "#DC3C37"
            }
        }
    },
    errorButtonDisabled: {
        backgroundColor: "rgb(243, 242, 241)"
    },
    warningButton: {
        backgroundColor: dashboard.darktheme ? theme.palette.yellow : "#F7D154",
        borderStyle: "hidden",
        selectors: {
            ':link,:visited': {
                color: modeColors.text
            },
            ':active,:hover': {
                backgroundColor: dashboard.darktheme ? "rgb(199, 173, 54)" : "#E7C144"
            }
        }
    },
    warningButtonDisabled: {
        backgroundColor: "rgb(243, 242, 241)"
    },
    gutter: [
        {
            padding: 0,
            margin: 0,
            paddingTop: 0,
            paddingBottom: 0,
            paddingRight: 14,
            marginTop: 0,
            marginBottom: 0,
        }
    ],
    gutterError: [
        {
            background: "#FEF6F6",
            borderLeftStyle: 'solid',
            borderLeftColor: "#EC4C47",
            borderLeftWidth: 6,
            padding: 0,
            margin: 0,
            paddingTop: 0,
            paddingBottom: 0,
            paddingRight: 8,
            marginTop: 0,
            marginBottom: 0,
        }
    ],
    gutterWarning: [
        {
            background: "#FEF8E7",
            borderLeftStyle: 'solid',
            borderLeftColor: "#F7D154",
            borderLeftWidth: 6,
            padding: 0,
            margin: 0,
            paddingTop: 0,
            paddingBottom: 0,
            paddingRight: 8,
            marginTop: 0,
            marginBottom: 0,
        }
    ],
    itemError: [
        {
            background: "#FEF6F6",
        }
    ]
});


export const logStyleNormal = mergeStyleSets(logStyleBase, {

    container: {
        selectors: {
            '.ms-List-cell': {
                height: logMetricNormal.lineHeight,
                lineHeight: logMetricNormal.lineHeight
            }
        }
    },
    logLine: [
        {
            fontSize: logMetricNormal.fontSize,
        }
    ],
    gutter: [
        {
            height: logMetricNormal.lineHeight
        }
    ],
    gutterError: [
        {
            height: logMetricNormal.lineHeight
        }
    ],
    gutterWarning: [
        {
            height: logMetricNormal.lineHeight
        }
    ]
});

export const logStyleSmall = mergeStyleSets(logStyleBase, {

    container: {        
        selectors: {
            '.ms-List-cell': {
                height: logMetricSmall.lineHeight,
                lineHeight: logMetricSmall.lineHeight
            }
        }
    },
    logLine: [
        {
            fontSize: logMetricSmall.fontSize,
        }
    ],
    gutter: [
        {
            height: logMetricSmall.lineHeight
        }
    ],
    gutterError: [
        {
            height: logMetricSmall.lineHeight
        }
    ],
    gutterWarning: [
        {
            height: logMetricSmall.lineHeight
        }
    ]
});
