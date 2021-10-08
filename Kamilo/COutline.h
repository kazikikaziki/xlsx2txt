#pragma once
#include "Kamilo.h"

class COutline: public Kamilo::KMaterialCallback {
public:
	Kamilo::KColor m_Color;
	COutline();
	void apply(Kamilo::KNode *node);
	void unapply(Kamilo::KNode *node);
	virtual void onMaterial_SetShaderVariable(const Kamilo::KMaterial *material) override;
};
