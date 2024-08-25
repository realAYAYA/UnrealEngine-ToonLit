import { Theme } from "@fluentui/react";

export type HordeThemeExtensions = {

    // Whether using the dark theme
    darkTheme: boolean;

    // background color of the top navigation bar
    topNavBackground: string;

    // background color of the top bread crumbs area
    breadCrumbsBackground: string;

    // background of neutral area
    neutralBackground: string;

    // background of neutral area
    dividerColor: string;

    // background of content areas
    contentBackground: string;

    // background color for scrollbar track
    scrollbarTrackColor: string;

    // background color for scrollbar thumb
    scrollbarThumbColor: string;

}

export interface HordeTheme extends Theme {
    horde: HordeThemeExtensions;
}
