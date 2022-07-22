import 'package:flutter/material.dart';
import 'dart:async';

import 'package:webview_windows/webview_windows.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(MyApp());
}

class MyApp extends StatefulWidget {

  MyApp();

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  final List<String> _messages = [];
  final _controller = WebviewController(headless: true);
  final _textController = TextEditingController(text: 'https://flutter.dev');

  @override
  void initState() {
    super.initState();

    _controller.initialize().then(_onWebviewReady);
  }

  Future _onWebviewReady(_) async {
    if (_textController.text.isNotEmpty) {
      unawaited(_controller.loadUrl(_textController.text));
    }

    _controller.loadingState.listen((s) {
      setState(() {
        _messages.add('Loading state changed: $s');
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: StreamBuilder<String>(
            stream: _controller.title,
            builder: (context, snapshot) {
              var title = 'Current Page Title -> ';
              if (snapshot.hasData) title += snapshot.data!;
              return Text(title);
            },
          ),
        ),
        body:  Column(
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
              child: Container(
                padding: EdgeInsets.all(12),
                color: Colors.grey,
                child: ListView.builder(
                  itemCount: _messages.length,
                  itemBuilder: (context, index) => Text(_messages[index]),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
