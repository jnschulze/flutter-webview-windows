import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'dart:async';

import 'package:webview_windows/webview_windows.dart';

final navigatorKey = GlobalKey<NavigatorState>();

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(navigatorKey: navigatorKey, home: ExampleBrowser());
  }
}

class ExampleBrowser extends StatefulWidget {
  @override
  State<ExampleBrowser> createState() => _ExampleBrowser();
}

class _ExampleBrowser extends State<ExampleBrowser> {
  final _controller = WebviewController();
  final _textController = TextEditingController();
  bool _isWebviewSuspended = false;

  @override
  void initState() {
    super.initState();
    initPlatformState();
  }

//   Future<dynamic> executeScript(String script) async {
//     final scriptWraps = '''
// try {
//   JSON.stringify({result: $script});
// } catch(e) {
//   JSON.stringify({error: e.toString()});
// }
// ''';

//     final result = await _controller.executeScript(scriptWraps);
//     print('result = $result');

//     final m = jsonDecode(result) as Map<String, dynamic>;
//     if (m.containsKey('error')) throw Exception(m['error']); // JavascriptException
//     if (m.containsKey('result')) return m['result'];
//     return null; // undefined
//   }

  Future<void> initPlatformState() async {
    // Optionally initialize the webview environment using
    // a custom user data directory
    // and/or a custom browser executable directory
    // and/or custom chromium command line flags
    //await WebviewController.initializeEnvironment(
    //    additionalArguments: '--show-fps-counter');

    try {
      await _controller.initialize();
      final scriptID1 = await _controller.addScriptToExecuteOnDocumentCreated('window.hello1 = "world1"');
      final scriptID2 = await _controller.addScriptToExecuteOnDocumentCreated('window.hello2 = "world2"');
      print('==== $scriptID1 $scriptID2');
      await _controller.removeScriptToExecuteOnDocumentCreated(scriptID1!);

      _controller.url.listen((url) {
        _textController.text = url;
      });

      await _controller.setBackgroundColor(Colors.transparent);
      await _controller.setPopupWindowPolicy(WebviewPopupWindowPolicy.deny);
      await _controller.loadUrl('https://flutter.dev');

      print(await _controller.executeScript('window.hello1')); // ✔️ return null
      print(await _controller.executeScript('window.hello2')); // ✔️ return world2
      print(await _controller.executeScriptStatement('window.hello2')); // ✔️ return world2

      print(await _controller.executeScript('document.activeElement.getBoundingClientRect()')); // ❌ return null
      print(await _controller.executeScriptStatement('document.activeElement.getBoundingClientRect()'));  // ✔️ return {...}

      print(await _controller.executeScript('1 + 2')); // ✔️ return 3
      print(await _controller.executeScriptStatement('1 + 2')); // ✔️ return 3

      print(await _controller.executeScript('function printObj() { return {"a": 1, "arr": [2, 3, "string", 4.5]}; }'));
      print(await _controller.executeScript('printObj()')); // ✔️ return {"a": 1, "arr": [2, 3, "string", 4.5]}; }
      print(await _controller.executeScriptStatement('printObj()')); // ✔️ return {"a": 1, "arr": [2, 3, "string", 4.5]}; }

      print(await _controller.executeScript("document.querySelector('.container>h1').textContent")); // ✔️ 
      print(await _controller.executeScriptStatement("document.querySelector('.container>h1').textContent")); // ✔️ 

      if (!mounted) return;
      setState(() {});
    } on PlatformException catch (e) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        showDialog(
            context: context,
            builder: (_) => AlertDialog(
                  title: Text('Error'),
                  content: Column(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('Code: ${e.code}'),
                      Text('Message: ${e.message}'),
                    ],
                  ),
                  actions: [
                    TextButton(
                      child: Text('Continue'),
                      onPressed: () {
                        Navigator.of(context).pop();
                      },
                    )
                  ],
                ));
      });
    }
  }

  Widget compositeView() {
    if (!_controller.value.isInitialized) {
      return const Text(
        'Not Initialized',
        style: TextStyle(
          fontSize: 24.0,
          fontWeight: FontWeight.w900,
        ),
      );
    } else {
      return Padding(
        padding: EdgeInsets.all(20),
        child: Column(
          children: [
            Card(
              elevation: 0,
              child: Row(children: [
                Expanded(
                  child: TextField(
                    decoration: InputDecoration(
                      hintText: 'URL',
                      contentPadding: EdgeInsets.all(10.0),
                    ),
                    textAlignVertical: TextAlignVertical.center,
                    controller: _textController,
                    onSubmitted: (val) {
                      _controller.loadUrl(val);
                    },
                  ),
                ),
                IconButton(
                  icon: Icon(Icons.refresh),
                  splashRadius: 20,
                  onPressed: () {
                    _controller.reload();
                  },
                ),
                IconButton(
                  icon: Icon(Icons.developer_mode),
                  tooltip: 'Open DevTools',
                  splashRadius: 20,
                  onPressed: () {
                    _controller.openDevTools();
                  },
                )
              ]),
            ),
            Expanded(
                child: Card(
                    color: Colors.transparent,
                    elevation: 0,
                    clipBehavior: Clip.antiAliasWithSaveLayer,
                    child: Stack(
                      children: [
                        Webview(
                          _controller,
                          permissionRequested: _onPermissionRequested,
                        ),
                        Positioned(
                          top: 100,
                          left: 100,
                          child: OutlinedButton.icon(onPressed: (){}, icon: Icon(Icons.abc_outlined), label: Text('ABCD')),
                        ),
                        StreamBuilder<LoadingState>(
                            stream: _controller.loadingState,
                            builder: (context, snapshot) {
                              if (snapshot.hasData &&
                                  snapshot.data == LoadingState.loading) {
                                return LinearProgressIndicator();
                              } else {
                                return SizedBox();
                              }
                            }),
                      ],
                    ))),
          ],
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      floatingActionButton: FloatingActionButton(
        tooltip: _isWebviewSuspended ? 'Resume webview' : 'Suspend webview',
        onPressed: () async {
          if (_isWebviewSuspended) {
            await _controller.resume();
          } else {
            await _controller.suspend();
          }
          setState(() {
            _isWebviewSuspended = !_isWebviewSuspended;
          });
        },
        child: Icon(_isWebviewSuspended ? Icons.play_arrow : Icons.pause),
      ),
      appBar: AppBar(
          title: StreamBuilder<String>(
        stream: _controller.title,
        builder: (context, snapshot) {
          return Text(
              snapshot.hasData ? snapshot.data! : 'WebView (Windows) Example');
        },
      )),
      body: Center(
        child: compositeView(),
      ),
    );
  }

  Future<WebviewPermissionDecision> _onPermissionRequested(
      String url, WebviewPermissionKind kind, bool isUserInitiated) async {
    final decision = await showDialog<WebviewPermissionDecision>(
      context: navigatorKey.currentContext!,
      builder: (BuildContext context) => AlertDialog(
        title: const Text('WebView permission requested'),
        content: Text('WebView has requested permission \'$kind\''),
        actions: <Widget>[
          TextButton(
            onPressed: () =>
                Navigator.pop(context, WebviewPermissionDecision.deny),
            child: const Text('Deny'),
          ),
          TextButton(
            onPressed: () =>
                Navigator.pop(context, WebviewPermissionDecision.allow),
            child: const Text('Allow'),
          ),
        ],
      ),
    );

    return decision ?? WebviewPermissionDecision.none;
  }
}
