# Change Log

## [1.2.0-fooboody.1]

### Added

- Elementumとしてのフォーク識別を導入し、アプリケーション名、ベンダー名、About画面、アイコン、プラグイン識別子、データディレクトリ、配布用バージョン表記をFooboody Studio向けに整備しました。
- システムトレイからのメインウィンドウ表示切替、ウィンドウ表示状態の保存、Elementum向けのウィンドウchromeを追加しました。
- `--profile-dir`によるプロファイル分離を追加し、異なるプロファイルを独立して同時起動できるようにしました。現在のデータディレクトリもUIから確認できます。
- WindowsでのLV2プラグイン検索パス管理を追加しました。
- 新規プラグインのMIDI入力自動接続、および近傍Audio／MIDIポートの再接続・切断・入替を追加しました。入替時のノード位置交換にも対応します。
- Audio接続に応じたノードの自動有効／無効化、ノードAltクリックによる有効／無効切替、複数選択ノードへの一括適用機能を追加しました。
- Graph Editorのノード、ケーブル、各種ポートおよび近傍判定の配色を設定ファイルの直接編集で変更できるようにしました。配色を再起動せず読み込む `Reload Colors from Conf File` も追加しました。
- ノード単位のDSP負荷メーターと、トータルDSP負荷、プロセス全体CPU使用率を区別した表示を追加しました。
- Graph Mixer、Consoleの配置を上部へ移動させる機能を追加しました。
- ノードの位置と現在の表示領域に応じてGraph Canvasを縮小する `Fit Canvas to Nodes` を追加しました。
- Graph Mixerフェーダーのつまみをダブルクリックして0.0 dBへ戻す操作を追加しました。
- PluginsメニューとノードのReplaceメニュー用に、メーカー名を頭文字別に整理する `Group Manufacturers by Initial` を追加しました。
- プラグイン検索にメーカー名を含める `Include Manufacturers`、検索結果・ノード・Patch Bay・プラグインウィンドウから詳細を開く `Details`、プラグインノードのお気に入り切替を追加しました。
- PresetsメニューからNodesフォルダを開く `Open Nodes Folder` を追加しました。
- Plugin Managerに `Rescan Selected` を追加しました。選択したプラグインだけを再スキャンできます。
- Plugin Managerリストに複数文字対応の検索ジャンプを追加しました。入力した先頭文字列に一致するプラグインへ移動します。
- Plugin Manager用のScanキーマッピングを追加しました。
- プラグインスキャンの安定性とカスタム性を上げるため、タイムアウトを30／60／90／180／300秒から選べるようにしました。また、10秒以上応答しないケースのみ自動再試行を1回かけるようにしました。
- プラグイン詳細をテキスト保存するコマンドラインオプションを追加しました。`--details`、`--details-json`または`--details-overwrite`、`--details-overwrite-json`に続けてプラグインファイルパスまたはフォルダパスを渡します。テキストファイルは`PluginDetails`フォルダに格納されます。
- Session保存、Import Graph、Export Graphの開始フォルダを、プロファイル内の適切な既定場所とユーザーの直近選択に基づいて扱うようにしました。
- プラグイン検索パネルのプラグイン項目にメーカー名ツールチップを追加しました。

### Changed

- Graph MixerとConsoleの高さを別々に保存し、各パネルの表示・非表示・上下入替・ビュー遷移の後にも独立した高さを維持するようにしました。
- Plugin ManagerまたはKey Mappingsを表示中はGraph Mixerを一時的に隠し、両画面ではGraph Mixer表示を変更できないようにしました。
- Plugin ManagerおよびKey MappingsのCloseは、履歴復帰ではなく常にGraph Editorへ戻るようにしました。
- Plugin ManagerのUnverified一覧は登録済み検索パスを省いた相対パス表示へ変更し、PluginsメニューはFavorites、Unverified、グループ化設定、メーカー一覧の順へ整理しました。
- 多くのメニュー表記をMenu Caseへ統一し、入力やダイアログを開かない項目から不要な三点リーダーを除去しました。
- 一部のメニュー名をより実際の動作に則した名称へ整理しました。
- Audioケーブルとホバー中ケーブルの描画優先度を上げ、ノード・ポート・ケーブルの既定配色を更新しました。
- 3桁HEX配色値を6桁へ正規化して保存し、設定から削除された配色VALUEは既定値で復元するようにしました。
- Graph Mixerの高さ設定を絶対値に変更しました。

### Removed

- Updater関連のUIコントロールを削除しました。

### Fixed

- `Scan Plugins at Startup`が有効な状態で、Scanボタンまたはキーマップから開始したスキャンをCancelするとクラッシュする場合がある問題を修正しました。
- プラグインスキャナのクラッシュ・ハング耐性を改善し、候補ごとのタイムアウト、ワーカー終了、ブラックリスト、明示的な再スキャンの整合性を修正しました。
- 機能していなかった`Mute Input Ports`機能を修正しました。
- Plugin Managerの検索パス編集ウィンドウの動作不具合を修正しました。
- ノード選択直後のGraph Editorフォーカスを徹底し、Deleteキーなどが安定して届くよう修正しました。

> このCHANGELOGは、Elementumフォーク固有の変更をまとめたものです。上流Elementの既存CHANGELOGは `CHANGELOG.md` を参照してください。
