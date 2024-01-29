#include <iostream>
#include <windows.h>

#include <SDL.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <SDL2/SDL_opengl.h>
#include <btBulletDynamicsCommon.h>

// DRAG CONTROLS
bool isDragging = false;
int lastMouseX = 0;
int lastMouseY = 0;

void handleMouseMotion(SDL_Event& event) {
    int mouseX = event.motion.x;
    int mouseY = event.motion.y;

    if (isDragging) {
        int deltaX = mouseX - lastMouseX;
        int deltaY = mouseY - lastMouseY;

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(20 + deltaX * 0.1, 20 - deltaY * 0.1, 18, 0, 0, 0, 0, 1, 0);
    }

    lastMouseX = mouseX;
    lastMouseY = mouseY;
}

void handleMouseButton(SDL_Event& event) {
    if (event.button.button == SDL_BUTTON_LEFT) {
        if (event.button.state == SDL_PRESSED) {
            isDragging = true;
            lastMouseX = event.motion.x;
            lastMouseY = event.motion.y;
        } else if (event.button.state == SDL_RELEASED) {
            isDragging = false;
        }
    }
}

// GRAIVITY ATRACTION BETWEEN BODIES
class CustomGravityForce : public btActionInterface {
public:
    btVector3 gravity;

    CustomGravityForce(const btVector3& gravity) : gravity(gravity) {}
	// virtual void updateAction(btCollisionWorld* collisionWorld, btScalar deltaTimeStep) = 0;

    void updateAction(btCollisionWorld* dynamicsWorld, btScalar deltaTimeStep) override {
        // Iterate through all objects in the world
        for (int i = 0; i < dynamicsWorld->getNumCollisionObjects(); ++i) {
            btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];

            // Check if the object is a rigid body
            if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
                btRigidBody* body = btRigidBody::upcast(obj);
                if (body && body->getInvMass() > 0.0) {
                    // Apply gravitational force
                    // body->applyForce(gravity * body->getMass(), btVector3(0, 0, 0));
                }
            }
        }
    }

    void debugDraw(btIDebugDraw* debugDrawer) override {}

    // You can override other methods if needed

    // Destructor
    ~CustomGravityForce() {}
};

void render(btDiscreteDynamicsWorld& dynamicsWorld, SDL_Window * window) {

    // Render rigid bodies
    for (int i = 0; i < dynamicsWorld.getNumCollisionObjects(); ++i) {
        btCollisionObject* obj = dynamicsWorld.getCollisionObjectArray()[i];
        if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
            btRigidBody* body = btRigidBody::upcast(obj);
            btTransform trans;
            body->getMotionState()->getWorldTransform(trans);
            btVector3 pos = trans.getOrigin();

            glPushMatrix();
            glColor3i(0x1E,0x1E,0x1E);
            // Convert Bullet to OpenGL coordinates
            glTranslatef(pos.getX(), pos.getY(), -pos.getZ());
            glutSolidCube(1);  // Rendering a simple cube for each rigid body
            glPopMatrix();
        }
    }
}

int SDL_main(int argc, char* argv[]) {
    glutInit(&argc, argv);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return -1;
    }
    // Set up SDL window and OpenGL context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window* window = SDL_CreateWindow("Bullet + SDL + OpenGL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_GLContext glContext = SDL_GL_CreateContext(window);

    // Initialize Bullet
    btDefaultCollisionConfiguration collisionConfig;
    btCollisionDispatcher dispatcher(&collisionConfig);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld dynamicsWorld(&dispatcher, &broadphase, &solver, &collisionConfig);
    dynamicsWorld.setGravity(btVector3(0, -9.81, 0));  // No global gravity

    // Custom gravity force
    CustomGravityForce* gravityForce = new CustomGravityForce(btVector3(0, -9.81, 0));
    dynamicsWorld.addAction(gravityForce);
    
    btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(50.), btScalar(1.), btScalar(50.)));
    btTransform groundTransform;
    groundTransform.setIdentity();
    groundTransform.setOrigin(btVector3(0, -1, 0));
    btDefaultMotionState* groundMotionState = new btDefaultMotionState(groundTransform);
    btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, groundShape);
    btRigidBody* groundRigidBody = new btRigidBody(groundRigidBodyCI);
    dynamicsWorld.addRigidBody(groundRigidBody);

    // Create multiple bodies
    const int numBodies = 5;
    btCollisionShape* boxShape = new btBoxShape(btVector3(1, 1, 1));

    for (int i = 0; i < numBodies; ++i) {
        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin(btVector3(0, i * 4, 0));
    
        btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(1, motionState, boxShape);
        btRigidBody* rigidBody = new btRigidBody(rigidBodyCI);

        dynamicsWorld.addRigidBody(rigidBody);
    }

    // Set up OpenGL
    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    gluPerspective(70.0f, 800.0f / 600.0f, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    gluLookAt(20, 20, 18, 0, 0, 0, 0, 1, 0);

    // Simulate and render loop
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e.type == SDL_MOUSEMOTION)
            {
                handleMouseMotion(e);
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
            {
                handleMouseButton(e);
            }
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // glLoadIdentity();
        // glPushMatrix();
        // glTranslatef(0,0,0);
        // glutSolidCube(5);  // Rendering a simple cube for each rigid body
        // glPopMatrix();

        // Access body positions
        for (int j = 0; j < dynamicsWorld.getNumCollisionObjects(); ++j) {
            btCollisionObject* obj = dynamicsWorld.getCollisionObjectArray()[j];

            // Check if the object is a rigid body
            if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
                btRigidBody* body = btRigidBody::upcast(obj);
                btTransform trans;
                body->getMotionState()->getWorldTransform(trans);
                btVector3 pos = trans.getOrigin();
                
            }
        }
        dynamicsWorld.stepSimulation(1 / 60.f, 10);
        render(dynamicsWorld, window);
        SDL_GL_SwapWindow(window);
        SDL_Delay(32);
    }

    // Clean up
    dynamicsWorld.removeAction(gravityForce);
    delete gravityForce;
    
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}


