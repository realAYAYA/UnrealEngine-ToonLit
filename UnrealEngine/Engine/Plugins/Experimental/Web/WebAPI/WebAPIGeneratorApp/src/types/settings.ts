import { NameVariant } from "./typeInfo";

export interface SettingsClassInfo {
    name: NameVariant;
    base?: NameVariant;
    displayName?: string;
    host: string;
    baseUrl?: string;
    userAgent: string;
    dateTimeFormat?: string;
    schemes: Array<string>;
}

// const a = { hat: "h"};
// const b = { hat: "w"};
// const obj: SettingsClassInfo = {...a, ...b};
