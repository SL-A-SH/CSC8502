#include "Renderer.h"
#include <algorithm>
#include <iomanip>
#include "../nclgl/window.h"

Renderer::Renderer(Window& parent) : OGLRenderer(parent) 
{
    frameTimeSum = 0.0f;
    frameCount = 0;
    currentFPS = 0.0f;
    fpsUpdateTime = 0.0f;

    InitializeTerrain();
    InitializeSkyBox();
    InitializeLighting();
    InitializeWater();
    InitializeRainSystem();
    InitializeLightningSystem();
    InitializeShadowMapping();
    InitializeUFO();
    InitializeCameraTrack();
    InitializeTransitionResources();
    InitializeSplitScreenShader();
    InitializeBloom();
    InitializeRobot();

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    init = true;
}

Renderer::~Renderer(void) 
{
    delete terrainShader;
    delete skyboxShader;
    delete waterShader;
    delete rainShader;
    delete lightningShader;
    delete shadowShader;
    delete transitionShader;
    delete splitScreenShader;
    delete ufoShader;
    delete ufoBeamShader;
    delete hdrShader;
    delete blurShader;

    glDeleteTextures(1, &baseSoilTex);
    glDeleteTextures(1, &rockTex);
    glDeleteTextures(1, &sedimentTex);
    glDeleteTextures(1, &currentMarsRock);
    glDeleteTextures(1, &marsSkyboxAncientNight);
    glDeleteTextures(1, &marsSkyboxAncientDay);
    glDeleteTextures(1, &marsSkyboxCurrent);
    glDeleteTextures(1, &ufoTexture);
    glDeleteTextures(1, &shadowTex);

    delete skyQuad;
    delete waterMesh;
    delete ufoMesh;
    delete marsTerrain;
    delete camera;
    delete secondaryCamera;
    delete sunLight;
    delete lightningLight;
    delete ufo;

    for (int i = 0; i < 2; ++i) {
        glDeleteFramebuffers(1, &splitScreenFBO[i]);
        glDeleteTextures(1, &splitScreenTextures[i]);
        glDeleteRenderbuffers(1, &splitScreenDepth[i]);

        glDeleteFramebuffers(1, &transitionFBO[i]);
        glDeleteTextures(1, &transitionTextures[i]);
        glDeleteRenderbuffers(1, &transitionDepth[i]);
    }

    if (rainVAO) {
        glDeleteVertexArrays(1, &rainVAO);
        glDeleteBuffers(1, &rainVBO);
    }

    if (lightningVAO) {
        glDeleteVertexArrays(1, &lightningVAO);
        glDeleteBuffers(1, &lightningVBO);
    }

    rainVAO = rainVBO = lightningVAO = lightningVBO = 0;

    glDeleteFramebuffers(1, &shadowFBO);
    glDeleteFramebuffers(1, &bloomBuffers.hdrFBO);
    glDeleteFramebuffers(2, bloomBuffers.pingpongFBO);
    glDeleteTextures(2, bloomBuffers.colorBuffers);
    glDeleteTextures(2, bloomBuffers.pingpongBuffers);
    glDeleteRenderbuffers(1, &bloomBuffers.rboDepth);
}

void Renderer::SetupViewport(bool isSplitScreen)
{
    if (isSplitScreen) {
        projMatrix = Matrix4::Perspective(1.0f, 10000.0f, (float)width / (float)(height / 2), 45.0f);
    }
    else {
        projMatrix = Matrix4::Perspective(1.0f, 10000.0f, (float)width / (float)height, 45.0f);
        glViewport(0, 0, width, height);
    }
}

