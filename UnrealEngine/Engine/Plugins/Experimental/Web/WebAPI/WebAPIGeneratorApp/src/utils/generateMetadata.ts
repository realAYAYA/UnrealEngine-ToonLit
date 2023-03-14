import { MetadataInfo } from '../types/metadata';

const generateMetadata = (metadataMap: Array<MetadataInfo>): string => {
    let metadata_: string = "";
    metadataMap?.forEach((metadata: MetadataInfo) => {
        if (metadata_?.length !== 0) {
            metadata_ = `${metadata_}, ${metadata.key} = "${metadata.value}"`;
        } else {
            metadata_ = `meta = (${metadata.key} = "${metadata.value}"`;
        }
    })

    return `${metadata_})`;
}

module.exports = generateMetadata;