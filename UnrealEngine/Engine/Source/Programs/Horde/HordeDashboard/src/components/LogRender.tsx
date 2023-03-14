// Copyright Epic Games, Inc. All Rights Reserved.
import { Stack, Text } from '@fluentui/react';
import React from 'react';
import Highlight from 'react-highlighter';
import { IssueData, LogLine } from '../backend/Api';

enum TagType {
   None,
   SourceFile,
   MSDNCode
}

export type LogItem = {
   line?: LogLine;
   lineNumber: number;
   requested: boolean;
   issueId?: number;
   issue?: IssueData;
}

const getLineNumber = (line: LogLine): number | undefined => {

   const properties = line.properties;
   if (!properties) {
      return undefined;
   }

   const lineProp = properties["line"];

   if (!lineProp || typeof (lineProp) !== "object") {
      return undefined;
   }

   return (lineProp as Record<string, number | undefined>).line;

}

const renderMessage = (line: LogLine, lineNumber: number | undefined, logStyle: any, search?: string) => {

   const message = line?.message ?? "";

   if (lineNumber === undefined || lineNumber === null) {
      lineNumber = getLineNumber(line);
   }

   const key = `log_line_${lineNumber}_message`;

   return <Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{message ?? ""}</Highlight >;

};


const renderTags = (line: LogLine, lineNumber: number | undefined, logStyle: any, tags: string[], search?: string) => {

   if (!line || !line.format || !line.properties) {
      return <Stack styles={{ root: { color: "#000000", paddingLeft: 8, whiteSpace: "pre", tabSize: "3" } }}>Internal log line format error</Stack>;
   }

   const properties = line.properties;
   const lineProp = properties["line"];

   if (lineNumber === undefined || lineNumber === null) {
      lineNumber = (lineProp as Record<string, number | undefined>)?.line ?? 0;
   }

   // render tags
   let renderedTags = tags.map((tag, index) => {

      let tagType = TagType.None;
      let property: Record<string, string | number> | string | undefined;
      let record: Record<string, string> | undefined;
      const key = `log_line_${lineNumber}_${index}`;

      let formatter = "";
      tag = tag.replace("{", "").replace("}", "");
      if (tag.indexOf(":") !== -1) {
         [tag, formatter] = tag.split(":");                  
      }

      property = properties[tag];

      // C# format specifiers, yes this could be more elegant
      if (formatter) {
         if (typeof (property) === "number") {

            let precision: number | undefined;

            if (formatter.startsWith("n") && formatter.length > 1) {
               precision = parseInt(formatter.slice(1));
            }

            if (formatter.startsWith("0.") && formatter.length > 2) {
               precision = 0;
               for (let i = 2; i < formatter.length; i++) {
                  if (formatter[i] === "0") {
                     precision++;
                  } else {
                     break;
                  }
               }
            }

            if (precision !== undefined && !isNaN(precision)) {
               property = (property as number).toFixed(precision);
            }

         }         
      }

      let type = "";
      let text = "";

      if (property && typeof (property) !== "string" && typeof (property) !== "number" && typeof (property) !== "boolean" && property !== null) {
         record = property as Record<string, string>;
      } else {
         text = (property === null || property === undefined) ? "null" : property.toString();
      }

      if (record) {
         
         type = record.type ?? record["$type"] ?? "";
         text = record.text ?? record["$text"] ?? "";

         if (type === "SourceFile") {
            tagType = TagType.SourceFile;
         }

         if (type === "ErrorCode" && text.startsWith("C")) {
            tagType = TagType.MSDNCode;
         }

      }

      if (tagType === TagType.None || !record) {

         return <Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight>;

      } else if (tagType === TagType.MSDNCode) {

         return <a key={key} target="_blank" rel="noopener noreferrer" href={`https://msdn.microsoft.com/query/dev16.query?appId=Dev16IDEF1&l=EN-US&k=k(${text.toLowerCase()})&rd=true`} onClick={(ev) => ev.stopPropagation()}><Highlight search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight></a>;

      } else if (tagType === TagType.SourceFile) {

         let depotPath = record.depotPath;

         if (!depotPath) {
            return <Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight>;
         }

         if (depotPath.indexOf("@") !== -1) {
            depotPath = depotPath.slice(0, depotPath.indexOf("@"));
         }

         depotPath = encodeURIComponent(depotPath);

         // @todo: handle line, it isn't in the source file meta data, and matching isn't obvious (file vs note, etc)
         let tagLine = "";
         if (tagLine) {
            depotPath += `&line=${tagLine}`;
         }

         return <a key={key} href={`ugs://timelapse?depotPath=${(depotPath)}`} onClick={(ev) => ev.stopPropagation()}><Highlight search={search ? search : ""} className={logStyle.logLine}>{record.relativePath ? record.relativePath : text}</Highlight></a>;
      }

      return <span key={key} />;
   })

   let remaining = line.format;

   renderedTags = renderedTags.map((t, idx) => {

      let current = remaining;

      const tag = tags[idx];
      const index = remaining.indexOf(tag);

      remaining = remaining.slice(tag.length + (index > 0 ? index : 0));

      if (index < 0) {
         console.error("not able to find tag in format");
         return <Text>Error, unable to find tag</Text>;
      }

      if (index === 0) {
         return t;
      }

      const rtags = [];

      const key = `log_line_${lineNumber}_${idx}_${index}_fragment`;

      rtags.push(<Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{current.slice(0, index)}</Highlight>);
      rtags.push(t);

      return rtags;

   }).flat();

   if (remaining) {
      const key = `log_line_${lineNumber}_remaining_fragment`;
      renderedTags.push(<Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{remaining}</Highlight>)
   }

   return <div>
      {renderedTags}
   </div>;

};

export const renderLine = (line: LogLine | undefined, lineNumber: number | undefined, logStyle: any, search?: string) => {

   if (!line) {
      return null;
   }

   const tagRegex = /{[^{}]+}/g
   let format = line?.format;

   let tags: string[] = [];
   if (format) {

      // some automation logs output their own structured logging, which is marked by {{}}
      if (format.indexOf("{{") !== -1) {
         format = format.replaceAll("{", "");
         format = format.replaceAll("}", "");
      }

      const match = format.match(tagRegex);
      if (match?.length)
         tags = match;
   }

   // we don't support c# alignment, as this requireds read behinds, etc 
   // so strip tags in this case and just output the line
   if (tags.find(t => t.indexOf(",") !== -1)) {
      tags = [];
   }


   if (tags.length && format && line.properties) {
      return renderTags(line, lineNumber, logStyle, tags, search);
   }

   return renderMessage(line, lineNumber, logStyle, search);

};


