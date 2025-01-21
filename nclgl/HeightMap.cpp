#include "HeightMap.h"
#include <iostream>

HeightMap::HeightMap(const std::string& name) {
    int iWidth, iHeight, iChans;
    unsigned char* data = SOIL_load_image(name.c_str(), &iWidth, &iHeight, &iChans, 1);
    if (!data) {
        std::cout << "Heightmap can't load file!\n";
        return;
    }

    mapWidth = iWidth;
    mapHeight = iHeight;
    verticalScale = 1.0f;  // Store the vertical scale factor

    // Store height data
    heightData.resize(mapWidth * mapHeight);
    for (int i = 0; i < mapWidth * mapHeight; ++i) {
        heightData[i] = data[i] * verticalScale;
    }

    numVertices = iWidth * iHeight;
    numIndices = (iWidth - 1) * (iHeight - 1) * 6;
    vertices = new Vector3[numVertices];
    textureCoords = new Vector2[numVertices];
    indices = new GLuint[numIndices];

    Vector3 vertexScale = Vector3(16.0f, 1.0f, 16.0f);
    Vector2 textureScale = Vector2(1 / 16.0f, 1 / 16.0f);

    // Original vertex generation code...
    for (int z = 0; z < iHeight; ++z) {
        for (int x = 0; x < iWidth; ++x) {
            int offset = (z * iWidth) + x;
            vertices[offset] = Vector3(x, data[offset], z) * vertexScale;
            textureCoords[offset] = Vector2(x, z) * textureScale;
        }
    }

    SOIL_free_image_data(data);

    // Original index generation code...
    int i = 0;
    for (int z = 0; z < iHeight - 1; ++z) {
        for (int x = 0; x < iWidth - 1; ++x) {
            int a = (z * (iWidth)) + x;
            int b = (z * (iWidth)) + (x + 1);
            int c = ((z + 1) * (iWidth)) + (x + 1);
            int d = ((z + 1) * (iWidth)) + x;

            indices[i++] = a;
            indices[i++] = c;
            indices[i++] = b;

            indices[i++] = c;
            indices[i++] = a;
            indices[i++] = d;
        }
    }

    GenerateNormals();
    GenerateTangents();
    BufferData();

    heightmapSize.x = vertexScale.x * (iWidth - 1);
    heightmapSize.y = vertexScale.y * 255.0f;
    heightmapSize.z = vertexScale.z * (iHeight - 1);
}

float HeightMap::GetHeightAt(float worldX, float worldZ) const {
    // Convert from world coordinates to terrain coordinates
    Vector2 terrainPos = WorldToTerrain(worldX, worldZ);

    // If outside the terrain bounds, return 0
    if (terrainPos.x < 0 || terrainPos.x >= mapWidth - 1 ||
        terrainPos.y < 0 || terrainPos.y >= mapHeight - 1) {
        return 0.0f;
    }

    return BilinearInterpolation(terrainPos.x, terrainPos.y);
}

Vector2 HeightMap::WorldToTerrain(float worldX, float worldZ) const {
    // Convert world coordinates to terrain coordinates
    float x = (worldX / heightmapSize.x) * (mapWidth - 1);
    float z = (worldZ / heightmapSize.z) * (mapHeight - 1);
    return Vector2(x, z);
}

float HeightMap::SampleHeight(int x, int z) const {
    // Clamp coordinates to valid range
    x = std::min(std::max(x, 0), mapWidth - 1);
    z = std::min(std::max(z, 0), mapHeight - 1);

    return heightData[z * mapWidth + x] * heightmapSize.y / 255.0f;
}

float HeightMap::BilinearInterpolation(float x, float z) const {
    // Get the integer coordinates
    int x1 = (int)floor(x);
    int x2 = x1 + 1;
    int z1 = (int)floor(z);
    int z2 = z1 + 1;

    // Get the fractional parts
    float fx = x - x1;
    float fz = z - z1;

    // Sample the heights at the four corners
    float h11 = SampleHeight(x1, z1);
    float h21 = SampleHeight(x2, z1);
    float h12 = SampleHeight(x1, z2);
    float h22 = SampleHeight(x2, z2);

    // Bilinear interpolation
    float h1 = h11 * (1 - fx) + h21 * fx;
    float h2 = h12 * (1 - fx) + h22 * fx;

    return h1 * (1 - fz) + h2 * fz;
}