import { MetadataInfo } from "./metadata";
import { PropertyInfo } from "./property";
import { NameVariant } from "./typeInfo";

export interface StructInfo {
    description: string;
    name: NameVariant;
    namespace: string;
    module: string;
    base?: NameVariant;
    specifiers: Array<MetadataInfo> | string;
    metadata: Array<MetadataInfo> | string;
    properties: Array<PropertyInfo>;
}
