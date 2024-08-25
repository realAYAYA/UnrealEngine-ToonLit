// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';
import 'package:webview_flutter/webview_flutter.dart';

import '../../../../models/engine_connection.dart';
import '../../../../utilities/net_utilities.dart';
import '../../../elements/spinner_overlay.dart';

final _log = Logger('WebBrowser');

class WebBrowserTab extends StatefulWidget {
  const WebBrowserTab({Key? key}) : super(key: key);

  static const String iconPath = 'packages/epic_common/assets/icons/web_browser.svg';

  static String getTitle(BuildContext context) => AppLocalizations.of(context)!.tabTitleWebBrowser;

  /// Browser tabs states that are currently active in the hierarchy.
  static final Set<_WebBrowserTabState> _activeBrowserTabs = {};

  @override
  State<WebBrowserTab> createState() => _WebBrowserTabState();

  /// Refresh all active web browser tabs.
  static void refreshAll() {
    for (_WebBrowserTabState tabState in _activeBrowserTabs) {
      tabState.refresh();
    }
  }
}

class _WebBrowserTabState extends State<WebBrowserTab> {
  late final EngineConnectionManager _connectionManager;

  /// Controls the state of the web view.
  WebViewController? _webViewController = null;

  /// The current URL to display in the address bar.
  String _currentUrl = '';

  /// The error message to display. If set, this will be shown instead of the web view.
  String? _errorMessage;

  /// Whether the page has finished its initial load.
  bool _bIsPageLoaded = false;

  @override
  void initState() {
    super.initState();

    _connectionManager = Provider.of<EngineConnectionManager>(context, listen: false);
    WidgetsBinding.instance.addPostFrameCallback((_) => _attemptToConnect());

    WebBrowserTab._activeBrowserTabs.add(this);
  }

  @override
  void activate() {
    super.activate();

    WebBrowserTab._activeBrowserTabs.add(this);
  }

  @override
  void deactivate() {
    WebBrowserTab._activeBrowserTabs.remove(this);

    super.deactivate();
  }

  @override
  void dispose() {
    WebBrowserTab._activeBrowserTabs.remove(this);

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_webViewController == null) {
      _initWebViewController();
    }

    late final Widget mainContent;

