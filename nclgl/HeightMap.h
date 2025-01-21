#pragma once

#include <string>
#include "mesh.h"

class HeightMap : public Mesh {
public:
	HeightMap(const std::string& name);
	~HeightMap(void) {};

	Vector3 GetHeightmapSize() const { return heightmapSize; }
	float GetHeightAt(float x, float z) const;

protected:
	Vector3 heightmapSize;

	std::vector<float> heightData;
	int mapWidth;
	int mapHeight;
	float verticalScale;

protected:
	float SampleHeight(int x, int z) const;
	float BilinearInterpolation(float x, float z) const;
	Vector2 WorldToTerrain(float x, float z) const;
};