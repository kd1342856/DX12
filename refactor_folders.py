# -*- coding: utf-8 -*-
# encoding: cp932
"""
DX12Framework フォルダ構成リファクタリングスクリプト

【移動内容】
Application:
  - Application/Object/Script/* → Application/Script/*
  - Application/Scene/SceneBase.h → Framework/Scene/SceneBase.h

Framework/Manager 解体:
  - Framework/Manager/Scene.h/.cpp         → Framework/Scene/Scene.h/.cpp
  - Framework/Manager/SceneManager.h/.cpp  → Framework/Scene/SceneManager.h/.cpp
  - Framework/Manager/CollisionManager.*   → Framework/Collision/CollisionManager.*
  - Framework/Manager/CollisionShape.*     → Framework/Collision/CollisionShape.*
  - Framework/Manager/CollisionSolver.*    → Framework/Collision/CollisionSolver.*
  - Framework/Manager/ResourceManager.*   → Framework/Resource/ResourceManager.*
  - Framework/Manager/PrefabManager.*      → Framework/Resource/PrefabManager.*
  - Framework/Manager/AudioManager.*       → Framework/Audio/AudioManager.*
  - Framework/Manager/AnimationManager.*   → Framework/Animation/AnimationManager.*
  - Framework/Manager/GameManager.*        → Framework/GameManager.*  (Manager直下へ)

Framework/DirectX 解体:
  - Framework/DirectX/Utility/* → Framework/Utility/*
  - Framework/DirectX/Window/*  → Framework/Window/*
  - Framework/DirectX/GDF/*     → Graphics/GDF/*

その他:
  - Framework/ImGuiEditor/Editor/* → Framework/Editor/*
  - Framework/System/JobSystem/*   → Framework/JobSystem/*
"""

import os
import re
import shutil

# ===== ルートパス =====
ROOT = r"c:\GitHub\DX12"
SRC  = os.path.join(ROOT, "Source")

