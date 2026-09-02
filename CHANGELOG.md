# Changelog

本プロジェクトの変更履歴。書式は [Keep a Changelog](https://keepachangelog.com/ja/1.1.0/)、
バージョンは [Semantic Versioning](https://semver.org/lang/ja/)（MAJOR.MINOR.PATCH）に準拠します。
0.x 系はプレリリースで、非互換の変更があり得ます。

- **PATCH（0.0.x）**: バグ修正。
- **MINOR（0.x.0）**: 機能・コンテンツの追加（後方互換）。
- **MAJOR（x.0.0）**: 大きな節目（1.0.0＝一通り完成した最初のリリース）。

## [Unreleased]

## [0.0.1] - 2026-09-02
### 追加
- 最初の公開プロトタイプ（試作品）。全5面（ビスマルク級／エセックス級／HMSフッド／
  ネルソン&ロドニー／USSアイオワ級）、海イントロ＋戦艦戦、面別オリジナルBGM、エンディング。
- 自機耐久度の既定を **1（一撃死）** に設定（現状の難度だと簡単すぎるため）。
- MSX turboR 向け高速化群（VDPコマンドの間接発行、HMMMコピー、対空砲の可視カリング、
  炎の部分幅再描画、描画の単一走査化、戦闘中の海間引き 等）。
- ソフトリセット堅牢化（crt0で _DATA をゼロ化）。
- 設計ドキュメント（アルゴリズム解説／開発で苦労したこと）。

[Unreleased]: https://github.com/suzuki-black/BattleshipProtoR/compare/v0.0.1...HEAD
[0.0.1]: https://github.com/suzuki-black/BattleshipProtoR/releases/tag/v0.0.1
