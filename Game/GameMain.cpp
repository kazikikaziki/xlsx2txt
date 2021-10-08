#include <Kamilo.h>

using namespace Kamilo;


/// XLSX 内のテキストを抜き出す
bool ExportTextFromXLSX(const std::string &inpath, const std::string &outpath) {
	KInputStream input = KInputStream::fromFileName(inpath);
	if (!input.isOpen()) {
		KLog::printError("Failed to open: %s", inpath.c_str());
		return false;
	}
	KExcelFile ef;
	ef.loadFromStream(input, inpath);
	if (ef.empty()) {
		KLog::printError("Invalid excel file: %s", inpath.c_str());
		return false;
	}

	std::string text_u8 = ef.exportText();

	// 書き出す
	KOutputStream output = KOutputStream::fromFileName(outpath);
	if (!output.isOpen()) {
		KLog::printError("Failed to open output: %s", outpath.c_str());
		return false;
	}
	output.write(text_u8.data(), text_u8.size());
	KLog::printText("Output: %s\n", outpath.c_str());
	return true;
}


static std::string _Unquote(const std::string &s) {
	const char QUOTE = '"';
	std::string t = s;
	// 前後の " を削除する
	if (t.size() >= 2 && t.front()==QUOTE && t.back()==QUOTE) {
		t.erase(t.begin());
		t.pop_back();
	}
	// 連続する "" を１つの " に変換する
	K::strReplace(t, "\"\"", "\"");

	return t;
}

void GameMain(const char *args_ansi) {
	std::string args_u8 = K::strAnsiToUtf8(args_ansi, "");

	K::sysSetCurrentDir(K::sysGetCurrentExecDir()); // exe の場所をカレントディレクトリにする
	K::win32_AllocConsole();
	KLog::setOutputConsole(true);
	KLog::printText("***");
	KLog::printText("*** XLSX2TXT (%s) ***", __DATE__);
	KLog::printText("***");
	KLog::printText("");
	KLog::printDebug("ARGS=%s\n", args_u8.c_str());

	auto tok = K::strSplit(args_u8, " ");
	for (int i=0; i<tok.size(); i++) {
		const std::string &in = _Unquote(tok[i]);
		if (K::pathHasExtension(in, ".xlsx")) {
			KLog::printText("[%d] %s\n", i, tok[i].c_str());
			std::string out = in + ".xlsx2txt";
			ExportTextFromXLSX(in, out);
		}
	}

	printf("[Hit enter key]\n");
	getchar();
	K::win32_FreeConsole();
}
