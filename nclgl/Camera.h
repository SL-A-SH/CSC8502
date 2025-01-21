#pragma once

#include "Matrix4.h"
#include "Vector3.h"

class Camera {
public:
	Camera(void) {
		yaw = 0.0f;
		pitch = 0.0f;
	};

	Camera(float pitch, float yaw, Vector3 position) {
		this->pitch = pitch;
		this->yaw = yaw;
		this->position = position;
	}

	~Camera(void) {};

	void UpdateCamera(float dt = 1.0f);

	Matrix4 BuildViewMatrix();

	void LookAt(const Vector3& point);
	
	Vector3 GetPosition() const { return position; }
	void SetPosition(Vector3 val) { position = val; }
	
	float GetYaw() const { return yaw; }
	void SetYaw(float y) { yaw = y; }
	
	float GetPitch() const { return pitch; }
	void SetPitch(float p) { pitch = p; }

	Vector3 GetForward() const { return forward; }
	
protected:
	float yaw;
	float pitch;
	Vector3 position; // Set to 0,0,0 by Vector3 constructor
	Vector3 forward;
};