Tracy Profiler のクライアント(ゲームに組み込む側)のフォルダです。

  バージョン : v0.14.1 (2026-08-22, コミット 30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c)
  入手元     : https://github.com/wolfpld/tracy

リポジトリの public/ 以下(TracyClient.cpp、tracy/、client/、common/、libbacktrace/)と LICENSE をそのまま置いています。
ビルドするのは TracyClient.cpp だけです(ほかの .cpp はこのファイルから #include されます)。プリコンパイル済みヘッダーと
Pch.h の強制インクルードは、このファイルだけ外しています(windows.h より先に winsock2.h を読む必要があるため)。

プロジェクトの定義:
  TRACY_ENABLE         … Tracy を有効にする。外すと Tracy の呼び出しはすべて空になり、TracyClient.cpp も空になる
  TRACY_ON_DEMAND      … サーバーが接続している間だけ計測する。接続していない間はほぼコストなし
  TRACY_ONLY_LOCALHOST … 同じPCのサーバーからだけ接続を受け付ける(ファイアウォールの確認も出ない)

サーバー(計測を見る側)は、同じバージョンの Windows 版のリリース
  https://github.com/wolfpld/tracy/releases/tag/v0.14.1 の windows-0.14.1.zip
に含まれる tracy-profiler.exe を使います。バージョンが違うと接続できません。
更新するときは、public/ 以下を丸ごと差し替え、このファイルのバージョンを書き換えてください。