# ===========================
# 移動マッピング定義
# (旧パス, 新パス) ← Sourceからの相対パス
# ===========================
MOVE_MAP = [
    # ---- Application: Object/Script → Script ----
    (r"Application\Object\Script\Player",  r"Application\Script\Player"),
    (r"Application\Object\Script\Ghost",   r"Application\Script\Ghost"),
    (r"Application\Object\Script\System",  r"Application\Script\System"),
    # ---- Application: SceneBase → Framework/Scene ----
    (r"Application\Scene\SceneBase.h",     r"Framework\Scene\SceneBase.h"),
    # ---- Framework/Manager 解体 ----
    (r"Framework\Manager\Scene.h",             r"Framework\Scene\Scene.h"),
    (r"Framework\Manager\Scene.cpp",           r"Framework\Scene\Scene.cpp"),
    (r"Framework\Manager\SceneManager.h",      r"Framework\Scene\SceneManager.h"),
    (r"Framework\Manager\SceneManager.cpp",    r"Framework\Scene\SceneManager.cpp"),
    (r"Framework\Manager\CollisionManager.h",  r"Framework\Collision\CollisionManager.h"),
    (r"Framework\Manager\CollisionManager.cpp",r"Framework\Collision\CollisionManager.cpp"),
    (r"Framework\Manager\CollisionShape.h",    r"Framework\Collision\CollisionShape.h"),
    (r"Framework\Manager\CollisionShape.cpp",  r"Framework\Collision\CollisionShape.cpp"),
    (r"Framework\Manager\CollisionSolver.h",   r"Framework\Collision\CollisionSolver.h"),
    (r"Framework\Manager\CollisionSolver.cpp", r"Framework\Collision\CollisionSolver.cpp"),
    (r"Framework\Manager\ResourceManager.h",   r"Framework\Resource\ResourceManager.h"),
    (r"Framework\Manager\ResourceManager.cpp", r"Framework\Resource\ResourceManager.cpp"),
    (r"Framework\Manager\PrefabManager.h",     r"Framework\Resource\PrefabManager.h"),
    (r"Framework\Manager\PrefabManager.cpp",   r"Framework\Resource\PrefabManager.cpp"),
    (r"Framework\Manager\AudioManager.h",      r"Framework\Audio\AudioManager.h"),
    (r"Framework\Manager\AudioManager.cpp",    r"Framework\Audio\AudioManager.cpp"),
    (r"Framework\Manager\AnimationManager.h",  r"Framework\Animation\AnimationManager.h"),
    (r"Framework\Manager\AnimationManager.cpp",r"Framework\Animation\AnimationManager.cpp"),
    (r"Framework\Manager\GameManager.h",       r"Framework\GameManager.h"),
    (r"Framework\Manager\GameManager.cpp",     r"Framework\GameManager.cpp"),
    # ---- Framework/DirectX 解体 ----
    (r"Framework\DirectX\Utility\ClassAssembly.h",   r"Framework\Utility\ClassAssembly.h"),
    (r"Framework\DirectX\Utility\ClassAssembly.cpp", r"Framework\Utility\ClassAssembly.cpp"),
    (r"Framework\DirectX\Utility\Input.h",           r"Framework\Utility\Input.h"),
    (r"Framework\DirectX\Utility\Input.cpp",         r"Framework\Utility\Input.cpp"),
    (r"Framework\DirectX\Utility\Logger.h",          r"Framework\Utility\Logger.h"),
    (r"Framework\DirectX\Utility\Logger.cpp",        r"Framework\Utility\Logger.cpp"),
    (r"Framework\DirectX\Utility\Random.h",          r"Framework\Utility\Random.h"),
    (r"Framework\DirectX\Utility\Time.h",            r"Framework\Utility\Time.h"),
    (r"Framework\DirectX\Utility\Time.cpp",          r"Framework\Utility\Time.cpp"),
    (r"Framework\DirectX\Utility\Utility.h",         r"Framework\Utility\Utility.h"),
    (r"Framework\DirectX\Utility\Utility.cpp",       r"Framework\Utility\Utility.cpp"),
    (r"Framework\DirectX\Window\Window.h",           r"Framework\Window\Window.h"),
    (r"Framework\DirectX\Window\Window.cpp",         r"Framework\Window\Window.cpp"),
    (r"Framework\DirectX\GDF\GDF.h",                 r"Graphics\GDF\GDF.h"),
    (r"Framework\DirectX\GDF\GDF.cpp",               r"Graphics\GDF\GDF.cpp"),
    # ---- ImGuiEditor → Editor ----
    (r"Framework\ImGuiEditor\Editor\Editor.h",       r"Framework\Editor\Editor.h"),
    (r"Framework\ImGuiEditor\Editor\Editor.cpp",     r"Framework\Editor\Editor.cpp"),
    # ---- System/JobSystem → JobSystem ----
    (r"Framework\System\JobSystem\JobSystem.h",      r"Framework\JobSystem\JobSystem.h"),
    (r"Framework\System\JobSystem\JobSystem.cpp",    r"Framework\JobSystem\JobSystem.cpp"),
]

# ===========================
# #include パス置換マッピング
# 旧相対パス文字列 → 新相対パス文字列（ダブルクォート内で使われるスラッシュ区切り）
# ===========================
INCLUDE_REPLACE_MAP = [
    # Application SceneBase
    (r"Application/Scene/SceneBase.h",               r"Framework/Scene/SceneBase.h"),
    # Manager → Scene
    (r"Manager/Scene.h",                             r"Scene/Scene.h"),
    (r"Manager/SceneManager.h",                      r"Scene/SceneManager.h"),
    # Manager → Collision
    (r"Manager/CollisionManager.h",                  r"Collision/CollisionManager.h"),
    (r"Manager/CollisionShape.h",                    r"Collision/CollisionShape.h"),
    (r"Manager/CollisionSolver.h",                   r"Collision/CollisionSolver.h"),
    # Manager → Resource
    (r"Manager/ResourceManager.h",                   r"Resource/ResourceManager.h"),
    (r"Manager/PrefabManager.h",                     r"Resource/PrefabManager.h"),
    # Manager → Audio
    (r"Manager/AudioManager.h",                      r"Audio/AudioManager.h"),
    # Manager → Animation
    (r"Manager/AnimationManager.h",                  r"Animation/AnimationManager.h"),
    # Manager → Framework直下 (GameManager)
    (r"Manager/GameManager.h",                       r"GameManager.h"),
    # DirectX/Utility → Utility
    (r"DirectX/Utility/ClassAssembly.h",             r"Utility/ClassAssembly.h"),
    (r"DirectX/Utility/Input.h",                     r"Utility/Input.h"),
    (r"DirectX/Utility/Logger.h",                    r"Utility/Logger.h"),
    (r"DirectX/Utility/Random.h",                    r"Utility/Random.h"),
    (r"DirectX/Utility/Time.h",                      r"Utility/Time.h"),
    (r"DirectX/Utility/Utility.h",                   r"Utility/Utility.h"),
    # DirectX/Window → Window
    (r"DirectX/Window/Window.h",                     r"Window/Window.h"),
    # DirectX/GDF → Graphics/GDF (参照元がFramework側から来るので絶対パス系も対応)
    (r"DirectX/GDF/GDF.h",                           r"../Graphics/GDF/GDF.h"),
    # ImGuiEditor → Editor
    (r"ImGuiEditor/Editor/Editor.h",                 r"Editor/Editor.h"),
    # System/JobSystem → JobSystem
    (r"System/JobSystem/JobSystem.h",                r"JobSystem/JobSystem.h"),
    # Application Script の include パス更新
    # 旧: Framework/ECS/Components/Data/NativeScript.h → 変更なし (移動しないので不要)
    # Object/Script → Script (Application内の相対パス)
    (r"Application/Object/Script/Player/Player.h",   r"Application/Script/Player/Player.h"),
    (r"Application/Object/Script/Ghost/GhostAI.h",   r"Application/Script/Ghost/GhostAI.h"),
    (r"Application/Object/Script/System/GameSequence.h", r"Application/Script/System/GameSequence.h"),
]

