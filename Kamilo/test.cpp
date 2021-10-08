#include "KInternal.h"
#include "KImage.h"
#include "KMath.h"

namespace Kamilo {

namespace Test {
void Test_MakeAndSavePerlinImage(const char *name, int xwwap) {
	KImage img = KImage::createFromSize(256, 256, KColor32::BLACK);
	for (int i=0; i<256; i++) {
		img.setPixel(i, 127, KColor(1, 0, 0, 1));
	}
	for (int i=0; i<256; i++) {
		float p = KMath::perlin((float)i/256, 0, 0, xwwap, 0, 0);
		img.setPixel(i, 127 + (int)(126 * p), KColor::WHITE);
	}
	img.saveToFileName(name);
}
void Test_output_parlin_graph_images() {
	Test_MakeAndSavePerlinImage("_perlin_graph_0.png", 0);
	Test_MakeAndSavePerlinImage("_perlin_graph_1.png", 1);
	Test_MakeAndSavePerlinImage("_perlin_graph_2.png", 2);
	Test_MakeAndSavePerlinImage("_perlin_graph_4.png", 4);
	Test_MakeAndSavePerlinImage("_perlin_graph_8.png", 8);
	Test_MakeAndSavePerlinImage("_perlin_graph16.png", 16);
	Test_MakeAndSavePerlinImage("_perlin_graph32.png", 32);
}
void Test_output_parlin_texture_images() {
	KImageUtils::perlin(1024,   0,   0).saveToFileName("_perlin___0.png");
	KImageUtils::perlin(1024,   1,   1).saveToFileName("_perlin___1.png");
	KImageUtils::perlin(1024,   2,   2).saveToFileName("_perlin___2.png");
	KImageUtils::perlin(1024,   4,   4).saveToFileName("_perlin___4.png");
	KImageUtils::perlin(1024,   8,   8).saveToFileName("_perlin___8.png");
	KImageUtils::perlin(1024,  16,  16).saveToFileName("_perlin__16.png");
	KImageUtils::perlin(1024,  32,  32).saveToFileName("_perlin__32.png");
	KImageUtils::perlin(1024,  64,  64).saveToFileName("_perlin__64.png");
	KImageUtils::perlin(1024, 128, 128).saveToFileName("_perlin_128.png");
	KImageUtils::perlin(1024, 256, 256).saveToFileName("_perlin_256.png");
}

} // Test

} // namespace
