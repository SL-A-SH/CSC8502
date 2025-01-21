#pragma once
#include "../../nclgl/Matrix4.h"
#include "../../nclgl/Vector3.h"
#include "../../nclgl/Mesh.h"

class UFO {
public:
    UFO(const Vector3& terrainSize);
    ~UFO() = default;

    void Update(float dt);
    Matrix4 GetModelMatrix() const;
    Vector3 GetPosition() const { return position; }

    void SetBeamActive(bool active) { beamActive = active; }
    bool IsBeamActive() const { return beamActive; }
    float GetBeamIntensity() const { return beamIntensity; }

    void DrawBeam() const;


    void DebugBeamStatus() const {
        std::cout << "Beam status: " << (beamActive ? "Active" : "Inactive") << std::endl;
        std::cout << "Beam intensity: " << beamIntensity << std::endl;
        std::cout << "Beam mesh exists: " << (beamMesh != nullptr) << std::endl;
    };


private:
    void UpdatePosition(float dt);

    Vector3 position;
    Vector3 terrainSize;
    float rotation;
    float currentOrbitAngle;
    float orbitSpeed;
    float hoverOffset;
    float hoverSpeed;
    float orbitHeight;
    float orbitRadius;

    Mesh* beamMesh;
    bool beamActive;
    float beamIntensity;
    float beamPulseTimer;
};