    if (_webViewController != null && _errorMessage == null) {
      // Show web view
      mainContent = Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _AddressBar(address: _currentUrl),
          const SizedBox(height: UnrealTheme.sectionMargin),
          Expanded(
            child: Stack(
              children: [
                if (!_bIsPageLoaded) const SpinnerOverlay(),
                Center(
                  child: WebViewWidget(
                    controller: _webViewController!,
                  ),
                ),
              ],
            ),
          ),
        ],
      );
    } else {
      // Show error message
      mainContent = Center(
        child: EmptyPlaceholder(
          message: _errorMessage!,
          button: _webViewController != null
              ? EpicWideButton(
                  text: AppLocalizations.of(context)!.webBrowserReconnectButtonLabel,
                  iconPath: 'packages/epic_common/assets/icons/refresh.svg',
                  onPressed: _attemptToConnect,
                )
              : null,
        ),
      );
    }

    return Padding(
      padding: const EdgeInsets.all(UnrealTheme.cardMargin),
      child: Card(child: mainContent),
    );
  }

  /// Refresh the currently loaded page, or attempt to reconnect if not currently showing a page.
  void refresh() {
    if (_webViewController == null) {
      return;
    }

    _attemptToConnect();
    _webViewController!.reload();
  }

  /// Try to set up the web view controller and set [_bIsWebViewAvailable] accordingly.
  void _initWebViewController() {
    if (WebViewPlatform.instance == null) {
      // Not supported on this platform
      _errorMessage = AppLocalizations.of(context)!.webBrowserUnsupportedMessage;
      return;
    }

    _webViewController = WebViewController()
      ..setBackgroundColor(Theme.of(context).colorScheme.surface)
      ..setJavaScriptMode(JavaScriptMode.unrestricted)
      ..setNavigationDelegate(
        NavigationDelegate(
          onPageFinished: _onPageFinished,
          onPageStarted: (String url) => setState(() {
            _currentUrl = url;
          }),
        ),
      );
  }

  /// Try the full connection process from scratch, including retrieving the URI from the engine.
  void _attemptToConnect() {
    if (_webViewController == null) {
      return;
    }

    _errorMessage = null;
    _bIsPageLoaded = false;

    if (_errorMessage == null) {
      _findBrowserURI().then((bool bSuccess) {
        if (!bSuccess) {
          setState(() {
            _errorMessage = AppLocalizations.of(context)!.webBrowserConnectionDataFailedMessage;
          });
        }
      });
    }
  }

  /// Request the port information for the web interface from the engine and construct the URL in [_initialUrl] if
  /// found.
  Future<bool> _findBrowserURI() async {
    final ConnectionData? connectionData = await _connectionManager.getLastConnectionData();

    if (connectionData == null) {
      _log.warning('');
      return false;
    }

    if (_connectionManager.connectionState != EngineConnectionState.connected) {
      return false;
    }

    final UnrealHttpRequest request;

    final bool bUseNewApi = _connectionManager.apiVersion!.bHasWebInterfacePortFunction;
    if (bUseNewApi) {
      request = UnrealHttpRequest(url: '/remote/object/call', verb: 'PUT', body: {
        'objectPath': '/Script/EpicStageApp.Default__StageAppFunctionLibrary',
        'functionName': 'GetRemoteControlWebInterfacePort',
        'generateTransaction': false,
      });
    } else {
      request = UnrealHttpRequest(url: '/remote/object/property', verb: 'PUT', body: {
        'objectPath': '/Script/RemoteControlCommon.Default__RemoteControlSettings',
        'propertyName': 'RemoteControlWebInterfacePort',
        'access': 'READ_ACCESS',
      });
    }

    final bool bSuccess = await _connectionManager.sendHttpRequest(request).then((UnrealHttpResponse response) {
      if (response.code != 200) {
        setState(() {
          _log.warning('Request for web interface port failed (error ${response.code}).');

          final String? responseError = response.body?['errorMessage'];
          if (responseError != null) {
            _errorMessage = _errorMessage! + '\n\n$responseError';
          }
        });
        return false;
      }

      final int? port = response.body?[bUseNewApi ? 'ReturnValue' : 'RemoteControlWebInterfacePort'];
      if (port == null) {
        setState(() {
          _log.warning('Response from engine did not contain a web interface port.');
        });
        return false;
      }

      setState(() {
        _webViewController!.loadRequest(Uri.http('${connectionData.websocketAddress.address}:$port'));
      });

      return true;
    }).onError((error, stackTrace) {
      setState(() {
        _log.warning('Failed to send request for web interface port.', error, stackTrace);
      });
      return false;
    });

    return bSuccess;
  }

  /// Called when the web view finishes loading a page.
  void _onPageFinished(String url) {
    if (!mounted) {
      return;
    }

    setState(() {
      _bIsPageLoaded = true;
    });
  }
}

/// Address bar displayed at the top of the web browser tab.
class _AddressBar extends StatelessWidget {
  const _AddressBar({Key? key, required this.address}) : super(key: key);

  final String address;

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 44,
      color: Theme.of(context).colorScheme.surfaceTint,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: UnrealTheme.cardMargin),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.center,
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Flexible(
              child: Padding(
                padding: const EdgeInsets.symmetric(vertical: UnrealTheme.cardMargin),
                child: Container(
                  constraints: BoxConstraints(minWidth: 300, maxWidth: 500),
                  decoration: BoxDecoration(
                    color: Theme.of(context).colorScheme.background,
                    borderRadius: BorderRadius.circular(UnrealTheme.outerCornerRadius),
                  ),
                  child: Center(
                    child: Padding(
                      padding: EdgeInsets.symmetric(horizontal: 8),
                      child: Text(
                        address,
                        style: Theme.of(context).textTheme.bodyMedium!.copyWith(
                              height: 1.2,
                              overflow: TextOverflow.fade,
                            ),
                        maxLines: 1,
                        softWrap: false,
                      ),
                    ),
                  ),
                ),
              ),
            ),
            const _RefreshWebBrowserButton(),
          ],
        ),
      ),
    );
  }
}

/// Button that refreshes any open browser tabs.
class _RefreshWebBrowserButton extends StatelessWidget {
  const _RefreshWebBrowserButton({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return EpicIconButton(
      iconPath: 'packages/epic_common/assets/icons/refresh.svg',
      tooltipMessage: AppLocalizations.of(context)!.webBrowserRefreshButtonTooltip,
      onPressed: WebBrowserTab.refreshAll,
      buttonSize: const Size(40, 40),
    );
  }
}
