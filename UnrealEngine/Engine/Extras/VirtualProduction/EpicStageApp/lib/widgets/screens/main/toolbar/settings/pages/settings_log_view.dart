// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:io';

import 'package:flutter/material.dart';
import 'package:share_plus/share_plus.dart';

import '../../../../../../utilities/external_notifier.dart';
import '../../../../../../utilities/guarded_refresh_state.dart';
import '../../../../../../utilities/logging.dart';
import '../../../../../elements/asset_icon.dart';
import '../../../../../elements/epic_icon_button.dart';
import '../settings_generic.dart';

/// Arguments passed to the route when a [SettingsLogView] is pushed to the navigation stack.
class SettingsLogViewArguments {
  const SettingsLogViewArguments({required this.file});

  /// The file to open.
  final File file;
}

/// Page showing the full view of a log.
class SettingsLogView extends StatefulWidget {
  const SettingsLogView({Key? key}) : super(key: key);

  static const String route = '/log_view';

  @override
  State<SettingsLogView> createState() => _SettingsLogViewState();
}

class _SettingsLogViewState extends State<SettingsLogView> with GuardedRefreshState {
  final shareButtonKey = GlobalKey();

  @override
  Widget build(BuildContext context) {
    final arguments = ModalRoute.of(context)?.settings.arguments as SettingsLogViewArguments;

    return SettingsPageScaffold(
      title: Logging.getNameForLog(arguments.file),
      titleBarTrailing: EpicIconButton(
        key: shareButtonKey,
        iconPath: 'assets/images/icons/share.svg',
        color: Theme.of(context).colorScheme.primary,
        onPressed: () => _shareLog(arguments.file),
      ),
      body: SizedBox(
        height: MediaQuery.of(context).size.height - 120,
        child: FutureBuilder(
          future: arguments.file.readAsString(),
          builder: (context, AsyncSnapshot<String> snapshot) {
            return _LogText(text: snapshot.data);
          },
        ),
      ),
    );
  }

  /// Open a prompt to share the log's contents using the local operating system.
  void _shareLog(File log) async {
    // Need to provide this position for sharing to work on iPad
    final renderBox = shareButtonKey.currentContext?.findRenderObject() as RenderBox?;
    if (renderBox == null) {
      return;
    }

    final sharePosition = renderBox.localToGlobal(Offset.zero) & renderBox.size;

    Share.share(
      await log.readAsString(),
      subject: log.uri.pathSegments.last,
      sharePositionOrigin: sharePosition,
    );
  }
}

/// Widget that displays text from a log.
class _LogText extends StatefulWidget {
  const _LogText({Key? key, required this.text}) : super(key: key);

  /// Text to display.
  final String? text;

  @override
  State<_LogText> createState() => _LogTextState();
}

class _LogTextState extends State<_LogText> {
  /// Vertical scroll controller
  final _verticalScroll = ScrollController();

  /// Horizontal scroll controller
  final _horizontalScroll = ScrollController();

  /// Notifier for when the text is changed
  final _textChangeNotifier = ExternalNotifier();

  /// Text style to use for log text
  late TextStyle _textStyle;

  /// List of lines to display in the text
  List<String> _lines = [];

  /// Width of the text scrolling area
  double? _textWidth;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();

    _textStyle = Theme.of(context).textTheme.bodyMedium!.copyWith(
          fontFamily: 'Droid Sans Mono',
          fontSize: 12,
        );