void Renderer::RenderScene()
{
    DrawShadowMap();

    projMatrix = Matrix4::Perspective(1.0f, 10000.0f, (float)width / (float)height, 45.0f);
    glViewport(0, 0, width, height);

    if (isTransitioning) {
        glBindFramebuffer(GL_FRAMEBUFFER, transitionFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        bool tempState = isAncientMars;
        isAncientMars = true;

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        DrawSkybox();
        DrawTerrain();
        DrawWater();
        if (isRaining && isNight) {
            DrawLightning();
            DrawRain();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, transitionFBO[1]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        isAncientMars = false;

        DrawSkybox();
        DrawTerrain();

        isAncientMars = tempState;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderTransitionEffect();
    }
    else if (isSplitScreen) {
        RenderSplitScreen();
    }
    else {
        if (!hdrEnabled) {
            // Normal rendering
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderSceneContent();
            return;
        }

        // HDR rendering
        glBindFramebuffer(GL_FRAMEBUFFER, bloomBuffers.hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderSceneContent();

        bool horizontal = false, first_iteration = false;
        if (bloomEnabled) {
            horizontal = true, first_iteration = true;
            BindShader(blurShader);

            for (unsigned int i = 0; i < 10; i++) {
                glBindFramebuffer(GL_FRAMEBUFFER, bloomBuffers.pingpongFBO[horizontal]);
                glUniform1i(glGetUniformLocation(blurShader->GetProgram(), "horizontal"), horizontal);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, first_iteration ? bloomBuffers.colorBuffers[1] :
                    bloomBuffers.pingpongBuffers[!horizontal]);

                modelMatrix.ToIdentity();
                viewMatrix.ToIdentity();
                projMatrix.ToIdentity();
                UpdateShaderMatrices();

                skyQuad->Draw();

                horizontal = !horizontal;
                if (first_iteration) first_iteration = false;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        BindShader(hdrShader);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloomBuffers.colorBuffers[0]);
        glUniform1i(glGetUniformLocation(hdrShader->GetProgram(), "scene"), 0);

        glActiveTexture(GL_TEXTURE1);
        if (bloomEnabled) {
            glBindTexture(GL_TEXTURE_2D, bloomBuffers.pingpongBuffers[!horizontal]);
        } else {
            glBindTexture(GL_TEXTURE_2D, bloomBuffers.colorBuffers[0]);
        }

        glUniform1i(glGetUniformLocation(hdrShader->GetProgram(), "bloomBlur"), 1);
        glUniform1f(glGetUniformLocation(hdrShader->GetProgram(), "exposure"), exposure);
        glUniform1i(glGetUniformLocation(hdrShader->GetProgram(), "bloomEnabled"), bloomEnabled);

        modelMatrix.ToIdentity();
        viewMatrix.ToIdentity();
        projMatrix.ToIdentity();
        UpdateShaderMatrices();

        glDisable(GL_DEPTH_TEST);
        skyQuad->Draw();
        glEnable(GL_DEPTH_TEST);
    }
}

void Renderer::RenderSceneContent() 
{
    DrawSkybox();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    DrawTerrain();

    BindShader(sceneShader);
    UpdateShaderMatrices();
    DrawNode(root);

    if (ufo) {
        ufo->SetBeamActive(isNight);
        DrawUFO();

        if (isNight) {
            DrawUFOBeam();
        }
    }

    if (isAncientMars) {
        DrawWater();
        if (isRaining && isNight)
        {
            DrawRain();
            DrawLightning();
        }
    }
}

void Renderer::RenderSplitScreen()
{
    // Store original camera and matrices
    Matrix4 originalProj = projMatrix;
    Matrix4 originalView = viewMatrix;
    Camera* originalCam = camera;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Top half (fixed camera)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, splitScreenFBO[0]);
        glViewport(0, 0, width, height / 2);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Fixed camera view setup
        camera = secondaryCamera;
        projMatrix = Matrix4::Perspective(1.0f, 10000.0f, (float)width / (float)(height / 2), 45.0f);
        viewMatrix = camera->BuildViewMatrix();

        DrawSkybox();
        DrawTerrain();

        if (ufo) {
            ufo->SetBeamActive(isNight);
            DrawUFO();

            if (isNight) {
                DrawUFOBeam();
            }
        }

        if (isAncientMars) {
            DrawWater();
            if (isRaining && isNight) {
                DrawLightning();
                DrawRain();
            }
        }
    }

    // Bottom half (free camera)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, splitScreenFBO[1]);
        glViewport(0, 0, width, height / 2);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Free camera view setup
        camera = originalCam;
        projMatrix = Matrix4::Perspective(1.0f, 10000.0f, (float)width / (float)(height / 2), 45.0f);
        viewMatrix = camera->BuildViewMatrix();

        // Draw scene
        DrawSkybox();
        DrawTerrain();

        if (ufo) {
            ufo->SetBeamActive(isNight);
            DrawUFO();

            if (isNight) {
                DrawUFOBeam();
            }
        }

        if (isAncientMars) {
            DrawWater();
            if (isRaining && isNight) {
                DrawLightning();
                DrawRain();
            }
        }
    }

    // Restore original camera and matrices
    camera = originalCam;
    projMatrix = originalProj;
    viewMatrix = originalView;

    // Render the split screen views to the main framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);

    // Draw top half
    glViewport(0, height / 2, width, height / 2);
    RenderTextureToQuad(splitScreenTextures[0]);

    // Draw bottom half
    glViewport(0, 0, width, height / 2);
    RenderTextureToQuad(splitScreenTextures[1]);

    // Restore states
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, width, height);
}

void Renderer::UpdateScene(float dt) 
{
    UpdateFPSCounter(dt);

    camera->UpdateCamera(dt);
    viewMatrix = camera->BuildViewMatrix();

    if (isTrackCamera) {
        UpdateCameraTrack(dt);
    }

    UpdateSunPosition(dt);
    GenerateShadowMatrix();

    if (secondaryCamera) {
        secondaryCamera->SetPosition(Vector3(heightmapSize.x * 0.2f, heightmapSize.y * 0.4f, heightmapSize.z * 0.5f));
        secondaryCamera->LookAt(Vector3(heightmapSize.x * 0.8f, heightmapSize.y * 0.1f, heightmapSize.z * 0.2f));
    }

    if (isTransitioning) {
        UpdateTransition(dt);
    }

    if (isAncientMars) {
        UpdateWater(dt);
        if (isRaining) {
            UpdateRain(dt);
            UpdateLightning(dt);
        }
    }

    if (ufo)
    {
        ufo->Update(dt);
    }

    root->Update(dt);
}

void Renderer::RenderTransitionEffect()
{
    BindShader(transitionShader);

    // Reset matrices for full screen quad
    viewMatrix.ToIdentity();
    modelMatrix.ToIdentity();
    projMatrix.ToIdentity();
    UpdateShaderMatrices();

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, transitionTextures[0]);  // Ancient Mars
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, transitionTextures[1]);  // Current Mars

    // Adjust progress based on transition direction
    float adjustedProgress = isAncientMars ? (1.0f - transitionProgress) : transitionProgress;

    glUniform1i(glGetUniformLocation(transitionShader->GetProgram(), "sourceScene"), 0);
    glUniform1i(glGetUniformLocation(transitionShader->GetProgram(), "targetScene"), 1);
    glUniform1f(glGetUniformLocation(transitionShader->GetProgram(), "transitionProgress"), adjustedProgress);
    glUniform2f(glGetUniformLocation(transitionShader->GetProgram(), "resolution"), (float)width, (float)height);
    glUniform1f(glGetUniformLocation(transitionShader->GetProgram(), "noiseScale"), noiseScale);
    glUniform1f(glGetUniformLocation(transitionShader->GetProgram(), "edgeSharpness"), edgeSharpness);

    // Render states
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // Draw full screen quad
    skyQuad->Draw();

    // Restore states
    glEnable(GL_DEPTH_TEST);
}

