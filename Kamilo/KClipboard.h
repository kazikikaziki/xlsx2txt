#pragma once
#include <string>
namespace Kamilo {

class KClipboard {
public:
	static bool setText(const std::string &text_u8);
	static bool getText(std::string &out_text_u8);
};

} // namespace
