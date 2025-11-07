#include "OCC2Mesh.h"

using namespace gmsh;

void  Shape2Mesh3d(TopoDS_Shape aPartShape)
{
	double lc = 1;
	initialize();

	option::setNumber("General.Terminal", 1);
	//option::setNumber("General.Verbosity", 99);
	option::setNumber("Mesh.Algorithm", 1);
	option::setNumber("Mesh.Algorithm3D", 1);
	option::setNumber("Mesh.MinimumCirclePoints", 20);
	option::setNumber("Mesh.MeshSizeMax", 2);

	std::vector<std::pair<int, int>> dimTags;
	//model::occ::importShapes("E:\\CATIAProcess\\Test31.stp", dimTags);
	model::occ::importShapesNativePointer(&aPartShape, dimTags);
	model::occ::synchronize();

	//model::mesh::setSize(dimTags, 1);
	model::mesh::generate(3);
	//model::mesh::recombine();
	gmsh::write("E:\\Gmsh.stl");
	//测试测试
}