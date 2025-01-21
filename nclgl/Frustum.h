#pragma once

#include "Plane.h"

class Matrix4;
class SceneNode;

class Frustum {
public:
	Frustum(void) {};
	~Frustum(void) {};
	
	void FromMatrix(const Matrix4 & mvp);
	bool InsideFrustum(SceneNode & n);
protected:
	Plane planes[6];
};