import { PropertyInfo } from "./property";
import { NameVariant } from "./typeInfo";

export interface DelegateParameterInfo extends PropertyInfo {
    isConst?: boolean; // if unspecified, false
    accessModifier?: string // * or &
}

export interface DelegateInfo {
    name: NameVariant;
    parameters: Array<DelegateParameterInfo>;
    isDynamic?: boolean;
    isMulticast?: boolean;
}
