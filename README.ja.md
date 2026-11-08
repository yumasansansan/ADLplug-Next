<!--
SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later

このファイルは README.md の日本語版で，ADLplug-Next の一部です．SPDX の行は，著作権者とライセンスを REUSE 仕様の機械可読な形式で示しています．GNU General Public License バージョン 3 またはそれ以降のバージョン（LICENSES/GPL-3.0-or-later.txt）の下にあります．
-->

# ADLplug-Next（Rust 版）

[English](README.md) | 日本語

このブランチは，ADLplug-Next を Rust で一から作り直しているものです．まだ使える段階にはありません．動くプラグインは main ブランチの C++ 版で，このブランチが main に代わるときに，2.2 という名前のブランチへ移ります．

目指しているもの:

- 二つのプラグイン，ADLplug-Next（OPL3）と OPNplug-Next（OPN2）を，自前のエミュレータのコア（音源チップをソフトウェアで再現する部分）で作り直します．コアは，チップの回路から起こしたエミュレーションとビット単位で同じ音を出し，C++ 版が使っていたライブラリは何も使いません．
- C++ 版と互換にします．プラグインとパラメータの ID を同じにして，ホストのプロジェクトが C++ 版の代わりにこの版で開けるようにし，C++ 版で保存したプロジェクトとバンクもそのまま読めるようにします．
- できるところはすべて Safe Rust（Rust のうち，メモリの安全をコンパイラが保証する部分）で書きます．そうできないところ（OS の関数の呼び出しと，アセンブリで書いた部分）は，安全でない操作を一つずつに分け，なぜ安全なのかを書き添えます．
- crates.io（Rust のパッケージの置き場）からは何も取らず，OS が提供するものは手で宣言します．
- x86-64 は x86-64-v2 以降の処理器を対象にし，AVX2 と AVX-512 は処理器にあるときに使います．AArch64 にも対応し（Apple の処理器と，AArch64 の Windows・Linux），SHA3（Arm のハッシュ SHA-3 のための命令の拡張で，コアはその命令の一つを使います）と SVE・SVE2（Arm の可変長ベクトルの拡張）は処理器にあるときに使います．

## ビルドとテスト

Rust の最新の安定版（rust-toolchain.toml で指定しています）と，LLVM（Clang，LLD，LLVM のツールの llvm-ar・llvm-nm）を PATH に置いたうえで，次を実行します．

```
cargo test --workspace --locked
```

`ci/rust.sh` は，CI と同じこと（rustfmt による書式の確認，警告をすべてエラーにした clippy，dev と release の両方のプロファイルでのテスト）を行います．

## ライセンス

ADLplug-Next は GNU General Public License バージョン 3 またはそれ以降のバージョン（LICENSES/GPL-3.0-or-later.txt）の下にあります．resources/ と docs/ にある画像・アイコン・フォント・マニュアルには，それぞれのライセンスがあり，REUSE.toml に記しています．
