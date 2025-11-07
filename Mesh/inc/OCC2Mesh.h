
#pragma once

#include "gmsh.h"
#include<stdio.h>

#include <BRepLProp_SLProps.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Cone.hxx>
#include <gp_lin.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <Geom_Surface.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Brep_Tool.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepClass3d_SolidClassifier.hxx> 
#include <GeomLProp_CurveTool.hxx> 
#include <BRepLProp_CLProps.hxx> 
#include <BRepCheck_Edge.hxx> 
#include <BRepPrimAPI_MakeBox.hxx> 
#include <Poly_Triangulation.hxx>
#include <TopoDS.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeSphere.hxx> 
#include <gp_Sphere.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <TopoDS.hxx>
#include <STEPControl_StepModelType.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <Geom_Circle.hxx>
#include <GeomAPI_IntCS.hxx>
#include <IntTools_Context.hxx>
#include <GProp_GProps.hxx>
#include <BrepGProp.hxx>
#include <BRepIntCurveSurface_Inter.hxx>

#include <Bnd_Box.hxx>
#include <Bnd_OBB.hxx>
#include <BVH_Set.hxx>
#include <BRepBndLib.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_Wireframe.hxx>

#include <ShapeAnalysis.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GCPnts_UniformDeflection.hxx>
#include <TopTools_DataMapOfShapeInteger.hxx>
#include <TopTools.hxx>

#include <GeomAPI_ExtremaSurfaceSurface.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepExtrema_ExtFF.hxx>
#include <Poly_Triangulation.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <GeomProjLib.hxx>

#include <STEPControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <tuple>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <stack>
#include <deque>

#include <io.h>

using namespace std;
void  Shape2Mesh3d(TopoDS_Shape aPartShape);