void Renderer::RenderTextureToQuad(GLuint texture)
{
    BindShader(splitScreenShader);

    // Reset matrices for the quad rendering
    modelMatrix.ToIdentity();
    viewMatrix.ToIdentity();
    projMatrix.ToIdentity();
    UpdateShaderMatrices();

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(splitScreenShader->GetProgram(), "diffuseTex"), 0);

    // Render states
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    skyQuad->Draw();

    // Restore states
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Renderer::InitializeTerrain()
{
    marsTerrain = new HeightMap(TEXTUREDIR"Mars/mars_terrain.png");
    if (!marsTerrain) {
        std::cout << "Failed to load terrain heightmap!" << std::endl;
        return;
    }
    heightmapSize = marsTerrain->GetHeightmapSize();

    terrainShader = new Shader("Mars/terrainVertex.glsl", "Mars/terrainFragment.glsl");
    if (!terrainShader->LoadSuccess()) {
        std::cout << "Failed to load terrain shader!" << std::endl;
        return;
    }

    baseSoilTex = SOIL_load_OGL_texture(
        TEXTUREDIR "Mars/ancient_mars_soil.jpg",
        SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS
    );

    rockTex = SOIL_load_OGL_texture(
        TEXTUREDIR "Mars/ancient_mars_rock.png",
        SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS
    );

    sedimentTex = SOIL_load_OGL_texture(
        TEXTUREDIR "Mars/ancient_mars_sediment.jpg",
        SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS
    );

    currentMarsRock = SOIL_load_OGL_texture(
        TEXTUREDIR "Mars/mars_rock.jpg",
        SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS
    );

    SetTextureRepeating(baseSoilTex, true);
    SetTextureRepeating(rockTex, true);
    SetTextureRepeating(sedimentTex, true);
    SetTextureRepeating(currentMarsRock, true);

    InitializeCamera();
}

void Renderer::InitializeCamera()
{
    camera = new Camera(-40, 270, Vector3());
    
    Vector3 camStartPos = Vector3(heightmapSize.x * 0.5f, heightmapSize.y * 1, heightmapSize.z * 0.5f);
    camera->SetPosition(camStartPos);
    smoothedHeight = camStartPos.y;
    currentTargetHeight = camStartPos.y;
}

void Renderer::InitializeCameraTrack()
{
    float heightOffset = heightmapSize.y * 0.3f;
    float minHeight = heightmapSize.y * 0.2f;

    std::vector<CameraTrackPoint> points = {
        // Look at sky
        {
            Vector3(heightmapSize.x * 0.5f, heightmapSize.y * 1.2f, heightmapSize.z * 0.5f),
            Vector3(heightmapSize.x * 0.5f, 0, heightmapSize.z * 0.5f),
            8.0f
        },
        // Spiral down
        {
            Vector3(heightmapSize.x * 0.7f, std::max(heightmapSize.y * 0.8f, minHeight), heightmapSize.z * 0.7f),
            Vector3(heightmapSize.x * 0.5f, heightmapSize.y * 0.1f, heightmapSize.z * 0.5f),
            6.0f
        },
        // Look at UFO
        {
            Vector3(5129.45f, 190.62f, 4173.3f),
            Vector3(5229.22f, 190.62f, 4180.07f),
            8.0f
        },
        // Look at lake and UFO from distance
        {
            Vector3(4104.12f, 174.294f, 3428.54f),
            Vector3(4202.71f, 174.294f, 3411.81f),
            8.0f
        },
        // View from above
        {
            Vector3(4171.74f, 473.566f, 2802.38f),
            Vector3(4248.42f, 473.566f, 2866.57f),
            8.0f
        },
        // Turn around and look at mountain
        {
            Vector3(4259.22f, 382.669f, 3183.46f),
            Vector3(4176.75f, 382.669f, 3126.91f),
            8.0f
        },
        // Look at UFO and close up of lake
        {
            Vector3(5789.38f, 91.026f, 5599.52f),
            Vector3(5799.63f, 91.026f, 5500.04f),
            8.0f
        },
        // Look at UFO from above and back to the sky for a loop
        {
            Vector3(6537.64f, 654.651f, 5503.53f),
            Vector3(6475.33f, 654.651f, 5425.32f),
            8.0f
        },
    };

    cameraTrack = points;

    // Secondary camera for split screen
    secondaryCamera = new Camera(-45, 180, Vector3());
    secondaryCamera->SetPosition(cameraTrack[0].position);

    // Initialize split screen FBOs
    for (int i = 0; i < 2; ++i) {
        glGenFramebuffers(1, &splitScreenFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, splitScreenFBO[i]);

        glGenTextures(1, &splitScreenTextures[i]);
        glBindTexture(GL_TEXTURE_2D, splitScreenTextures[i]);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height / 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        GLint texWidth, texHeight;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, splitScreenTextures[i], 0);

        glGenRenderbuffers(1, &splitScreenDepth[i]);
        glBindRenderbuffer(GL_RENDERBUFFER, splitScreenDepth[i]);

        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height / 2);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, splitScreenDepth[i]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::InitializeLighting() 
{
    sunLight = new Light(
        Vector3(-3000.0f, 2000.0f, -3000.0f),
        Vector4(1.5f, 1.4f, 1.3f, 1.0f),
        12000.0f
    );
}

