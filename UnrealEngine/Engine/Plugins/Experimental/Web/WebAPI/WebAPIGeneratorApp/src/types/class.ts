import { FunctionInfo } from "./function";
import { MetadataInfo } from "./metadata";
import { PropertyInfo } from "./property";
import { NameVariant } from "./typeInfo";

export interface ClassInfo {
    description: string;
    name: NameVariant;
    namespace: string;
    module: string;
    specifiers: Array<MetadataInfo> | string;
    metadata: Array<MetadataInfo> | string;
    properties: Array<PropertyInfo>;
    functions: Array<FunctionInfo>;
}
