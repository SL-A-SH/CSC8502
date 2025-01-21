#include "UFO.h"

UFO::UFO(const Vector3& terrainSize) :
    terrainSize(terrainSize),
    position(terrainSize.x * 0.5f, 500.0f, terrainSize.z * 0.5f),
    rotation(0.0f),
    currentOrbitAngle(0.0f),
    orbitSpeed(0.2f),
    hoverOffset(0.0f),
    hoverSpeed(2.0f),
    orbitHeight(200.0f),
    orbitRadius(terrainSize.x * 0.25f),
    beamMesh(nullptr),
    beamActive(false),
    beamIntensity(0.0f),
    beamPulseTimer(0.0f)
{
    beamMesh = Mesh::GenerateLightBeam();
}

void UFO::Update(float dt) {
    if (!dt) return;

    // Update orbit angle
    currentOrbitAngle += orbitSpeed * dt;
    if (currentOrbitAngle >= 360.0f) {
        currentOrbitAngle -= 360.0f;
    }

    // Calculate new position
    float rad = DegToRad(currentOrbitAngle);
    position.x = terrainSize.x * 0.5f + cos(rad) * orbitRadius;
    position.z = terrainSize.z * 0.5f + sin(rad) * orbitRadius;

    // Add hovering motion
    hoverOffset = sin(hoverSpeed * currentOrbitAngle) * 20.0f;
    position.y = orbitHeight + hoverOffset;

    // Update rotation
    rotation += dt * 45.0f;

    // Only update beam effects if beam is active
    if (beamActive) {
        beamPulseTimer += dt;
        beamIntensity = (sin(beamPulseTimer * 2.0f) * 0.2f) + 0.8f;  // Pulsing between 0.6 and 1.0
    }
    else {
        beamPulseTimer = 0.0f;
        beamIntensity = 0.0f;
    }
}

void UFO::UpdatePosition(float dt) {
    // Calculate orbit position
    float rad = DegToRad(currentOrbitAngle);
    position.x = terrainSize.x * 0.5f + cos(rad) * orbitRadius;
    position.z = terrainSize.z * 0.5f + sin(rad) * orbitRadius;

    // Update hovering
    hoverOffset = sin(currentOrbitAngle * hoverSpeed) * 20.0f;
    position.y = orbitHeight + hoverOffset;

    // Update rotation
    rotation += dt * 45.0f;
    if (rotation >= 360.0f) {
        rotation -= 360.0f;
    }
}

Matrix4 UFO::GetModelMatrix() const {
    return Matrix4::Translation(position) *
        Matrix4::Rotation(rotation, Vector3(0, 1, 0)) *
        Matrix4::Scale(Vector3(2.0f, 2.0f, 2.0f));
}

void UFO::DrawBeam() const {
    if (beamMesh && beamActive) {
        beamMesh->Draw();
    }
}