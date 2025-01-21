#pragma once

#include <algorithm>
#include "../../nclgl/OGLRenderer.h"
#include "../../nclgl/Camera.h"
#include "../../nclgl/HeightMap.h"
#include "../../nclgl/Light.h"
#include "../nclgl/SceneNode.h"
#include "../nclgl/CubeRobot.h"
#include "UFO.h"

class Renderer : public OGLRenderer 
{
private:
    struct RainDrop {
        Vector3 position;
        Vector3 velocity;
        float life;
        float length;
    };

    struct LightningBolt {
        std::vector<Vector3> segments;
        float life;
        float intensity;
        bool active;
        Vector3 position;
    };

    struct LightningVertex {
        Vector3 position;
        float intensity;
    };

    struct CameraTrackPoint {
        Vector3 position;
        Vector3 lookAt;
        float timeToNext;
    };

    struct BloomBuffers {
        GLuint hdrFBO;
        GLuint colorBuffers[2];
        GLuint rboDepth;
        GLuint pingpongFBO[2];
        GLuint pingpongBuffers[2];
    };

public:
    Renderer(Window& parent);
    ~Renderer();

    void UpdateScene(float dt) override;
    void RenderScene() override;
    void RenderSplitScreen();

    void StartTransition()
    {
        if (!isTransitioning) {
            isTransitioning = true;
            transitionProgress = 0.0f;
        }
    }

    void ToggleTrackCamera() { isTrackCamera = !isTrackCamera; }
    void ToggleSplitScreen() { 
        isSplitScreen = !isSplitScreen;
        SetupViewport(isSplitScreen);
    }
    bool IsSplitScreen() const { return isSplitScreen; }

    void ToggleHDR() { hdrEnabled = !hdrEnabled; }
    void ToggleBloom() { bloomEnabled = !bloomEnabled; }

    void AdjustExposure(float dt, bool increase) {
        if (increase) {
            exposure = std::min(5.0f, exposure + EXPOSURE_CHANGE * dt);
        } else {
            exposure = std::max(0.1f, exposure - EXPOSURE_CHANGE * dt);
        }
    }

protected:
    SceneNode* root;
    Mesh* cube;

private:
    // Constants
    static constexpr float EXPOSURE_CHANGE = 0.1f;
    static constexpr float WATER_WAVE_SPEED = 2.0f;
    static constexpr float DAY_LENGTH = 120.0f;
    static constexpr float SUNRISE_TIME = 6.0f;
    static constexpr float SUNSET_TIME = 18.0f;
    static constexpr float HEIGHT_SMOOTH_SPEED = 2.0f;
    static constexpr int MAX_RAINDROPS = 10000;
    static constexpr int MAX_LIGHTNING_BOLTS = 3;
    static constexpr int SHADOW_SIZE = 4096;
    const float FPS_UPDATE_INTERVAL = 0.5f;

    // State variables
    Vector3 heightmapSize;
    float ambientStrength = 0.7f;
    float exposure = 1.0f;
    float waterHeight = 0.0f;
    float waterScale = 0.0f;
    float waterMovement = 0.0f;
    float timeOfDay = 1.0f;
    float currentTargetHeight = 0.0f;
    float smoothedHeight = 0.0f;
    float transitionProgress = 0.0f;
    const float transitionDuration = 5.0f;
    const float noiseScale = 8.0f;
    const float edgeSharpness = 2.0f;

    // State flags
    bool hdrEnabled = false;
    bool bloomEnabled = false;
    bool isTrackCamera = true;
    bool isSplitScreen = false;
    bool isTransitioning = false;
    bool isAncientMars = true;
    bool isRaining = true;
    bool isNight = false;

    // Meshes
    Mesh* skyQuad;
    Mesh* waterMesh;
    Mesh* ufoMesh;

    // Textures
    GLuint baseSoilTex;
    GLuint rockTex;
    GLuint sedimentTex;
    GLuint currentMarsRock;
    GLuint marsSkyboxAncientDay;
    GLuint marsSkyboxAncientNight;
    GLuint marsSkyboxCurrent;
    GLuint ufoTexture;

    // Pointers
    HeightMap* marsTerrain;
    Camera* camera;
    Camera* secondaryCamera;
    Light* sunLight;
    Light* lightningLight;
    UFO* ufo;

    // Shader pointers
    Shader* terrainShader;
    Shader* skyboxShader;
    Shader* waterShader;
    Shader* rainShader;
    Shader* lightningShader;
    Shader* shadowShader;
    Shader* transitionShader;
    Shader* splitScreenShader;
    Shader* ufoShader;
    Shader* ufoBeamShader;
    Shader* hdrShader;
    Shader* blurShader;
    Shader* sceneShader;

    // FPS
    float frameTimeSum;
    int frameCount;
    float currentFPS;
    float fpsUpdateTime;

    // Bloom
    BloomBuffers bloomBuffers;

    // Camera System
    // Splitscreen FBOs and textures
    GLuint splitScreenFBO[2];       // Two FBOs for rendering each scene
    GLuint splitScreenTextures[2];  // Textures to store each scene
    GLuint splitScreenDepth[2];     // Depth buffers for each scene

    // Camera
    std::vector<CameraTrackPoint> cameraTrack;
    int currentTrackPoint = 0;
    float trackTimer = 0.0f;

    // Rain system
    std::vector<RainDrop> raindrops;
    GLuint rainVAO, rainVBO;

    // Lightning system
    std::vector<LightningBolt> lightningBolts;
    float lightningTimer = 0.0f;
    float timeBetweenStrikes = 1.0f;
    float lightningProbability = 0.8f;
    Vector3 lastLightningPos;
    float lightningIlluminationIntensity;
    GLuint lightningVAO, lightningVBO;

    // Shadow mapping
    GLuint shadowFBO;
    GLuint shadowTex;
    Matrix4 shadowMatrix;

    // Transition System
    // Transition FBOs and textures
    GLuint transitionFBO[2];       // Two FBOs for rendering each scene
    GLuint transitionTextures[2];  // Textures to store each scene
    GLuint transitionDepth[2];     // Depth buffers for each scene

    // Initializers
    void SetupViewport(bool isSplitScreen);
    void InitializeTerrain();
    void InitializeCamera();
    void InitializeCameraTrack();
    void InitializeLighting();
    void InitializeSkyBox();
    void InitializeWater();
    void InitializeRainSystem();
    void InitializeLightningSystem();
    void InitializeShadowMapping();
    void InitializeUFO();
    void InitializeTransitionResources();
    void InitializeSplitScreenShader();
    void InitializeBloom();
    void InitializeRobot();

    // Methods for different render stages
    void RenderSceneContent();
    void RenderTransitionEffect();
    void RenderTextureToQuad(GLuint texture);

    // Draw methods
    void DrawTerrain();
    void DrawSkybox();
    void DrawWater();
    void DrawRain();
    void DrawLightning();
    void DrawShadowMap();
    void DrawUFO();
    void DrawUFOBeam();
    void DrawNode(SceneNode* n);

    Vector3 GenerateLightningPosition();
    void CreateLightningBolt();
    void GenerateShadowMatrix();

    // Update Methods
    void UpdateSunPosition(float dt);
    void UpdateWater(float dt);
    void UpdateRain(float dt);
    void UpdateLightning(float dt);
    void UpdateTransition(float dt);
    void UpdateCameraTrack(float dt);
    void UpdateFPSCounter(float dt);
    void UpdateBeamLight();
};