#include "KXml.h"
#include "KInternal.h"
#include "KStream.h"

// Use tinyxml2.h
#define K_USE_TINYXML 1


#if K_USE_TINYXML
	// TinyXML-2
	// http://leethomason.github.io/tinyxml2/
	#include "tinyxml/tinyxml2.h"
#endif // K_USE_TINYXML



namespace Kamilo {


#pragma region KXmlElement
static bool _LoadTinyXml(const std::string &xlmtext_u8, const std::string &debug_name, tinyxml2::XMLDocument &tinyxml2_doc, std::string *p_errmsg) {
	if (xlmtext_u8.empty()) {
		*p_errmsg = K::str_sprintf(u8"E_XML: Xml document has no text: %s", debug_name.c_str());
		return false;
	}

	// TinyXML は SJIS に対応していない。
	// あらかじめUTF8に変換しておく。また、UTF8-BOM にも対応していないことに注意
	// 末尾が改行文字で終わるようにしておかないとパースエラーになる
	//
	// http://www.grinninglizard.com/tinyxmldocs/
	// For example, Japanese systems traditionally use SHIFT-JIS encoding.
	// Text encoded as SHIFT-JIS can not be read by TinyXML. 
	// A good text editor can import SHIFT-JIS and then save as UTF-8.
	tinyxml2::XMLError xerr = tinyxml2_doc.Parse(K::strSkipBom(xlmtext_u8.c_str()));
	if (xerr == tinyxml2::XML_SUCCESS) {
		*p_errmsg = "";
		return true;
	}

	const char *err_name = tinyxml2_doc.ErrorName();
	const char *err_msg = tinyxml2_doc.ErrorStr();
	if (tinyxml2_doc.ErrorLineNum() > 0) {
		// 行番号があるとき
		char s[1024];
		sprintf_s(s, sizeof(s),
			"E_XML: An error occurred while parsing XML document %s(%d): \n%s\n%s\n",
			debug_name.c_str(), tinyxml2_doc.ErrorLineNum(), err_name, err_msg
		);
		*p_errmsg = s;
		return false;

	} else {
		// 行番号がないとき（空文字列をパースしようとしたときなど）
		char s[1024];
		sprintf_s(s, sizeof(s),
			"E_XML: An error occurred while parsing XML document %s: \n%s\n%s\n",
			debug_name.c_str(), err_name, err_msg
		);
		*p_errmsg = s;
		return false;
	}
}

class CXNode: public KXmlElement {
	typedef std::pair<std::string, std::string> PairStrStr;
	std::string m_Tag;
	std::string m_Text;
	std::vector<PairStrStr> m_Attrs;
	std::vector<KXmlElement *> m_Nodes;
	int m_SourceLine;
public:
	CXNode() {
		m_SourceLine = 0;
	}
	virtual ~CXNode() {
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			(*it)->drop();
		}
	}
	virtual const char * getTag() const override {
		return m_Tag.c_str();
	}
	virtual void setTag(const char *tag) override {
		if (tag && tag[0]) {
			m_Tag = tag;
		}
	}
	virtual int getAttrCount() const override {
		return (int)m_Attrs.size();
	}
	virtual const char * getAttrName(int index) const override {
		return m_Attrs[index].first.c_str();
	}
	virtual const char * getAttrValue(int index) const override {
		return m_Attrs[index].second.c_str();
	}
	virtual void setAttrString(const char *name, const char *value) override {
		if (value == nullptr) { removeAttr(name); return; }
		int index = findAttrByName(name);
		if (index >= 0) {
			m_Attrs[index].second = value; // 既存の属性を変更
		} else {
			m_Attrs.push_back(PairStrStr(name, value)); // 属性を追加
		}
	}
	virtual void removeAttr(const char *name) override {
		int index = findAttrByName(name);
		if (index >= 0) {
			m_Attrs.erase(m_Attrs.begin() + index);
		}
	}
	virtual const char * getText(const char *def) const override {
		if (m_Text.empty()) {
			return def;
		} else {
			return m_Text.c_str();
		}
	}
	virtual void setText(const char *text) override {
		m_Text = text ? text : "";
	}
	virtual int getChildCount() const override {
		return (int)m_Nodes.size();
	}
	virtual const KXmlElement * getChild(int index) const override {
		if (0 <= index && index < (int)m_Nodes.size()) {
			return m_Nodes[index];
		}
		return nullptr;
	}
	virtual KXmlElement * getChild(int index) override {
		if (0 <= index && index < (int)m_Nodes.size()) {
			return m_Nodes[index];
		}
		return nullptr;
	}
	virtual KXmlElement * addChild(const char *tag, int pos) override {
		if (tag && tag[0]) {
			CXNode *newnode = new CXNode();
			newnode->m_Tag = tag;
			addChild(newnode, pos);
			return newnode;
		}
		return nullptr;
	}
	virtual void addChild(KXmlElement *newnode, int pos) override {
		if (newnode) {
			newnode->grab();
			if (pos >= 0) {
				m_Nodes.insert(m_Nodes.begin() + pos, newnode);
			} else {
				m_Nodes.push_back(newnode);
			}
		}
	}
	virtual void removeChild(int index) override {
		if (0 <= index && index < (int)m_Nodes.size()) {
			m_Nodes[index]->drop();
			m_Nodes.erase(m_Nodes.begin() + index);
		}
	}
	virtual int getLineNumber() const override {
		return m_SourceLine;
	}
	virtual KXmlElement * clone() const override {
		CXNode *result = new CXNode();
		result->m_Tag = this->m_Tag;
		result->m_SourceLine = this->m_SourceLine;
		result->m_Attrs = this->m_Attrs;
		result->m_Text = this->m_Text;
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			KXmlElement *elm = *it;
			result->m_Nodes.push_back(elm->clone());
		}
		return result;
	}

	static CXNode * createFromTinyXml(const tinyxml2::XMLNode *tiNode) {
		CXNode *result = new CXNode();

		const tinyxml2::XMLDocument *tiDoc = tiNode->ToDocument();
		const tinyxml2::XMLElement *tiElm = tiNode->ToElement();

		// 位置情報
		result->m_SourceLine = tiNode->GetLineNum();

		// タグ
		if (tiElm) {
			const char *tag = tiElm->Name();
			result->m_Tag = tag ? tag : "";
		}

		// 属性
		if (tiElm) {
			for (const tinyxml2::XMLAttribute *attr=tiElm->FirstAttribute(); attr!=nullptr; attr=attr->Next()) {
				const char *k = attr->Name();
				const char *v = attr->Value();
				result->m_Attrs.push_back(PairStrStr(k, v));
			}
		}

		// ノードテキスト
		if (tiElm) {
			const char *text = tiElm->GetText();
			if (text && text[0]) {
				result->m_Text = text;
			}
		}

		// 子ノード
		for (const tinyxml2::XMLElement *sub=tiNode->FirstChildElement(); sub!=nullptr; sub=sub->NextSiblingElement()) {
			CXNode *subnode = createFromTinyXml(sub);
			if (subnode) {
				result->m_Nodes.push_back(subnode);
			}
		}

		return result;
	}
};

