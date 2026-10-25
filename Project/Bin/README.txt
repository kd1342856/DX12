実行に必須なDLLを置くフォルダです。

  dxcompiler.dll
  dxil.dll

DirectX Shader Compiler (https://github.com/microsoft/DirectXShaderCompiler) のリリースに含まれる
bin/x64 の2ファイルを、ここにコピーしてコミットしてください(.gitignore で例外にしてあります)。
起動時に AppPaths がこのフォルダを DLL の検索先に加え、ShaderCompiler がここから dxcompiler.dll を読み込みます。
