/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once

namespace Kamilo {

/// 起動時パラメータの確認と、例外フックの設定を両方行う。
/// 
/// この関数は WinMain の最初に一度呼ぶだけで機能する。
/// 例外確認モードで起動されていた場合、K_ErrorCheck は必要な処理を行って true を返す。
/// この関数が true を返した場合はすぐに WinMain から抜けること。
///
/// 例外フックが呼ばれた場合、例外確認オプションを指定して自分自身を二重起動する。
/// 起動時パラメータ args を確認し、例外確認オプションで起動されている場合は
/// Windows のエラーログに登録されているであろう例外イベントをチェックし、その中身を表示する。
bool K_ErrorCheck(const char *args, const char *comment_u8, const char *include_log_file);

} // namespace