void Renderer::InitializeSkyBox()
{
    skyQuad = Mesh::GenerateQuad();

    skyboxShader = new Shader("Mars/skyboxVertex.glsl", "Mars/skyboxFragment.glsl");
    if (!skyboxShader->LoadSuccess()) {
        std::cout << "Failed to load skybox shader!" << std::endl;
        return;
    }

    marsSkyboxAncientDay = SOIL_load_OGL_cubemap(
        TEXTUREDIR"Mars/stormydays_ft.png",
        TEXTUREDIR"Mars/stormydays_bk.png",
        TEXTUREDIR"Mars/stormydays_up.png",
        TEXTUREDIR"Mars/stormydays_dn.png",
        TEXTUREDIR"Mars/stormydays_rt.png",
        TEXTUREDIR"Mars/stormydays_lf.png",
        SOIL_LOAD_RGB,
        SOIL_CREATE_NEW_ID,
        0
    );

    marsSkyboxAncientNight = SOIL_load_OGL_cubemap(
        TEXTUREDIR"Mars/space_ft.png",
        TEXTUREDIR"Mars/space_bk.png",
        TEXTUREDIR"Mars/space_up.png",
        TEXTUREDIR"Mars/space_dn.png",
        TEXTUREDIR"Mars/space_rt.png",
        TEXTUREDIR"Mars/space_lf.png",
        SOIL_LOAD_RGB,
        SOIL_CREATE_NEW_ID,
        0
    );

    marsSkyboxCurrent = SOIL_load_OGL_cubemap(
        TEXTUREDIR"Mars/arid_ft.jpg",
        TEXTUREDIR"Mars/arid_bk.jpg",
        TEXTUREDIR"Mars/arid_up.jpg",
        TEXTUREDIR"Mars/arid_dn.jpg",
        TEXTUREDIR"Mars/arid_rt.jpg",
        TEXTUREDIR"Mars/arid_lf.jpg",
        SOIL_LOAD_RGB,
        SOIL_CREATE_NEW_ID,
        0
    );
}

void Renderer::InitializeWater()
{
    waterMesh = Mesh::GenerateHorizontalQuad();
    waterShader = new Shader("Mars/waterVertex.glsl", "Mars/waterFragment.glsl");
    if (!waterShader->LoadSuccess()) {
        std::cout << "Failed to load water shader!" << std::endl;
        return;
    }

    waterHeight = heightmapSize.y * 0.15f;
    waterScale = heightmapSize.x * 0.6f;
}

