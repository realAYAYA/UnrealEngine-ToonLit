// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:flutter/material.dart';
import 'package:logging/logging.dart';
import 'package:xml/xml.dart';

final _log = Logger('ParsedRichText');

/// A function that takes the [parentStyle] of the parent text node and returns a new style for the current node.
typedef _TextStyleBuilder = TextStyle Function(TextStyle parentStyle);

/// Map from XML node names to builders for the associated text style.
final Map<String, _TextStyleBuilder> _styleBuildersByTagName = {
  'b': (parentStyle) => parentStyle.copyWith(
        fontVariations: [FontVariation('wght', 700)],
      ),
  'i': (parentStyle) => parentStyle.copyWith(
        fontStyle: FontStyle.italic,
      ),
};

/// XML node names for which missing style warnings have already been printed.
final Set<String> _alreadyWarnedMissingStyleNames = {};

/// Parses an input string and converts it to rich text based on XML-style tags.
class ParsedRichText extends StatelessWidget {
  const ParsedRichText(
    this.text, {
    Key? key,
    this.style,
  }) : super(key: key);

  /// The text to parse.
  final String text;

  /// The base style for the text. If null, use the default style for the context.
  final TextStyle? style;

  @override
  Widget build(BuildContext context) {
    final TextStyle baseStyle = style ?? DefaultTextStyle.of(context).style;

    TextSpan textSpan;
    try {
      final XmlNode rootNode = XmlDocumentFragment.parse(text);
      textSpan = _makeTextSpanFromNode(xmlNode: rootNode, style: baseStyle);
    } catch (e) {
      _log.severe('Failed to parse rich text. Text will not be displayed. Input string:\n"$text"\nError: $e');
      textSpan = TextSpan();
    }

    return RichText(text: textSpan);
  }

  /// Given a [xmlNode], return a text span with the appropriate style and containing spans for any of its descendants.
  /// The provided [style] will be used if the node has no associated style.
  TextSpan _makeTextSpanFromNode({
    required XmlNode xmlNode,
    required TextStyle style,
  }) {
    if (xmlNode is XmlElement) {
      final String nodeName = xmlNode.name.toString();
      final _TextStyleBuilder? styleBuilder = _styleBuildersByTagName[nodeName];

      if (styleBuilder == null) {
        if (_alreadyWarnedMissingStyleNames.add(nodeName)) {
          _log.warning('No style for "$nodeName" tag; falling back to default');
        }
      } else {
        style = styleBuilder.call(style);
      }
    }

    if (xmlNode.children.length > 0) {
      final childrenSpans = xmlNode.children
          .map((child) => _makeTextSpanFromNode(
                xmlNode: child,
                style: style,
              ))
          .toList(growable: false);

      return TextSpan(
        children: childrenSpans,
        style: style,
      );
    }

    return TextSpan(text: xmlNode.text, style: style);
  }
}