# vcxproj/filters 内のパス置換
# (旧 Windows パス文字列, 新 Windows パス文字列)
PROJ_REPLACE_MAP = [
    # Application Script
    (r"Source\Application\Object\Script\Player",      r"Source\Application\Script\Player"),
    (r"Source\Application\Object\Script\Ghost",       r"Source\Application\Script\Ghost"),
    (r"Source\Application\Object\Script\System",      r"Source\Application\Script\System"),
    # SceneBase
    (r"Source\Application\Scene\SceneBase.h",         r"Source\Framework\Scene\SceneBase.h"),
    # Manager → Scene
    (r"Source\Framework\Manager\Scene.h",             r"Source\Framework\Scene\Scene.h"),
    (r"Source\Framework\Manager\Scene.cpp",           r"Source\Framework\Scene\Scene.cpp"),
    (r"Source\Framework\Manager\SceneManager.h",      r"Source\Framework\Scene\SceneManager.h"),
    (r"Source\Framework\Manager\SceneManager.cpp",    r"Source\Framework\Scene\SceneManager.cpp"),
    # Manager → Collision
    (r"Source\Framework\Manager\CollisionManager.h",  r"Source\Framework\Collision\CollisionManager.h"),
    (r"Source\Framework\Manager\CollisionManager.cpp",r"Source\Framework\Collision\CollisionManager.cpp"),
    (r"Source\Framework\Manager\CollisionShape.h",    r"Source\Framework\Collision\CollisionShape.h"),
    (r"Source\Framework\Manager\CollisionShape.cpp",  r"Source\Framework\Collision\CollisionShape.cpp"),
    (r"Source\Framework\Manager\CollisionSolver.h",   r"Source\Framework\Collision\CollisionSolver.h"),
    (r"Source\Framework\Manager\CollisionSolver.cpp", r"Source\Framework\Collision\CollisionSolver.cpp"),
    # Manager → Resource
    (r"Source\Framework\Manager\ResourceManager.h",   r"Source\Framework\Resource\ResourceManager.h"),
    (r"Source\Framework\Manager\ResourceManager.cpp", r"Source\Framework\Resource\ResourceManager.cpp"),
    (r"Source\Framework\Manager\PrefabManager.h",     r"Source\Framework\Resource\PrefabManager.h"),
    (r"Source\Framework\Manager\PrefabManager.cpp",   r"Source\Framework\Resource\PrefabManager.cpp"),
    # Manager → Audio
    (r"Source\Framework\Manager\AudioManager.h",      r"Source\Framework\Audio\AudioManager.h"),
    (r"Source\Framework\Manager\AudioManager.cpp",    r"Source\Framework\Audio\AudioManager.cpp"),
    # Manager → Animation
    (r"Source\Framework\Manager\AnimationManager.h",  r"Source\Framework\Animation\AnimationManager.h"),
    (r"Source\Framework\Manager\AnimationManager.cpp",r"Source\Framework\Animation\AnimationManager.cpp"),
    # Manager → Framework直下
    (r"Source\Framework\Manager\GameManager.h",       r"Source\Framework\GameManager.h"),
    (r"Source\Framework\Manager\GameManager.cpp",     r"Source\Framework\GameManager.cpp"),
    # DirectX/Utility → Utility
    (r"Source\Framework\DirectX\Utility\ClassAssembly.h",   r"Source\Framework\Utility\ClassAssembly.h"),
    (r"Source\Framework\DirectX\Utility\ClassAssembly.cpp", r"Source\Framework\Utility\ClassAssembly.cpp"),
    (r"Source\Framework\DirectX\Utility\Input.h",           r"Source\Framework\Utility\Input.h"),
    (r"Source\Framework\DirectX\Utility\Input.cpp",         r"Source\Framework\Utility\Input.cpp"),
    (r"Source\Framework\DirectX\Utility\Logger.h",          r"Source\Framework\Utility\Logger.h"),
    (r"Source\Framework\DirectX\Utility\Logger.cpp",        r"Source\Framework\Utility\Logger.cpp"),
    (r"Source\Framework\DirectX\Utility\Random.h",          r"Source\Framework\Utility\Random.h"),
    (r"Source\Framework\DirectX\Utility\Time.h",            r"Source\Framework\Utility\Time.h"),
    (r"Source\Framework\DirectX\Utility\Time.cpp",          r"Source\Framework\Utility\Time.cpp"),
    (r"Source\Framework\DirectX\Utility\Utility.h",         r"Source\Framework\Utility\Utility.h"),
    (r"Source\Framework\DirectX\Utility\Utility.cpp",       r"Source\Framework\Utility\Utility.cpp"),
    # DirectX/Window → Window
    (r"Source\Framework\DirectX\Window\Window.h",           r"Source\Framework\Window\Window.h"),
    (r"Source\Framework\DirectX\Window\Window.cpp",         r"Source\Framework\Window\Window.cpp"),
    # DirectX/GDF → Graphics/GDF
    (r"Source\Framework\DirectX\GDF\GDF.h",                 r"Source\Graphics\GDF\GDF.h"),
    (r"Source\Framework\DirectX\GDF\GDF.cpp",               r"Source\Graphics\GDF\GDF.cpp"),
    # ImGuiEditor → Editor
    (r"Source\Framework\ImGuiEditor\Editor\Editor.h",       r"Source\Framework\Editor\Editor.h"),
    (r"Source\Framework\ImGuiEditor\Editor\Editor.cpp",     r"Source\Framework\Editor\Editor.cpp"),
    # System/JobSystem → JobSystem
    (r"Source\Framework\System\JobSystem\JobSystem.h",      r"Source\Framework\JobSystem\JobSystem.h"),
    (r"Source\Framework\System\JobSystem\JobSystem.cpp",    r"Source\Framework\JobSystem\JobSystem.cpp"),
    # filters の Filter タグ内も更新
    (r"Source\Application\Object\Script\Ghost",             r"Source\Application\Script\Ghost"),
    (r"Source\Application\Object\Script\System",            r"Source\Application\Script\System"),
    (r"Source\Application\Object\Script\Player",            r"Source\Application\Script\Player"),
    (r"Source\Framework\Manager",                           r"Source\Framework\Manager_DELETED"),  # 後で個別対応
    (r"Source\Framework\DirectX\GDF",                       r"Source\Graphics\GDF"),
    (r"Source\Framework\DirectX\WIndow",                    r"Source\Framework\Window"),
    (r"Source\Framework\DirectX\Utility",                   r"Source\Framework\Utility"),
    (r"Source\Framework\ImGuiEditor\Editor",                r"Source\Framework\Editor"),
    (r"Source\Framework\System\JobSystem",                  r"Source\Framework\JobSystem"),
]


