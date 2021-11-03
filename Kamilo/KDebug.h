#if 0
/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <stdarg.h> // va_list
#include <string>

/// ソースコード中のファイル名と行番号をログに出力する
#define K_trace()            KLog::printTrace(__FUNCTION__, __LINE__)
#define K_tracef(fmt, ...)   KLog::printTracef(__FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#endif
