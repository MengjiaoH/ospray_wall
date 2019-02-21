#pragma once 

#include <vector>
#include <random>

//ospray
#include <ospray/ospray_cpp/Geometry.h>
#include <ospray/ospray_cpp/Data.h>
#include <ospcommon/vec.h>
#include "bench/pico_bench/pico_bench.h"

struct Particle {
	osp::vec3f pos;
	float radius;
	int atom_type;

	Particle(float x, float y, float z, float radius, int type)
		: pos(osp::vec3f{x, y, z}), radius(radius), atom_type(type)
	{}
};

void constructParticles(std::vector<Particle> &atoms, std::vector<float> &colors, float box){
  std::random_device rd;
  std::mt19937 rng(rd());
  size_t max_type;

  std::uniform_real_distribution<float> pos(-box, box);
  std::uniform_real_distribution<float> radius(15, 30);
  std::uniform_int_distribution<int> type(0, 2);
  max_type = type.max() + 1;

  // Setup our particle data as a sphere geometry.
  // Each particle is an x,y,z center position + an atom type id, which
  // we'll use to apply different colors for the different atom types.
  for (size_t i = 0; i < 10000; ++i) {
    atoms.push_back(Particle(pos(rng), pos(rng), pos(rng), radius(rng), type(rng)));
  }

  std::uniform_real_distribution<float> rand_color(0.0, 1.0);
  for (size_t i = 0; i < max_type; ++i) {
    for (size_t j = 0; j < 3; ++j) {
      colors.push_back(rand_color(rng));
    }
  }
}

OSPGeometry commitParticles(std::vector<Particle> &atoms, std::vector<float> &colors){
    OSPData sphere_data = ospNewData(atoms.size() * sizeof(Particle), OSP_CHAR, atoms.data(), OSP_DATA_SHARED_BUFFER);
    ospCommit(sphere_data);
    OSPData color_data = ospNewData(colors.size(), OSP_FLOAT3, colors.data(), OSP_DATA_SHARED_BUFFER);
    ospCommit(color_data);
   
    OSPGeometry spheres = ospNewGeometry("spheres");
    ospSetData(spheres, "spheres", sphere_data);
    ospSetData(spheres, "color", color_data);
    // Tell OSPRay how big each particle is in the atoms array, and where
    // to find the color id. The offset to the center position of the sphere
    // defaults to 0.
    ospSet1i(spheres, "bytes_per_sphere", sizeof(Particle));
    ospSet1i(spheres, "offset_radius", sizeof(osp::vec3f));
    ospSet1i(spheres, "offset_colorID", sizeof(osp::vec3f) + sizeof(float));

    //OSPMaterial mat = ospNewMaterial(renderer, "OBJMaterial");
    //ospSetVec3f(mat, "Ks", osp::vec3f{0.5f, 0.5f, 0.5f});
    //ospSet1f(mat, "Ns", 15.f);
    //ospCommit(mat);
    //ospSetMaterial(spheres, mat);

    // Our sphere data is now finished being setup, so we commit it to tell
    // OSPRay all the object's parameters are updated.
    ospCommit(spheres);

    return spheres;
}

void constructVertices(std::vector<float> &vertices, std::vector<float> &colors, std::vector<int> &index){
    //random vertices positions

}