def move_file(old_rel, new_rel):
    """ファイルまたはフォルダを移動する"""
    old_abs = os.path.join(SRC, old_rel)
    new_abs = os.path.join(SRC, new_rel)

    if not os.path.exists(old_abs):
        print(f"  [SKIP] 存在しない: {old_abs}")
        return

    # 新ディレクトリの作成
    new_dir = os.path.dirname(new_abs)
    if not os.path.exists(new_dir):
        os.makedirs(new_dir)
        print(f"  [MKDIR] {new_dir}")

    # フォルダの場合は中身ごとコピー移動
    if os.path.isdir(old_abs):
        if os.path.exists(new_abs):
            shutil.rmtree(new_abs)
        shutil.copytree(old_abs, new_abs)
        shutil.rmtree(old_abs)
        print(f"  [MOVE DIR] {old_rel} → {new_rel}")
    else:
        shutil.move(old_abs, new_abs)
        print(f"  [MOVE FILE] {old_rel} → {new_rel}")


def fix_includes_in_file(filepath):
    """ソースファイル内の #include パスを修正する"""
    try:
        with open(filepath, "r", encoding="cp932", errors="replace") as f:
            content = f.read()
    except Exception as e:
        print(f"  [READ ERR] {filepath}: {e}")
        return

    original = content
    for old_inc, new_inc in INCLUDE_REPLACE_MAP:
        # スラッシュとバックスラッシュ両方対応
        old_fwd = old_inc.replace("\\", "/")
        new_fwd = new_inc.replace("\\", "/")
        content = content.replace(f'"{old_fwd}"', f'"{new_fwd}"')
        # バックスラッシュ版
        old_bk = old_inc.replace("/", "\\")
        new_bk = new_inc.replace("/", "\\")
        content = content.replace(f'"{old_bk}"', f'"{new_bk}"')

    if content != original:
        with open(filepath, "w", encoding="cp932", errors="replace") as f:
            f.write(content)
        print(f"  [INCLUDE FIX] {os.path.relpath(filepath, SRC)}")


