#include <iostream>
// #include <SDL.h>
// #include <GL/gl.h>
// #include <GL/glut.h>
// #include <SDL2/SDL_opengl.h>
// #include <btBulletDynamicsCommon.h>

// class CustomGravityForce : public btActionInterface {
// public:
//     btVector3 gravity;

//     CustomGravityForce(const btVector3& gravity) : gravity(gravity) {}
// 	// virtual void updateAction(btCollisionWorld* collisionWorld, btScalar deltaTimeStep) = 0;

//     void updateAction(btCollisionWorld* dynamicsWorld, btScalar deltaTimeStep) override {
//         // Iterate through all objects in the world
//         for (int i = 0; i < dynamicsWorld->getNumCollisionObjects(); ++i) {
//             btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];

//             // Check if the object is a rigid body
//             if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
//                 btRigidBody* body = btRigidBody::upcast(obj);
//                 if (body && body->getInvMass() > 0.0) {
//                     // Apply gravitational force
//                     body->applyForce(gravity * body->getMass(), btVector3(0, 0, 0));
//                 }
//             }
//         }
//     }

//     void debugDraw(btIDebugDraw* debugDrawer) override {}

//     // You can override other methods if needed

//     // Destructor
//     ~CustomGravityForce() {}
// };

// void render(btDiscreteDynamicsWorld& dynamicsWorld, SDL_Window * window) {
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//     glLoadIdentity();

//     // Render rigid bodies
//     for (int i = 0; i < dynamicsWorld.getNumCollisionObjects(); ++i) {
//         btCollisionObject* obj = dynamicsWorld.getCollisionObjectArray()[i];
//         if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
//             btRigidBody* body = btRigidBody::upcast(obj);
//             btTransform trans;
//             body->getMotionState()->getWorldTransform(trans);
//             btVector3 pos = trans.getOrigin();

//             glPushMatrix();
//             glTranslatef(pos.getX(), pos.getY(), pos.getZ());
//             glutSolidCube(1.0);  // Rendering a simple cube for each rigid body
//             glPopMatrix();
//         }
//     }

//     SDL_GL_SwapWindow(window);
// }

// int main() {
//     // Initialize SDL
//     if (SDL_Init(SDL_INIT_VIDEO) < 0) {
//         std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
//         return -1;
//     }

//     // Set up SDL window and OpenGL context
//     SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
//     SDL_Window* window = SDL_CreateWindow("Bullet + SDL + OpenGL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
//     SDL_GLContext glContext = SDL_GL_CreateContext(window);

//     // Initialize Bullet
//     btDefaultCollisionConfiguration collisionConfig;
//     btCollisionDispatcher dispatcher(&collisionConfig);
//     btDbvtBroadphase broadphase;
//     btSequentialImpulseConstraintSolver solver;
//     btDiscreteDynamicsWorld dynamicsWorld(&dispatcher, &broadphase, &solver, &collisionConfig);
//     dynamicsWorld.setGravity(btVector3(0, -9.81, 0));  // No global gravity

//     // Custom gravity force
//     CustomGravityForce* gravityForce = new CustomGravityForce(btVector3(0, -9.81, 0));
//     dynamicsWorld.addAction(gravityForce);

//     // ... Create and add rigid bodies to the world ...

//     // Set up OpenGL
//     glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
//     glMatrixMode(GL_PROJECTION);
//     gluPerspective(45.0f, 800.0f / 600.0f, 0.1f, 100.0f);
//     glMatrixMode(GL_MODELVIEW);
//     gluLookAt(5, 5, 15, 0, 0, 0, 0, 1, 0);

//     // Simulate and render loop
//     bool quit = false;
//     SDL_Event e;
//     while (!quit) {
//         while (SDL_PollEvent(&e)) {
//             if (e.type == SDL_QUIT) {
//                 quit = true;
//             }
//         }

//         dynamicsWorld.stepSimulation(1 / 60.f, 10);

//         render(dynamicsWorld, window);
//     }

//     // Clean up
//     dynamicsWorld.removeAction(gravityForce);
//     delete gravityForce;
    
//     // ... Clean up other objects ...

//     SDL_GL_DeleteContext(glContext);
//     SDL_DestroyWindow(window);
//     SDL_Quit();

//     return 0;
// }


