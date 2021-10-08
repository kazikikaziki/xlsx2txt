#include "Kamilo.h"

using namespace Kamilo;

namespace KTools {


/// Pac ファイル情報を書き出す
bool exportPacFileInfo(const std::string &pac_filename_u8) {
	if (K::pathHasExtension(pac_filename_u8, ".pac")) {
		std::string text;
		KPacFileReader pac = KPacFileReader::fromFileName(pac_filename_u8);
		int numfiles = pac.getCount();
		text += K::str_sprintf("<pacfile count='%d'>\n", numfiles);
		for (int i=0; i<numfiles; i++) {
			text += K::str_sprintf("\t%s\n", pac.getName(i).c_str());
		}
		text += K::str_sprintf("</pacfile>\n\n");

		// 書き出す
		std::string output_name = pac_filename_u8 + ".export.txt";
		KOutputStream file = KOutputStream::fromFileName(output_name);
		file.write(text.data(), text.size());
		return true;
	}
	return false;
}

/// XLSX 内のテキストを抜き出す
bool exportTextFromXLSX(const std::string &xlsx_filename_u8) {
	if (K::pathHasExtension(xlsx_filename_u8, ".xlsx")) {
		KInputStream input = KInputStream::fromFileName(xlsx_filename_u8);
		KExcelFile ef;
		ef.loadFromStream(input, xlsx_filename_u8);
		std::string text_u8 = ef.exportText();

		// 書き出す
		std::string output_name = xlsx_filename_u8 + ".export.txt";
		KOutputStream file = KOutputStream::fromFileName(output_name);
		file.write(text_u8.data(), text_u8.size());
		return true;
	}
	return false;
}

/// EDGE2 で出力した PAL ファイルを XML でエクスポートする
bool exportPalXmlFromEdge2(const std::string &edg_filename_u8) {
	if (K::pathHasExtension(edg_filename_u8, ".pal")) {

		// パレットを開く
		KEdgePalReader reader;
		reader.loadFromFileName(edg_filename_u8);

		// 画像を保存
		for (size_t i=0; i<reader.m_items.size(); i++) {
			KImage img = reader.m_items[i].exportImage(false);
			std::string png = img.saveToMemory();
			std::string output_name = K::str_sprintf("%s(%d).export.png", edg_filename_u8.c_str(), i);
			
			KOutputStream file = KOutputStream::fromFileName(output_name);
			file.write(png.data(), png.size());
		}

		// テキストとして保存
		std::string xml_u8 = reader.exportXml();

		// 書き出す
		std::string output_name = edg_filename_u8 + ".export.xml";
		KOutputStream file = KOutputStream::fromFileName(output_name);
		file.write(xml_u8.data(), xml_u8.size());
		return true;
	}
	return false;
}

/// EDGE2 ファイルの中身をエクスポートする
bool exportEdge2(const std::string &edg_filename_u8) {
	if (K::pathHasExtension(edg_filename_u8, ".edg")) {
		// バイナリをエクスポート
		{
			KInputStream infile = KInputStream::fromFileName(edg_filename_u8);
			std::string uzipped_bin = KEdgeRawReader::unzip(infile);

			// 書き出す
			std::string output_name = edg_filename_u8 + ".export.unzip";
			KOutputStream file = KOutputStream::fromFileName(output_name);
			file.write(uzipped_bin.data(), uzipped_bin.size());
		}

		// XMLにエクスポート
		KEdgeDocument doc;
		if (doc.loadFromFileName(edg_filename_u8)) {
			std::string xml_u8 = doc.expotyXml();

			// 書き出す
			std::string output_name = edg_filename_u8 + ".export.xml";
			KOutputStream file = KOutputStream::fromFileName(output_name);
			file.write(xml_u8.data(), xml_u8.size());
		}
		return true;
	}
	return false;
}


static bool _PackPngInDirEx(const std::string &dir, int cellsize, bool exclude_dup_cells) {
	bool packed = false;
	KImgPackW packW(cellsize, exclude_dup_cells);
	{
		auto files = K::fileGetListInDir(dir);
		for (size_t i=0; i<files.size(); i++) {
			const std::string &name = files[i];
			if (K::strStartsWith(name, "~")) continue;
			if (K::pathHasExtension(name, ".png")) {
				std::string path = K::pathJoin(dir, name);
				std::string bin;
				KLog::printText("%s", path.c_str());
				KInputStream file = KInputStream::fromFileName(path);
				if (file.isOpen()) {
					bin = file.readBin();
				}

				KImage img = KImage::createFromFileInMemory(bin);
				packW.addImage(img, NULL);
				packed = true;
			}
			if (K::pathHasExtension(name, ".edg")) {
				std::string path = K::pathJoin(dir, name);
				KLog::printText("%s", path.c_str());
				KEdgeDocument doc;
				doc.loadFromFileName(path);
				for (int p=0; p<doc.getPageCount(); p++) {
					if (KGameEdgeBuilder::isIgnorablePage(doc.getPage(p))) {
						continue;
					}
					for (int l=0; l<doc.getPage(p)->getLayerCount(); l++) {
						if (KGameEdgeBuilder::isIgnorableLayer(doc.getPage(p)->getLayer(l))) {
							continue;
						}
						KImage img = doc.exportSurfaceWithAlpha(p, l, -1, 0);
						packW.addImage(img, NULL);
						packed = true;
					}
				}
			}
		}
	}
	if (packed) {
		std::string output_png = K::pathJoin(dir, K::str_sprintf("~pack_image_cell%d_dup%d.png", cellsize, exclude_dup_cells));
		std::string output_xml = K::pathJoin(dir, K::str_sprintf("~pack_meta_cell%d_dup%d.xml", cellsize, exclude_dup_cells));
		packW.saveToFileNames(output_png, output_xml);
		return true;
	}
	return false;
}


bool packPngInDir(const std::string &dir) {
	if (!_PackPngInDirEx(dir, 16, false)) { // 重複除外なし
		return false;
	}
	if (!_PackPngInDirEx(dir, 16, true)) { // 重複除外アリ
		return false;
	}
	return true;
}


} // KTools
