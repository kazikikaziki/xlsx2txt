/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <inttypes.h>

namespace Kamilo {

typedef intptr_t KDialogId;


class CDialogImpl; // internal

/// 簡単なダイアログを構築して表示するためのクラス
///
/// @snippet KDialog.cpp test
class KDialog {
public:
	enum Flag {
		NEVER_SEND_QUIT = 1, // ウィンドウを閉じた時に PostQuitMessage しないようにする
	};
	typedef int Flags;

	/// KDialog でコントロールを配置する座標などを管理する
	struct LayoutCursor {
		/// ウィンドウ内側の余白
		int margin_x;
		int margin_y;

		/// コントロール外側の余白
		int padding_x;
		int padding_y;

		/// コントロール配置座標
		int next_x, next_y;

		/// コントロールの大きさ
		int next_w, next_h;

		/// 何もコントロールを配置しない行の高さ
		int line_min_h;

		/// 現在カーソルがある行の高さ
		int curr_line_h;

		/// コントロールの大きさを自動計算する
		bool auto_sizing;

		/// これまで配置したコントロール矩形のうち一番右下の座標
		int _max_x;
		int _max_y;

		/// これまで配置したコントロールをすべて収めるために必要なサイズ
		int content_w() const { return _max_x - margin_x; }	// _max_x は配置コントロールの右端を指しているため、そこから左側の余白を引く
		int content_h() const { return _max_y - margin_y; }
	};

	/// KDialog で使うためのコールバッククラス
	class Callback {
	public:
		virtual void on_dialog_close(KDialog *wnd) {}
		virtual void on_dialog_click(KDialog *wnd, KDialogId id) {}
	};

	KDialog();
	~KDialog();

	/// ウィンドウを作成
	/// @param text_u8 テキストの初期値 (Utf8)
	void create(const char *text_u8, Flags flags=0);

	/// ウィンドウを作成
	/// @param w       ウィンドウサイズ（幅）
	/// @param h       ウィンドウサイズ（高さ）
	/// @param text_u8 テキストの初期値 (Utf8)
	void create(int w, int h, const char *text_u8, Flags flags=0);

	/// ウィンドウを閉じる
	void close();

	/// ラベルを追加する
	/// 現在の「コントロール配置カーソル」位置にラベルを配置し、カーソルを次の配置予定位置に進める。
	/// @param id      このラベルに割り当てる識別番号。0 を指定すると自動割り振りになる。負の値は使えない
	/// @param text_u8 ラベル文字列 (Utf8)
	/// @return 追加したラベルの識別番号。自動付加した場合は負の値になる。失敗したら 0 を返す
	KDialogId addLabel(KDialogId id, const char *text_u8);

	/// チェックボックスを追加する
	/// @param id      このチェックボックスに割り当てる識別番号。0 を指定すると自動割り振りになる。負の値は使えない
	/// @param text_u8 チェックボックスのテキスト (Utf8)
	/// @param value   チェックの有無
	KDialogId addCheckBox(KDialogId id, const char *text_u8, bool value=false);

	/// ボタンを追加する
	/// @param id      このボタンに割り当てる識別番号。0 を指定すると自動割り振りになる。負の値は使えない
	/// @param text_u8 ボタンのテキスト (Utf8)
	KDialogId addButton(KDialogId id, const char *text_u8);

	/// コマンドボタンを追加する
	/// コマンドボタンは普通のボタンとは異なり、押すとウィンドウを閉じる
	/// @param id      このボタンに割り当てる識別番号。0 を指定すると自動割り振りになる。負の値は使えない
	/// @param text_u8 ボタンのテキスト (Utf8)
	KDialogId addCommandButton(KDialogId id, const char *text_u8);

	/// テキスト入力ボックスを追加する
	/// @param id      このテキストボックスに割り当てる識別番号。0 を指定すると自動割り振りになる。負の値は使えない
	/// @param text_u8 テキストの初期値 (Utf8)
	KDialogId addEdit(KDialogId id, const char *text_u8);

	/// テキスト編集ボックスを追加する（複数行、スクロールバー付き、読み取り専用設定可能）
	/// @param id       このテキストボックスに割り当てる識別番号。0 を指定すると自動割り振りになる。負の値は使えない
	/// @param text_u8  テキストの初期値 (Utf8)
	/// @param wrap     右端で折り返すなら true
	/// @param readonly 読み取り専用ならば true
	KDialogId addMemo(KDialogId id, const char *text_u8, bool wrap, bool readonly);

	/// コントロールが整数値の表示に対応している場合、その値を int で指定する
	/// @param id    コントロール識別番号
	/// @param value 設定する値
	void setInt(KDialogId id, int value);

	/// コントロールが文字列の表示に対応している場合、その文字列をセットする
	/// @param id      コントロール識別番号
	/// @param text_u8 設定する文字列 (Utf8)
	/// @code
	/// dialog.setString(id, u8"こんにちは");
	/// @endcode
	void setString(KDialogId id, const char *text_u8);

	/// コントロールが何らかの値の表示に対応している場合、その値を int で取得する
	/// @param id コントロール識別番号
	int getInt(KDialogId id);

	/// コントロールが何らかの値の表示に対応している場合、その値を utf8 で取得する
	/// @param      id       コントロール識別番号
	/// @param[out] text_u8  取得した文字列の格納先
	/// @param      maxbytes text_u8 に格納できる最大バイト数
	void getString(KDialogId id, char *text_u8, int maxbytes);

	/// 「コントロール配置カーソル」を取得する
	void getLayoutCursor(LayoutCursor *cur) const;

	/// 「コントロール配置カーソル」を設定する
	void setLayoutCursor(const LayoutCursor *cur);

	/// 「コントロール配置カーソル」を「改行」する
	void newLine();

	/// コントロールを視覚的に区切るための水平線を追加する
	void separate();

	/// 直前に追加したコントロールに対してフォーカスを設定する
	void setFocus();

	/// 直前に追加したコントロールに対してツールヒントを設定する
	/// ヒント文字列は UTF8 で指定する
	void setHint(const char *hint_u8);

	/// 直前に追加したコントロールに対して、幅を最大限取るように設定する
	void setExpand();

	/// ウィンドウサイズを自動決定する
	void setBestWindowSize();

	/// ウィンドウ処理を開始する。ウィンドウを閉じるまで制御が戻らないことに注意
	void run(Callback *cb);

	/// ウィンドウ処理を開始する。run とは異なり、毎回制御を戻す
	/// run_step() が真の間ループを呼び続けないといけない。
	/// ウィンドウハンドルを返す
	void * run_start(Callback *cb);
	bool run_step();

	/// ウィンドウハンドルを返す
	void * get_handle();

	/// 最後に発行されたコマンド値 (KDialogId) を返す
	/// @see addCommandButton
	int getCommand();

private:
	CDialogImpl *m_impl;
};

namespace Test {
void Test_dialog(const char *text_u8);
}

} // namespace
