import 'package:flutter/material.dart';
import 'dart:async';

import 'package:webview_windows/webview_windows.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  late WebviewController _controller;

  final textController = TextEditingController();

  @override
  void initState() {
    super.initState();

    initPlatformState();
  }

  Future<void> initPlatformState() async {
    _controller = WebviewController();

    await _controller.initialize();
    _controller.url.listen((url) {
      textController.text = url;
    });
    _controller.loadUrl('https://flutter.dev');

    if (!mounted) return;

    setState(() {});
  }

  Widget compositeView() {
    if (!_controller.value.isInitialized) {
      return const Text(
        'Not Initialized',
        style: TextStyle(
          color: Colors.white,
          fontSize: 24.0,
          fontWeight: FontWeight.w900,
        ),
      );
    } else {
      return Container(
        color: Colors.white,
        child: Column(
          children: [
            TextField(
              autofocus: true,
              controller: textController,
              onSubmitted: (val) {
                _controller.loadUrl(val);
              },
            ),
            StreamBuilder<LoadingState>(
                stream: _controller.loadingState,
                builder: (context, snapshot) {
                  if (snapshot.hasData &&
                      snapshot.data == LoadingState.Loading) {
                    return LinearProgressIndicator();
                  } else {
                    return Container();
                  }
                }),
            Expanded(child: Webview(_controller)),
          ],
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        backgroundColor: Colors.blue,
        appBar: AppBar(
          title: const Text('WebView (Windows) Example'),
        ),
        body: Center(
          child: compositeView(),
        ),
      ),
    );
  }
}