KXmlElement * KXmlElement::create(const std::string &tag) {
	CXNode *xnode = new CXNode();
	xnode->setTag(tag.c_str());
	return xnode;
}
KXmlElement * KXmlElement::createFromFileName(const std::string &filename) {
	KXmlElement *elm = nullptr;
	KInputStream input = KInputStream::fromFileName(filename);
	elm = createFromStream(input, filename);
	return elm;
}
KXmlElement * KXmlElement::createFromStream(KInputStream &input, const std::string &filename) {
	if (!input.isOpen()) {
		K__ERROR("file is nullptr: %s", filename.c_str());
		return nullptr;
	}
	std::string bin = input.readBin();
	if (bin.empty()) {
		K__ERROR("file is empty: %s", filename.c_str());
		return nullptr;
	}
	return createFromString(bin.data(), filename);
}

KXmlElement * KXmlElement::createFromString(const std::string &xmlTextU8, const std::string &filename) {
	tinyxml2::XMLDocument tiDoc;
	std::string tiErrMsg;
	if (_LoadTinyXml(xmlTextU8, filename, tiDoc, &tiErrMsg)) {
		return CXNode::createFromTinyXml(&tiDoc);
	}
	K__ERROR("Failed to read xml: %s: %s", filename.c_str(), tiErrMsg.c_str());
	return nullptr;
}

