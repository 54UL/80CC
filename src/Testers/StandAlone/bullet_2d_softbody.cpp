#include <iostream>
#include <cmath>
#include <btBulletDynamicsCommon.h>
#include <SDL.h>
#include <GL/gl.h>

// Initialize Bullet Physics
btDefaultCollisionConfiguration collisionConfig;
btCollisionDispatcher dispatcher(&collisionConfig);
btBroadphaseInterface* broadphase = new btDbvtBroadphase();
btSequentialImpulseConstraintSolver solver;
btSoftRigidDynamicsWorld* dynamicsWorld = new btSoftRigidDynamicsWorld(&dispatcher, broadphase, &solver, &collisionConfig);

// Soft body settings
btSoftBodyWorldInfo softBodyWorldInfo;

// SDL and OpenGL variables
SDL_Window* window;
SDL_GLContext context;
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Function to initialize SDL and OpenGL
void initSDL() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window = SDL_CreateWindow("Soft Body Simulation", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
    context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Function to initialize Bullet Physics
void initBullet() {
    dynamicsWorld->setGravity(btVector3(0, -10, 0));
    softBodyWorldInfo.m_gravity.setValue(0, -10, 0);
}

// Function to create a 2D soft body circle
btSoftBody* createSoftBodyCircle(float radius, int segments) {
    btSoftBody* softBody = btSoftBodyHelpers::CreateEllipsoid(softBodyWorldInfo, btVector3(0, 10, 0), btVector3(radius, 0, 0), segments);
    softBody->m_cfg.piterations = 2;
    softBody->m_cfg.citerations = 2;
    softBody->m_cfg.diterations = 2;

    dynamicsWorld->addSoftBody(softBody);
    return softBody;
}

// Function to render the soft body
void renderSoftBody(btSoftBody* softBody) {
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLES);

    for (int i = 0; i < softBody->m_faces.size(); i++) {
        btSoftBody::Node* node0 = softBody->m_faces[i].m_n[0];
        btSoftBody::Node* node1 = softBody->m_faces[i].m_n[1];
        btSoftBody::Node* node2 = softBody->m_faces[i].m_n[2];

        glVertex2f(node0->m_x.getX(), node0->m_x.getY());
        glVertex2f(node1->m_x.getX(), node1->m_x.getY());
        glVertex2f(node2->m_x.getX(), node2->m_x.getY());
    }

    glEnd();
}

// Main loop
void runSimulation() {
    bool quit = false;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }

        dynamicsWorld->stepSimulation(1.0 / 60, 10);

        glClear(GL_COLOR_BUFFER_BIT);

        btSoftBodyArray& softBodies = dynamicsWorld->getSoftBodyArray();
        for (int i = 0; i < softBodies.size(); i++) {
            btSoftBody* softBody = softBodies[i];
            renderSoftBody(softBody);
        }

        SDL_GL_SwapWindow(window);
    }
}

// Cleanup function
void cleanup() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char* args[]) {
    initSDL();
    initBullet();

    // Create a 2D soft body circle
    btSoftBody* softBody = createSoftBodyCircle(5.0f, 32);

    runSimulation();
    cleanup();

    return 0;
}