def fix_project_file(filepath):
    """vcxproj / vcxproj.filters のパスを修正する"""
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()
    except Exception as e:
        print(f"  [READ ERR] {filepath}: {e}")
        return

    original = content
    # PROJ_REPLACE_MAPは長い文字列を先に置換するために長さでソート
    sorted_map = sorted(PROJ_REPLACE_MAP, key=lambda x: len(x[0]), reverse=True)
    for old_p, new_p in sorted_map:
        content = content.replace(old_p, new_p)

    # filters タグの整理（Managerからの移動に合わせて修正）
    # Scene 系
    content = content.replace(
        r"Source\Framework\Manager_DELETED\Scene",
        r"Source\Framework\Scene"
    )
    content = content.replace(
        r"Source\Framework\Manager_DELETED\Collision",
        r"Source\Framework\Collision"
    )
    # 残った Manager_DELETED はそのままにしておき、後ほど手動確認

    if content != original:
        with open(filepath, "w", encoding="utf-8", errors="replace") as f:
            f.write(content)
        print(f"  [PROJ FIX] {os.path.basename(filepath)}")


def cleanup_empty_dirs():
    """移動後に空になったディレクトリを削除する"""
    dirs_to_check = [
        os.path.join(SRC, r"Framework\Manager"),
        os.path.join(SRC, r"Framework\DirectX"),
        os.path.join(SRC, r"Framework\ImGuiEditor"),
        os.path.join(SRC, r"Framework\System"),
        os.path.join(SRC, r"Application\Object"),
    ]
    for d in dirs_to_check:
        if os.path.exists(d) and not os.listdir(d):
            os.rmdir(d)
            print(f"  [RMDIR] {d}")
        elif os.path.exists(d):
            print(f"  [NOT EMPTY] 手動確認が必要: {d}")
            for item in os.listdir(d):
                print(f"    残存: {item}")


def main():
    print("=" * 60)
    print("DX12Framework フォルダ構成リファクタリング")
    print("=" * 60)

    # フェーズ1: ファイル・フォルダ移動
    print("\n[フェーズ1] ファイル移動")
    for old_rel, new_rel in MOVE_MAP:
        move_file(old_rel, new_rel)

    # フェーズ2: ソースファイルの #include 修正
    print("\n[フェーズ2] #include パス修正")
    for root, dirs, files in os.walk(SRC):
        # 除外ディレクトリ
        dirs[:] = [d for d in dirs if d not in [".git", "x64"]]
        for filename in files:
            if filename.endswith((".h", ".cpp")):
                fix_includes_in_file(os.path.join(root, filename))

    # フェーズ3: vcxproj / filters 更新
    print("\n[フェーズ3] プロジェクトファイル更新")
    fix_project_file(os.path.join(ROOT, "DX12Framework.vcxproj"))
    fix_project_file(os.path.join(ROOT, "DX12Framework.vcxproj.filters"))

    # フェーズ4: 空ディレクトリ削除
    print("\n[フェーズ4] 空ディレクトリ削除")
    cleanup_empty_dirs()

    print("\n" + "=" * 60)
    print("完了！Visual Studioでビルドして確認してください。")
    print("残存ファイルがある場合は手動対応が必要です。")
    print("=" * 60)


if __name__ == "__main__":
    main()
