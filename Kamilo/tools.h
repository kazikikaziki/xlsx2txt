#pragma once
#include <string>

namespace KTools {

/// Pac ファイル情報を書き出す
bool exportPacFileInfo(const std::string &pac_filename_u8);

/// XLSX 内のテキストを抜き出す
bool exportTextFromXLSX(const std::string &xlsx_filename);

/// EDGE2 で出力した PAL ファイルを XML でエクスポートする
bool exportPalXmlFromEdge2(const std::string &edg_filename);

/// EDGE2 ファイルの中身をエクスポートする
bool exportEdge2(const std::string &edg_filename);

/// フォルダ内の .png を全てパックする
bool packPngInDir(const std::string &dir);

}