bool KXmlElement::writeDoc(KOutputStream &output) const {
	if (!output.isOpen()) return false;
	output.writeString("<?xml version=\"1.0\" encoding=\"utf8\" ?>\n");
	int num = getChildCount();
	for (int i=0; i<num; i++) { // ルートではなくその子を書き出す
		if (!getChild(i)->write(output, 0)) {
			return false;
		}
	}
	return true;
}
bool KXmlElement::write(KOutputStream &output, int indent) const {
#if 1
	if (output.isOpen()) {
		std::string s = toString(indent);
		if (!s.empty()) {
			output.writeString(s);
			return true;
		}
	}
	return false;
#else
	if (!output.isOpen()) return false;
	if (indent < 0) indent = 0;
	char s[1024] = {0};

	// Tag
	sprintf_s(s, sizeof(s), "%*s<%s", indent*2, "", getTag());
	output.writeString(s);

	// Attr
	for (size_t i=0; i<getAttrCount(); i++) {
		const char *k = getAttrName(i);
		const char *v = getAttrValue(i);
		if (k && k[0] && v) {
			sprintf_s(s, sizeof(s), " %s=\"%s\"", k, v);
			output.writeString(s);
		}
	}

	// Text
	const char *text = getText();
	if (text && text[0]) {
		output.writeString("<![CDATA[");
		output.writeString(text);
		output.writeString("]]>");
	}

	// Sub nodes
	if (getChildCount() == 0) {
		output.writeString("/>\n");
	} else {
		if (text && text[0]) {
			// テキスト属性と子ノードは両立しない。
			K__ERROR(u8"Xml element cannot have both Text Element and Child Elements");

		} else {
			output.writeString(">\n");
			for (int i=0; i<getChildCount(); i++) {
				getChild(i)->write(output, indent+1);
			}
			sprintf_s(s, sizeof(s), "%*s</%s>\n", indent*2, "", getTag());
			output.writeString(s);
		}
	}
	return true;
#endif
}
std::string KXmlElement::toString(int indent) const {
	std::string s;

	if (indent < 0) indent = 0;
	// Tag
	s += K::str_sprintf("%*s<%s", indent*2, "", getTag());

	// Attr
	for (int i=0; i<getAttrCount(); i++) {
		const char *k = getAttrName(i);
		const char *v = getAttrValue(i);
		if (k && k[0] && v) {
			s += K::str_sprintf(" %s=\"%s\"", k, v);
		}
	}

	// Text or Sub nodes
	if (getChildCount() == 0) {

		// Text
		const char *text = getText();
		if (text && text[0]) {
			s += ">\n"; // タグ閉じる

			// CDATA部
			s += "<![CDATA[";
			s += text;
			s += "]]>\n";
			s += K::str_sprintf("%*s</%s>\n", indent*2, "", getTag());
		} else {
			s += "/>\n"; // タグ閉じる
		}

	} else {
		const char *text = getText();
		if (text && text[0]) {
			// テキスト属性と子ノードは両立しない。
			K__ERROR(u8"Xml element cannot have both Text Element and Child Elements");

		} else {
			s += ">\n";
			for (int i=0; i<getChildCount(); i++) {
				s += getChild(i)->toString(indent+1);
			}
			s += K::str_sprintf("%*s</%s>\n", indent*2, "", getTag());
		}
	}
	return s;
}
bool KXmlElement::hasTag(const char *tag) const {
	const char *mytag = getTag();
	return mytag && tag && strcmp(mytag, tag)==0;
}
int KXmlElement::indexOf(const KXmlElement *child) const {
	for (int i=0; i<getChildCount(); i++) {
		if (getChild(i) == child) {
			return i;
		}
	}
	return -1;
}
const char * KXmlElement::getAttrString(const char *name, const char *def) const {
	int i = findAttrByName(name);
	return i>=0 ? getAttrValue(i) : def;
}
float KXmlElement::getAttrFloat(const char *name, float def) const {
	const char *s = getAttrString(name);
	if (s == nullptr) return def;
	char *err = 0;
	float result = strtof(s, &err);
	if (err==s || err[0]) return def;
	return result;
}
int KXmlElement::getAttrInt(const char *name, int def) const {
	const char *s = getAttrString(name);
	if (s == nullptr) return def;
	char *err = 0;
	int result = strtol(s, &err, 0);
	if (err==s || *err) return def;
	return result;
}
int KXmlElement::findAttrByName(const char *name, int start) const {
	if (name && name[0]) {
		for (int i=start; i<getAttrCount(); i++) {
			if (strcmp(getAttrName(i), name) == 0) {
				return i;
			}
		}
	}
	return -1;
}
void KXmlElement::setAttrInt(const char *name, int value) {
	char s[32] = {0};
	sprintf_s(s, sizeof(s), "%d", value);
	setAttrString(name, s);
}
void KXmlElement::setAttrFloat(const char *name, float value) {
	char s[32] = {0};
	sprintf_s(s, sizeof(s), "%g", value);
	setAttrString(name, s);
}
bool KXmlElement::removeChild(KXmlElement *node, bool in_tree) {
	if (node == nullptr) return false;
	int n = getChildCount();

	// 子ノードから探す
	for (int i=0; i<n; i++) {
		KXmlElement *nd = getChild(i);
		if (nd == node) {
			removeChild(i);
			return true;
		}
	}

	// 子ツリーから探す
	if (in_tree) {
		for (int i=0; i<n; i++) {
			KXmlElement *nd = getChild(i);
			if (nd->removeChild(node, true)) {
				return true;
			}
		}
	}
	return false;
}
int KXmlElement::findChildByTag(const char *tag, int start) const {
	int n = getChildCount();
	for (int i=start; i<n; i++) {
		const KXmlElement *elm = getChild(i);
		if (elm->hasTag(tag)) {
			return i;
		}
	}
	return -1;
}
const KXmlElement * KXmlElement::findNode(const char *tag, const KXmlElement *start) const {
	return _findnode_const(tag, start);
}
KXmlElement * KXmlElement::findNode(const char *tag, const KXmlElement *start) {
	const KXmlElement *elm = _findnode_const(tag, start);
	return const_cast<KXmlElement*>(elm);
}
const KXmlElement * KXmlElement::_findnode_const(const char *tag, const KXmlElement *start) const {
	int index = 0;
	int n = getChildCount();
	if (start) {
		for (int i=0; i<n; i++) {
			if (getChild(i) == start) {
				index = i+1;
				break;
			}
		}
	}
	for (int i=index; i<n; i++) {
		const KXmlElement *elm = getChild(i);
		if (elm->hasTag(tag)) {
			return elm;
		}
	}
	return nullptr;
}
#pragma endregion






