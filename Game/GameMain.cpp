#include <Kamilo.h>

using namespace Kamilo;


/// XLSX 内のテキストを抜き出す
static bool _ExportTextFromXLSX(const std::string &inpath, const std::string &outpath) {
	KInputStream input = KInputStream::fromFileName(inpath);
	if (!input.isOpen()) {
		KLogger::get()->emitf(KLogLv_ERROR, "Failed to open: %s", inpath.c_str());
		return false;
	}
	KExcelFile ef;
	ef.loadFromStream(input, inpath);
	if (ef.empty()) {
		KLogger::get()->emitf(KLogLv_ERROR, "Invalid excel file: %s", inpath.c_str());
		return false;
	}

	std::string text_u8 = ef.exportText();

	// 書き出す
	KOutputStream output = KOutputStream::fromFileName(outpath);
	if (!output.isOpen()) {
		KLogger::get()->emitf(KLogLv_ERROR, "Failed to open output: %s", outpath.c_str());
		return false;
	}
	output.write(text_u8.data(), text_u8.size());
	KLogger::get()->emitf(KLogLv_NONE, "Output: %s\n", outpath.c_str());
	return true;
}


void GameMain(const char *args_ansi) {
	std::string args_u8 = K::strAnsiToUtf8(args_ansi, "");
	K::sysSetCurrentDir(K::sysGetCurrentExecDir()); // exe の場所をカレントディレクトリにする
	K::win32_AllocConsole();

	KLogger::init();
	KLogger::get()->getEmitter()->setConsoleOutput(true);
	KLogger::get()->emitf(KLogLv_NONE, "***");
	KLogger::get()->emitf(KLogLv_NONE, "*** XLSX2TXT (%s) ***", __DATE__);
	KLogger::get()->emitf(KLogLv_NONE, "***");
	KLogger::get()->emitf(KLogLv_NONE, "");
	KLogger::get()->emitf(KLogLv_NONE, "ARGS=%s\n", args_u8.c_str());

	{
		auto tok = K::strSplitQuotedText(args_u8);
		for (int i=0; i<tok.size(); i++) {
			const std::string &in = tok[i];
			if (K::pathHasExtension(in, ".xlsx")) {
				KLogger::get()->emitf(KLogLv_NONE, "[%d] %s\n", i, tok[i].c_str());
				std::string out = in + ".xlsx2txt";
				_ExportTextFromXLSX(in, out);
			}
		}
	}

	printf("[Hit enter key]\n");
	getchar();

	KLogger::shutdown();
	K::win32_FreeConsole();
}
