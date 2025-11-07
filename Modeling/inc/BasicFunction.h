
#pragma once

#include "OCCInclude.h"


TopoDS_Shape ImportStp(std::string sFileName);
void ExtractCylinderAxes(const TopoDS_Shape& shape, const std::string& savePath);
std::vector<gp_Ax1> LoadSavedAxes(const std::string& filePath);
