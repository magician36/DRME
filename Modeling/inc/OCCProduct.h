#pragma once
#include "OCCInclude.h"

#include "OCCPart.h"

class OCCProduct
{
public:

	string sProductName;
	TopoDS_Shape pProductShape;

	vector<OCCPart*> pOCCParts;
};