void Renderer::InitializeRainSystem()
{
    raindrops.resize(MAX_RAINDROPS);
    rainShader = new Shader("Mars/rainVertex.glsl", "Mars/rainFragment.glsl");

    if (!rainShader->LoadSuccess()) {
        std::cout << "Failed to load rain shader!" << std::endl;
        return;
    }

    for (auto& drop : raindrops) {
        drop.position = Vector3(
            rand() % (int)heightmapSize.x,
            rand() % 100 + 400.0f,
            rand() % (int)heightmapSize.z
        );
        drop.velocity = Vector3(-20.0f, -100.0f, 0.0f);
        drop.life = (rand() % 100) / 100.0f;
        drop.length = 1.0f + ((rand() % 50) / 50.0f);
    }

    // Basic quad for rain particles
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
    };

    glGenVertexArrays(1, &rainVAO);
    glGenBuffers(1, &rainVBO);

    glBindVertexArray(rainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rainVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Renderer::InitializeLightningSystem()
{
    lightningBolts.resize(MAX_LIGHTNING_BOLTS);
    lightningShader = new Shader("Mars/lightningVertex.glsl", "Mars/lightningFragment.glsl");
    if (!lightningShader->LoadSuccess()) {
        std::cout << "Failed to load lightning shader!" << std::endl;
        return;
    }

    for (auto& bolt : lightningBolts) {
        bolt.active = false;
        bolt.life = 0.0f;
        bolt.intensity = 0.0f;
    }

    glGenVertexArrays(1, &lightningVAO);
    glGenBuffers(1, &lightningVBO);

    glBindVertexArray(lightningVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightningVBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LightningVertex), (void*)offsetof(LightningVertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(LightningVertex), (void*)offsetof(LightningVertex, intensity));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    lightningLight = new Light(Vector3(0, 0, 0), Vector4(0.8f, 0.8f, 1.0f, 1.0f), 20000.0f);

    lightningLight->SetColour(Vector4(1.0f, 1.0f, 1.2f, 1.0f));
    lightningLight->SetRadius(30000.0f);
}

void Renderer::InitializeShadowMapping()
{
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glGenFramebuffers(1, &shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    shadowShader = new Shader("Mars/shadowVertex.glsl", "Mars/shadowFragment.glsl");
}

void Renderer::InitializeUFO()
{
    ufoMesh = Mesh::GenerateUFO();

    ufo = new UFO(heightmapSize);

    ufoTexture = SOIL_load_OGL_texture(
        TEXTUREDIR "Mars/ufo.png",
        SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS
    );

    glBindTexture(GL_TEXTURE_2D, ufoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    ufoShader = new Shader("Mars/ufoVertex.glsl", "Mars/ufoFragment.glsl");
    if (!ufoShader->LoadSuccess()) {
        std::cout << "Failed to load UFO shader!" << std::endl;
    }

    ufoBeamShader = new Shader("Mars/ufoBeamVertex.glsl", "Mars/ufoBeamFragment.glsl");
    if (!ufoBeamShader->LoadSuccess()) {
        std::cout << "Failed to load beam shader!" << std::endl;
    }
}

void Renderer::InitializeTransitionResources()
{
    transitionShader = new Shader("Mars/transitionVertex.glsl", "Mars/transitionFragment.glsl");
    if (!transitionShader->LoadSuccess()) {
        std::cout << "Failed to load transition shader!" << std::endl;
        return;
    }

    glGenFramebuffers(2, transitionFBO);
    glGenTextures(2, transitionTextures);
    glGenRenderbuffers(2, transitionDepth);

    // Both FBOs with textures and depth buffers setup
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, transitionFBO[i]);

        // Setup color texture
        glBindTexture(GL_TEXTURE_2D, transitionTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, transitionTextures[i], 0);

        // Setup depth buffer
        glBindRenderbuffer(GL_RENDERBUFFER, transitionDepth[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, transitionDepth[i]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::InitializeSplitScreenShader()
{
    splitScreenShader = new Shader("Mars/splitScreenVertex.glsl", "Mars/splitScreenFragment.glsl");
    if (!splitScreenShader->LoadSuccess()) {
        std::cout << "Failed to load split screen shader!" << std::endl;
        return;
    }
}

void Renderer::InitializeBloom()
{
    // Create HDR framebuffer
    glGenFramebuffers(1, &bloomBuffers.hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, bloomBuffers.hdrFBO);

    // Create 2 floating point color buffers
    glGenTextures(2, bloomBuffers.colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, bloomBuffers.colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
            GL_TEXTURE_2D, bloomBuffers.colorBuffers[i], 0);
    }

    // Create and attach depth buffer
    glGenRenderbuffers(1, &bloomBuffers.rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, bloomBuffers.rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, bloomBuffers.rboDepth);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // Ping-pong framebuffers for blurring
    glGenFramebuffers(2, bloomBuffers.pingpongFBO);
    glGenTextures(2, bloomBuffers.pingpongBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomBuffers.pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, bloomBuffers.pingpongBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, bloomBuffers.pingpongBuffers[i], 0);
    }

    hdrShader = new Shader("Mars/bloomHDRVertex.glsl", "Mars/bloomHDRFragment.glsl");
    blurShader = new Shader("Mars/bloomBlurVertex.glsl", "Mars/bloomBlurFragment.glsl");
    if (!hdrShader->LoadSuccess() || !blurShader->LoadSuccess()) {
        std::cout << "Failed to load HDR/Bloom shaders!" << std::endl;
    }
}

void Renderer::InitializeRobot()
{
    cube = Mesh::LoadFromMeshFile("OffsetCubeY.msh");

    if (!cube) {
        std::cout << "Failed to load robot mesh!" << std::endl;
        return;
    }

    sceneShader = new Shader("SceneVertex.glsl", "SceneFragment.glsl");
    
    if (!sceneShader->LoadSuccess()) {
        std::cout << "Failed to load robot shader!" << std::endl;
        return;
    }

    std::cout << "Initializing robot at position: " << heightmapSize.x * 0.5f << ", "
        << heightmapSize.y * 0.3f << ", " << heightmapSize.z * 0.5f << std::endl;

       
    root = new SceneNode();
    root->SetTransform(
        Matrix4::Translation(Vector3(heightmapSize.x * 0.5f, heightmapSize.y * 0.3f, heightmapSize.z * 0.5f)) *
        Matrix4::Scale(Vector3(2, 2, 2))
    );
    //root->SetTransform(Matrix4::Translation(Vector3(heightmapSize.x * 0.7f, std::max(heightmapSize.y * 0.8f, heightmapSize.y * 0.2f), heightmapSize.z * 0.7f)) * Matrix4::Scale(Vector3(10, 10, 10)));
    root->AddChild(new CubeRobot(cube));
}

void Renderer::DrawTerrain()
{
    BindShader(terrainShader);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    modelMatrix.ToIdentity();

    UpdateShaderMatrices();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, isAncientMars ? baseSoilTex : currentMarsRock);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, isAncientMars ? rockTex : currentMarsRock);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, isAncientMars ? sedimentTex : baseSoilTex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowTex);

    glUniform1i(glGetUniformLocation(terrainShader->GetProgram(), "baseSoilTex"), 0);
    glUniform1i(glGetUniformLocation(terrainShader->GetProgram(), "rockTex"), 1);
    glUniform1i(glGetUniformLocation(terrainShader->GetProgram(), "sedimentTex"), 2);
    glUniform1i(glGetUniformLocation(terrainShader->GetProgram(), "shadowTex"), 3);

    glUniform3fv(glGetUniformLocation(terrainShader->GetProgram(), "lightPos"), 1, (float*)&sunLight->GetPosition());
    glUniform4fv(glGetUniformLocation(terrainShader->GetProgram(), "lightColour"), 1, (float*)&sunLight->GetColour());
    glUniform1f(glGetUniformLocation(terrainShader->GetProgram(), "lightRadius"), sunLight->GetRadius());
    glUniform1f(glGetUniformLocation(terrainShader->GetProgram(), "ambientStrength"), ambientStrength);
    glUniformMatrix4fv(glGetUniformLocation(terrainShader->GetProgram(), "shadowMatrix"), 1, false, shadowMatrix.values);

    // Camera position for specular calculations
    glUniform3fv(glGetUniformLocation(terrainShader->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());

    UpdateBeamLight();

    marsTerrain->Draw();
}

void Renderer::DrawSkybox()
{
    glDepthMask(GL_FALSE);

    BindShader(skyboxShader);
    UpdateShaderMatrices();

    glUniform1i(glGetUniformLocation(skyboxShader->GetProgram(), "cubeTex"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, isAncientMars ? (isNight ? marsSkyboxAncientNight : marsSkyboxAncientDay) : (isNight ? marsSkyboxAncientNight : marsSkyboxCurrent));

    skyQuad->Draw();

    glDepthMask(GL_TRUE);
}

void Renderer::DrawWater() 
{
    if (!isAncientMars) return;

    BindShader(waterShader);

    modelMatrix.ToIdentity();
    modelMatrix = Matrix4::Translation(Vector3(heightmapSize.x * 0.5f, waterHeight, heightmapSize.z * 0.5f)) * Matrix4::Scale(Vector3(waterScale, 1, waterScale));

    UpdateShaderMatrices();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glUniform1i(glGetUniformLocation(waterShader->GetProgram(), "shadowTex"), 1);
    glUniformMatrix4fv(glGetUniformLocation(waterShader->GetProgram(), "shadowMatrix"), 
        1, false, shadowMatrix.values);

    glUniform1f(glGetUniformLocation(waterShader->GetProgram(), "waterMovement"), waterMovement);
    glUniform3fv(glGetUniformLocation(waterShader->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());
    glUniform3fv(glGetUniformLocation(waterShader->GetProgram(), "lightPos"), 1, (float*)&sunLight->GetPosition());
    glUniform4fv(glGetUniformLocation(waterShader->GetProgram(), "lightColour"), 1, (float*)&sunLight->GetColour());

    // Proper transparency setup
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    waterMesh->Draw();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::DrawRain()
{
    if (!isRaining || !isAncientMars) return;

    BindShader(rainShader);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(rainVAO);

    Vector3 cameraPos = camera->GetPosition();
    Vector3 cameraUp(0, 1, 0);
    Vector3 cameraRight = Vector3::Cross(camera->GetForward(), cameraUp).Normalised();

    for (const auto& drop : raindrops) {
        if (drop.life <= 0.0f) continue;

        float distToCamera = (drop.position - cameraPos).Length();

        float baseSizeScale = 0.5f;

        float distanceFactor = 1.0f - (std::min(distToCamera, 1500.0f) / 1500.0f);
        float sizeScale = baseSizeScale + (distanceFactor * 0.3f);

        // Different scales for width and height to make drops look more like streaks
        float widthScale = sizeScale * 0.2f;
        float heightScale = sizeScale * drop.length;

        Matrix4 billboardMatrix;
        billboardMatrix.ToIdentity();

        Vector3 scaledRight = cameraRight * widthScale;
        Vector3 scaledUp = cameraUp * heightScale;

        billboardMatrix.values[0] = scaledRight.x;
        billboardMatrix.values[1] = scaledRight.y;
        billboardMatrix.values[2] = scaledRight.z;

        billboardMatrix.values[4] = scaledUp.x;
        billboardMatrix.values[5] = scaledUp.y;
        billboardMatrix.values[6] = scaledUp.z;

        billboardMatrix.values[8] = 0;
        billboardMatrix.values[9] = 0;
        billboardMatrix.values[10] = widthScale;

        billboardMatrix.values[12] = drop.position.x;
        billboardMatrix.values[13] = drop.position.y;
        billboardMatrix.values[14] = drop.position.z;
        billboardMatrix.values[15] = 1.0f;

        modelMatrix = billboardMatrix;
        UpdateShaderMatrices();

        glUniform1f(glGetUniformLocation(rainShader->GetProgram(), "rainLength"), drop.length);
        glUniform1f(glGetUniformLocation(rainShader->GetProgram(), "distToCamera"), distToCamera);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void Renderer::DrawLightning()
{
    if (!isRaining || !isAncientMars || !isNight) {
        return;
    }

    BindShader(lightningShader);

    // Pass camera position for glow effect
    glUniform3fv(glGetUniformLocation(lightningShader->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBindVertexArray(lightningVAO);

    for (const auto& bolt : lightningBolts) {
        if (!bolt.active || bolt.segments.empty()) continue;

        std::vector<LightningVertex> vertices;
        vertices.reserve(bolt.segments.size() * 2);

        for (size_t i = 0; i < bolt.segments.size() - 1; ++i) {
            float segmentIntensity = bolt.intensity * (1.0f - ((float)i / bolt.segments.size()));

            LightningVertex v1, v2;
            v1.position = bolt.segments[i];
            v1.intensity = segmentIntensity;
            v2.position = bolt.segments[i + 1];
            v2.intensity = segmentIntensity * 0.9f;

            vertices.push_back(v1);
            vertices.push_back(v2);
        }

        glBindBuffer(GL_ARRAY_BUFFER, lightningVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(LightningVertex),
            vertices.data(), GL_DYNAMIC_DRAW);

        modelMatrix.ToIdentity();
        UpdateShaderMatrices();

        glDrawArrays(GL_LINES, 0, vertices.size());
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void Renderer::DrawShadowMap()
{
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glClear(GL_DEPTH_BUFFER_BIT);

    BindShader(shadowShader);
    glUniformMatrix4fv(glGetUniformLocation(shadowShader->GetProgram(), "shadowMatrix"), 1, false, shadowMatrix.values);

    // Draw terrain to shadow map
    modelMatrix.ToIdentity();
    glUniformMatrix4fv(glGetUniformLocation(shadowShader->GetProgram(), "modelMatrix"), 1, false, modelMatrix.values);
    marsTerrain->Draw();

    // Draw UFO to shadow map
    if (ufo && ufoMesh) {
        modelMatrix = ufo->GetModelMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shadowShader->GetProgram(), "modelMatrix"), 1, false, modelMatrix.values);
        ufoMesh->Draw();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
}

void Renderer::DrawUFO()
{
    if (!ufo || !ufoMesh || !ufoShader) return;

    BindShader(ufoShader);

    modelMatrix = ufo->GetModelMatrix();
    UpdateShaderMatrices();

    glUniform3fv(glGetUniformLocation(ufoShader->GetProgram(), "lightPos"), 1, (float*)&sunLight->GetPosition());
    glUniform4fv(glGetUniformLocation(ufoShader->GetProgram(), "lightColour"), 1, (float*)&sunLight->GetColour());
    glUniform3fv(glGetUniformLocation(ufoShader->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glUniform1i(glGetUniformLocation(ufoShader->GetProgram(), "shadowTex"), 1);
    glUniformMatrix4fv(glGetUniformLocation(ufoShader->GetProgram(), "shadowMatrix"), 1, false, shadowMatrix.values);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ufoTexture);
    glUniform1i(glGetUniformLocation(ufoShader->GetProgram(), "diffuseTex"), 0);

    ufoMesh->Draw();
}

void Renderer::DrawUFOBeam()
{
    if (!ufo || !ufoBeamShader) {
        return;
    }

    BindShader(ufoBeamShader);

    modelMatrix = ufo->GetModelMatrix();
    UpdateShaderMatrices();

    Vector3 ufoPos = ufo->GetPosition();

    glUniform3fv(glGetUniformLocation(ufoBeamShader->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());
    glUniform1f(glGetUniformLocation(ufoBeamShader->GetProgram(), "beamIntensity"), ufo->GetBeamIntensity());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    ufo->DrawBeam();

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

void Renderer::DrawNode(SceneNode* n) {
    if (n->GetMesh()) {
        BindShader(sceneShader);
        Matrix4 model = n ->GetWorldTransform() * Matrix4::Scale(n->GetModelScale());
        glUniformMatrix4fv(glGetUniformLocation(sceneShader->GetProgram(), "modelMatrix"), 1, false, model.values);
        
        glUniform4fv(glGetUniformLocation(sceneShader->GetProgram(), "nodeColour"), 1, (float*)&n->GetColour());
        
        glUniform1i(glGetUniformLocation(sceneShader->GetProgram(), "useTexture"), 0);
        n->Draw(*this);
    }
 
    for (vector < SceneNode* >::const_iterator
            i = n->GetChildIteratorStart();
            i != n-> GetChildIteratorEnd(); ++i) {
        DrawNode(*i);
    }
}

Vector3 Renderer::GenerateLightningPosition()
{
    // Generate position near camera
    Vector3 camPos = camera->GetPosition();
    float minDistance = 200.0f;
    float maxDistance = 800.0f;
    float distance = minDistance + (rand() % (int)(maxDistance - minDistance));
    float angle = (rand() % 360) * (PI / 180.0f);

    return Vector3(camPos.x + cos(angle) * distance, camPos.y + 300.0f + (rand() % 200), camPos.z + sin(angle) * distance);
}

void Renderer::CreateLightningBolt()
{
    for (auto& bolt : lightningBolts) {
        if (!bolt.active) {
            bolt.active = true;
            bolt.life = 1.0f;
            bolt.intensity = 4.0f;
            bolt.position = GenerateLightningPosition();
            lastLightningPos = bolt.position;
            lightningIlluminationIntensity = 1.0f;

            // Create more complex lightning pattern
            const int numMainSegments = 15;  // More segments
            const float segmentLength = 200.0f;  // Longer segments

            Vector3 start = bolt.position;
            Vector3 end = start;
            end.y = marsTerrain->GetHeightAt(start.x, start.z);

            Vector3 direction = (end - start) / static_cast<float>(numMainSegments);
            Vector3 current = start;

            bolt.segments.clear();
            bolt.segments.push_back(current);

            // Create main bolt with more variation
            for (int i = 0; i < numMainSegments; ++i) {
                current += direction;
                // Zigzag
                current.x += (rand() % 200 - 100) * 0.8f;
                current.z += (rand() % 200 - 100) * 0.8f;
                bolt.segments.push_back(current);

                // Random branches with probability
                if (rand() % 100 < 40) {
                    Vector3 branchStart = current;
                    Vector3 branchDir = direction * 0.5f;

                    // Randomize branch direction
                    float branchAngle = (rand() % 360) * (PI / 180.0f);
                    branchDir.x += cos(branchAngle) * 20.0f;
                    branchDir.z += sin(branchAngle) * 20.0f;

                    // Create branch segments
                    int branchSegments = 3 + (rand() % 4);
                    for (int j = 0; j < branchSegments; ++j) {
                        branchStart += branchDir;
                        branchStart.x += (rand() % 60 - 30) * 0.5f;
                        branchStart.z += (rand() % 60 - 30) * 0.5f;
                        bolt.segments.push_back(branchStart);
                    }
                }
            }
            break;
        }
    }
}

void Renderer::GenerateShadowMatrix()
{
    Matrix4 shadowView = Matrix4::BuildViewMatrix(sunLight->GetPosition(), Vector3(heightmapSize.x * 0.5f, 0, heightmapSize.z * 0.5f));
    Matrix4 shadowProj = Matrix4::Orthographic(-heightmapSize.x * 0.5f, heightmapSize.x * 0.5f, heightmapSize.z * 0.5f, -heightmapSize.z * 0.5f, -heightmapSize.y, heightmapSize.y * 2.0f);
    shadowMatrix = shadowProj * shadowView;
}

void Renderer::UpdateSunPosition(float dt)
{
    timeOfDay += (dt / DAY_LENGTH) * 24.0f;
    if (timeOfDay >= 24.0f) {
        timeOfDay -= 24.0f;
    }

    float sunAngle = ((timeOfDay - SUNRISE_TIME) / (SUNSET_TIME - SUNRISE_TIME)) * PI;

    if (timeOfDay < SUNRISE_TIME || timeOfDay > SUNSET_TIME) {
        sunAngle = PI;
        isNight = true;
        ambientStrength = 0.15f;
    }
    else {
        isNight = false;
        float intensity = std::max(0.0f, sin(sunAngle));
        ambientStrength = 0.7f * intensity + 0.2f;
    }

    float sunHeight = sin(sunAngle) * 10000.0f;
    float sunDistance = cos(sunAngle) * 10000.0f;

    Vector3 terrainCenter(heightmapSize.x * 0.5f, 0, heightmapSize.z * 0.5f);
    sunLight->SetPosition(Vector3(
        terrainCenter.x + sunDistance,
        terrainCenter.y + sunHeight,
        terrainCenter.z
    ));

    float intensity = std::max(0.0f, sin(sunAngle));
    Vector4 sunColor = Vector4(1.5f, 1.4f, 1.3f, 1.0f) * intensity;
    sunColor.w = 1.0f;
    sunLight->SetColour(sunColor);
}

void Renderer::UpdateWater(float dt)
{
    waterMovement += dt * WATER_WAVE_SPEED;

    if (waterMovement > 1000.0f) {
        waterMovement = 0.0f;
    }
}

void Renderer::UpdateRain(float dt)
{
    if (!isRaining || !isAncientMars) return;

    Vector3 cameraPos = camera->GetPosition();
    Vector3 cameraForward = camera->GetForward(); // Get camera's forward direction
    float viewRange = 2000.0f; // Increased view range

    for (auto& drop : raindrops) {
        drop.position += drop.velocity * dt;
        drop.life -= dt;

        bool shouldReset = drop.life <= 0.0f || drop.position.y <= marsTerrain->GetHeightAt(drop.position.x, drop.position.z) || (drop.position - cameraPos).Length() > viewRange;

        if (shouldReset) {
            // Hemisphere of rain around the camera
            float theta = (rand() % 360) * PI / 180.0f;  // Angle around camera
            float radius = (rand() % (int)(viewRange * 0.8f));  // Distance from camera
            float heightVariation = 50.0f + (rand() % 200);  // Height above camera

            // Calculate position in a cylinder around camera
            drop.position = Vector3(cameraPos.x + cos(theta) * radius, cameraPos.y + heightVariation, cameraPos.z + sin(theta) * radius);

            // Angled rain with more consistent visibility
            float baseSpeed = -10.0f;  // Increased fall speed
            drop.velocity = Vector3(-40.0f + (rand() % 20 - 10), baseSpeed - (rand() % 50), (rand() % 20 - 10));

            drop.life = 1.0f;
            drop.length = 3.0f + ((rand() % 20) / 10.0f);
        }
    }
}

void Renderer::UpdateLightning(float dt)
{
    if (!isRaining || !isAncientMars || !isNight) {
        return;
    }

    lightningTimer -= dt;

    if (lightningIlluminationIntensity > 0.0f) {
        lightningIlluminationIntensity -= dt * 2.0f;
    }

    if (lightningTimer <= 0.0f) {
        if ((float)rand() / RAND_MAX < lightningProbability) {
            CreateLightningBolt();
            lightningLight->SetPosition(lastLightningPos);
            lightningLight->SetRadius(50000.0f);
            Vector4 lightColor = Vector4(0.8f, 0.9f, 1.0f, 1.0f) * (2.0f + (rand() % 3));
            lightningLight->SetColour(lightColor);
        }
        lightningTimer = timeBetweenStrikes;
    }

    for (auto& bolt : lightningBolts) {
        if (bolt.active) {
            bolt.life -= dt * 2.0f;
            bolt.intensity = bolt.life * 2.0f;

            if (bolt.life <= 0.0f) {
                bolt.active = false;
            }
        }
    }
}

void Renderer::UpdateTransition(float dt)
{
    if (!isTransitioning) return;

    transitionProgress += dt / transitionDuration;

    if (transitionProgress >= 1.0f) {
        transitionProgress = 1.0f;
        isTransitioning = false;
        isAncientMars = !isAncientMars;
    }
}

void Renderer::UpdateCameraTrack(float dt)
{
    if (!isTrackCamera) return;

    trackTimer += dt;
    CameraTrackPoint& current = cameraTrack[currentTrackPoint];
    CameraTrackPoint& next = cameraTrack[(currentTrackPoint + 1) % cameraTrack.size()];

    if (trackTimer >= current.timeToNext) {
        trackTimer = 0.0f;
        currentTrackPoint = (currentTrackPoint + 1) % cameraTrack.size();
        return;
    }

    // Interpolate between current and next point
    float t = trackTimer / current.timeToNext;
    float smoothT = (sin((t - 0.5f) * PI) + 1.0f) * 0.5f;

    Vector3 newPos = current.position * (1.0f - smoothT) + next.position * smoothT;
    Vector3 newLook = current.lookAt * (1.0f - smoothT) + next.lookAt * smoothT;

    // Calculate target height above terrain
    float terrainHeight = marsTerrain->GetHeightAt(newPos.x, newPos.z);
    float minHeight = terrainHeight + heightmapSize.y * 0.2f;
    currentTargetHeight = std::max(newPos.y, minHeight);

    // Smooth height interpolation
    float heightDiff = currentTargetHeight - smoothedHeight;
    smoothedHeight += heightDiff * dt * HEIGHT_SMOOTH_SPEED;

    // Apply smoothed height
    newPos.y = smoothedHeight;

    // Update camera position and look direction
    Camera* targetCam = (camera == secondaryCamera) ? secondaryCamera : camera;
    targetCam->SetPosition(newPos);
    targetCam->LookAt(newLook);
}

void Renderer::UpdateFPSCounter(float dt)
{
    frameTimeSum += dt;
    frameCount++;
    fpsUpdateTime += dt;

    if (fpsUpdateTime >= FPS_UPDATE_INTERVAL) {
        currentFPS = frameCount / frameTimeSum;

        std::string color;
        if (currentFPS >= 60.0f) {
            color = "\033[32m"; // Green
        }
        else if (currentFPS >= 30.0f) {
            color = "\033[33m"; // Yellow
        }
        else {
            color = "\033[31m"; // Red
        }

        std::cout << "FPS: " << std::fixed << std::setprecision(1) << currentFPS << std::endl;

        frameTimeSum = 0.0f;
        frameCount = 0;
        fpsUpdateTime = 0.0f;
    }
}

void Renderer::UpdateBeamLight()
{
    if (!ufo || !ufo->IsBeamActive()) return;

    Vector3 beamPos = ufo->GetPosition() + Vector3(0, -10, 0);
    Vector3 beamDir = Vector3(0, -1, 0);

    BindShader(terrainShader);
    glUniform1i(glGetUniformLocation(terrainShader->GetProgram(), "beamActive"), (isNight && ufo && ufo->IsBeamActive()) ? 1 : 0);
    glUniform3fv(glGetUniformLocation(terrainShader->GetProgram(), "beamPosition"), 1, (float*)&beamPos);
    glUniform3fv(glGetUniformLocation(terrainShader->GetProgram(), "beamDirection"), 1, (float*)&beamDir);
    glUniform4f(glGetUniformLocation(terrainShader->GetProgram(), "beamColor"), 0.5f, 1.0f, 0.2f, 1.0f);
    glUniform1f(glGetUniformLocation(terrainShader->GetProgram(), "beamIntensity"), ufo->GetBeamIntensity() * 2.0f);
    glUniform1f(glGetUniformLocation(terrainShader->GetProgram(), "beamCutoff"), cos(3.14159f / 6.0f));
}
