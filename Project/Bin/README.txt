実行に必須なDLLを置くフォルダです。

  dxcompiler.dll
  dxil.dll

DirectX Shader Compiler (https://github.com/microsoft/DirectXShaderCompiler) のリリースに含まれる
bin/x64 の2ファイルをコミットしてあります(.gitignore で例外にしてあります)。更新するときは2つとも同じリリースのものに差し替えてください。
起動時に AppPaths がこのフォルダを DLL の検索先に加え、ShaderCompiler がここから dxcompiler.dll を読み込みます。