namespace Test {
void Test_xml() {
	KXmlElement *elm = KXmlElement::createFromString(
		"<node1 pi='314'>"
		"	<aaa/>"
		"	<bbb/>"
		"	<ccc/>"
		"</node1>"
		"<node2>"
		"	<ddd>Hello world!</ddd>"
		"</node2>"
		, ""
	);
	K__VERIFY(elm);
	K__VERIFY(elm->hasTag(""));
	K__VERIFY(elm->getChildCount() == 2);

	KXmlElement *node1 = elm->getChild(0);
	K__VERIFY(node1);
	K__VERIFY(node1 == elm->findNode("node1"));
	K__VERIFY(node1->hasTag("node1"));
	K__VERIFY(node1->getChildCount() == 3); // aaa, bbb, ccc
	K__VERIFY(node1->getAttrCount() == 1);
	K__VERIFY(strcmp(node1->getAttrName(0), "pi")==0);
	K__VERIFY(strcmp(node1->getAttrValue(0), "314")==0);
	K__VERIFY(node1->getAttrInt("pi") == 314);

	KXmlElement *node2 = elm->getChild(1);
	K__VERIFY(node2);
	K__VERIFY(node2 == elm->findNode("node2"));
	K__VERIFY(node2->hasTag("node2"));
	K__VERIFY(strcmp(node2->findNode("ddd")->getText(""), "Hello world!") == 0);

	elm->drop();
}
} // Test
#pragma endregion // KXmlElement



} // namespace