    _updateText();
  }

  @override
  void didUpdateWidget(covariant _LogText oldWidget) {
    super.didUpdateWidget(oldWidget);

    if (oldWidget.text != widget.text) {
      _updateText();

      WidgetsBinding.instance.addPostFrameCallback((_) {
        _verticalScroll.jumpTo(_verticalScroll.initialScrollOffset);
        _horizontalScroll.jumpTo(_horizontalScroll.initialScrollOffset);

        _textChangeNotifier.notifyListeners();
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_textWidth == null) {
      return const _SettingsLogViewLoadingSpinner();
    }

    final scrollArea = MediaQuery.removePadding(
      context: context,
      removeTop: true,
      child: DefaultTextStyle(
        style: _textStyle,
        child: Scrollbar(
          controller: _verticalScroll,
          // Listen to the horizontal scroll view, nested 1 level past the nearest view
          notificationPredicate: (notification) => notification.depth == 1,
          child: Scrollbar(
            controller: _horizontalScroll,
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              controller: _horizontalScroll,
              child: SizedBox(
                width: _textWidth,
                child: ScrollConfiguration(
                  behavior: ScrollConfiguration.of(context).copyWith(scrollbars: false),

                  // Display text as a ListView of lines for better performance. Rather than laying out all the text,
                  // this only lays out and renders the visible lines.
                  child: ListView.builder(
                    padding: EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 12,
                    ),
                    shrinkWrap: true,
                    scrollDirection: Axis.vertical,
                    controller: _verticalScroll,
                    itemCount: _lines.length,
                    itemBuilder: (_, final int lineIndex) => Text(
                      _lines[lineIndex],
                      softWrap: false,
                    ),
                    prototypeItem: const Text(''),
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );

    // Add buttons to quickly scroll to top/bottom
    return Stack(
      clipBehavior: Clip.none,
      children: [
        scrollArea,
        Padding(
          padding: EdgeInsets.symmetric(vertical: 10),
          child: Stack(
            clipBehavior: Clip.none,
            children: [
              Align(
                alignment: Alignment.topCenter,
                child: _ScrollButton(
                  bScrollToStart: true,
                  scrollController: _verticalScroll,
                  resetNotifier: _textChangeNotifier,
                ),
              ),
              Align(
                alignment: Alignment.bottomCenter,
                child: _ScrollButton(
                  bScrollToStart: false,
                  scrollController: _verticalScroll,
                  resetNotifier: _textChangeNotifier,
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  /// Update information based on the text to display.
  void _updateText() async {
    if (widget.text == null) {
      _lines = const [];
      _textWidth = null;
      return;
    }

    _lines = widget.text!.split('\n').where((line) => line.length > 0).toList(growable: false);

    // Find the width of the longest line to determine our horizontal scroll area.
    // This only works if we use a monospace font, but is much cheaper than measuring the entire text for long logs.
    final String longestLine = _lines.fold(
      '',
      (String longest, String line) => line.length > longest.length ? line : longest,
    );

    final textPainter = TextPainter(
      textDirection: Directionality.of(context),
      text: TextSpan(
        text: longestLine,
        style: _textStyle,
      ),
    );

    textPainter.layout();

    _textWidth = textPainter.width;
  }
}

/// A button that lets the user instantly scroll to the start or end and hides itself when not relevant.
class _ScrollButton extends StatefulWidget {
  const _ScrollButton({
    Key? key,
    required this.scrollController,
    required this.bScrollToStart,
    this.resetNotifier,
  }) : super(key: key);

  /// Controller used to scroll the controlled view.
  final ScrollController scrollController;

  /// Whether this should scroll to the start (true) or end (false) of the scroll view.
  final bool bScrollToStart;

  /// If provided, update the button's visibility when this notifier fires (e.g. scroll bounds have changed).
  final ChangeNotifier? resetNotifier;

  @override
  State<_ScrollButton> createState() => _ScrollButtonState();
}

class _ScrollButtonState extends State<_ScrollButton> {
  bool _bIsVisible = false;
  bool _bIsAnimating = false;

  @override
  void initState() {
    super.initState();
    widget.scrollController.addListener(_updateVisibility);
    widget.resetNotifier?.addListener(_updateVisibility);

    WidgetsBinding.instance.addPostFrameCallback((_) {
      _updateVisibility();
    });
  }

  @override
  void dispose() {
    super.dispose();

    widget.scrollController.removeListener(_updateVisibility);
    widget.resetNotifier?.removeListener(_updateVisibility);
  }

  @override
  void didUpdateWidget(covariant _ScrollButton oldWidget) {
    super.didUpdateWidget(oldWidget);

    oldWidget.resetNotifier?.removeListener(_updateVisibility);
    oldWidget.scrollController.removeListener(_updateVisibility);

    widget.resetNotifier?.addListener(_updateVisibility);
    widget.scrollController.addListener(_updateVisibility);
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedOpacity(
      opacity: (_bIsVisible && !_bIsAnimating) ? 1 : 0,
      duration: Duration(milliseconds: 150),
      child: FloatingActionButton(
        child: AssetIcon(
          path: widget.bScrollToStart ? 'assets/images/icons/chevron_up.svg' : 'assets/images/icons/chevron_down.svg',
          size: 32,
        ),
        onPressed: _onPressed,
        heroTag: null,
      ),
    );
  }

  /// Update whether to show the button based on scroll position.
  void _updateVisibility() {
    bool bNewIsVisible = false;
    final double? scrollTarget = _getScrollTarget();
    if (scrollTarget == null) {
      bNewIsVisible = false;
    } else {
      bNewIsVisible = widget.scrollController.offset != scrollTarget;
    }

    if (bNewIsVisible != _bIsVisible) {
      setState(() {
        _bIsVisible = bNewIsVisible;
      });
    }
  }

  /// Called when the button is pressed.
  void _onPressed() {
    final double? scrollTarget = _getScrollTarget();
    if (scrollTarget != null) {
      setState(() {
        _bIsAnimating = true;
      });

      widget.scrollController
          .animateTo(
            scrollTarget,
            duration: Duration(milliseconds: 300),
            curve: Curves.easeInOutCubic,
          )
          .then((_) => setState(() {
                _bIsAnimating = false;
              }));
    }
  }

  /// Get the position to scroll to when the button is pressed.
  double? _getScrollTarget() {
    final scrollPosition = widget.scrollController.position;
    if (!scrollPosition.hasContentDimensions) {
      return null;
    }

    return widget.bScrollToStart ? scrollPosition.minScrollExtent : scrollPosition.maxScrollExtent;
  }
}

/// Contents to display while the log view is loading.
class _SettingsLogViewLoadingSpinner extends StatelessWidget {
  const _SettingsLogViewLoadingSpinner({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Center(
      child: const Center(
        child: SizedBox.square(
          dimension: 120,
          child: CircularProgressIndicator(
            strokeWidth: 8,
          ),
        ),
      ),
    );
  }
}
