Language: [English](README.en.md)

# Elementum

**Elementum は [Kushview Element](https://github.com/kushview/element) の非公式独立フォークです。**

VST3、CLAP、LV2に対応するオーディオプラグインホストです。スタンドアロンまたはプラグインとして使用できます。主な機能の詳細は本家をご確認ください。

このフォークでは、細かなバグ修正に加え、利便性を追求した独自の機能拡張を追加しています。動作検証はWindows環境ですが、クロスプラットフォームでの動作をできる限り維持する方針です。

## Elementとの関係

コアアーキテクチャや過去の開発履歴、既存機能の大部分はアップストリームであるElementに由来します。Elementum独自の変更点は本リポジトリで管理されており、アップストリームプロジェクトには適合しない、あるいは受け入れられない可能性があります。

そのため、問題を報告する際は、内容に応じて適切なプロジェクトへ報告してください。

| 内容 | 報告先 |
|---|---|
| 無改造のElementでも再現する挙動 | [Kushview Element の Issue トラッカー](https://gitlab.com/kushview/element/-/issues) |
| Elementum固有の機能・不具合・ドキュメントに関する問題 | [本リポジトリの Issue トラッカー](https://github.com/fooboody/elementum/issues) |

## 主な機能

Elementumは、AudioおよびMIDIノードを接続して再利用可能なグラフを構築するというElementのコアワークフローを踏襲しつつ、さらに様々な機能や修正を追加しています。コアアーキテクチャは改変していません。

| 分野 | 主な追加機能 |
|---|---|
| カスタマイズ | Graph Editor上の各種アイテムの配色変更、メニュー外観の変更（ダークモード） |
| グラフ編集 | 近接させたポートの自動接続/切断、Audioポートの一括差し替え、ノードの自動有効/無効、ノードのAltクリックによる有効/無効 |
| MIDIワークフロー | `MIDI IN DEVICE`、`MIDI IN`ノードから、新規追加したMIDI入力ノードへの自動配線 |
| リアルタイム負荷モニター | プロセスCPU使用率、DSPトータル負荷、ノードごとの負荷を表示 |
| プロファイル管理 | `--profile-dir`によるプロファイル単位での管理および多重起動 |

[CHANGELOG-FORK.md](CHANGELOG-FORK.md)

## 使用方法

スタンドアロンアプリケーションは、WindowsおよびLinuxビルドでは`elementum`、macOSアプリケーションバンドルでは`Elementum`という名称です。起動時にプロファイルディレクトリの指定がない限り、設定とプロファイルデータは標準のアプリケーションデータ格納場所に保存されます。

### プロファイル単位でのインスタンス起動

`--profile-dir` でプロファイルディレクトリを指定し、独立したデータプロファイルで起動することができます。

```text
elementum --profile-dir "C:\Elementum Profiles\Live"
```

同一のデータプロファイルで多重起動を行った場合は、既存のインスタンスのウィンドウが表示されます。異なるプロファイルであれば、複数インスタンスを同時に実行できます。

## ソースからビルドする場合

CMake 3.26以降が必要です。プラットフォームごとの詳細な手順は[docs/building.md](docs/building.md)を参照してください。

以下は、Visual Studioジェネレータを使用したWindows向けのRelease構成のビルド例です。

```bash
cmake -S . -B build -A x64 -DELEMENT_BUILD_PLUGINS=ON -DELEMENT_ENABLE_ASIO=ON
cmake --build build --config Release
```

このジェネレータの場合、ビルドされたスタンドアロン実行ファイルは通常以下の場所に生成されます。

```text
build\element_app_artefacts\Release\elementum.exe
```

出力ディレクトリの正確な場所は、選択したジェネレータやビルド構成によって異なります。LinuxおよびmacOSでのビルド手順については、[docs/building.md](docs/building.md)を参照してください。

## 設定とカスタマイズ

通常の設定項目の大部分はアプリケーションUIから変更できます。Graph Editorの配色まわりなど一部の高度なオプションは、プロファイルの`.conf`ファイルを直接編集する形をとっています。

## ライセンスとクレジット表記

Elementumは**GNU General Public License v3.0以降**の下で配布されています。本リポジトリおよび同梱物のライセンス全文は[LICENSES](LICENSES)にあります。ソースファイルは元の著作権表記とSPDX識別子を保持しており、Elementumでの変更によってアップストリームのクレジット表記が削除・上書きされることはありません。

アップストリームのドキュメント、ソースの開発履歴、商標については、それぞれの権利者に帰属します。Elementの作者・コントリビューターについては[AUTHORS.md](AUTHORS.md)に記載しています。

## プロジェクトの位置づけ

本リポジトリは独立して維持されているカスタマイズフォークです。本リポジトリのビルドは公式のKushviewリリースではなく、Kushview, LLCによるサポート対象ではありません。また、すべてのプラグインとの互換性を保証するものではありません。

Elementの情報および公式ダウンロードについては、[Kushview Element](https://kushview.net/element/)をご覧ください。
