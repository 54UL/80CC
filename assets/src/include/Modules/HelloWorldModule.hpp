// Front-end user code
// DOES NOT WORK

#ifndef HELLO_WORLD_MODULE_HPP
#define HELLO_WORLD_MODULE_HPP

#include <80CC.hpp>

namespace etycc
{
   class HelloWorldModule : GameModule
   {
   private:
      // GhostCamera playerCamera; 

   public:
      HelloWorldModule(/* args */);
      ~HelloWorldModule();

      // GameModule interface
      bool OnStart(const Engine* engine)
      {
         // engine->LoadScene(paths::DEFAULT_SCENE_NAME);
         auto& scene = engine->mainScene_; 

         // scene->instanceNode(Assets::LoadTemplate("default-sprite"));
         // auto renderable = dynamic_cast<Sprite*>(scene->findNode("some-node")->addComponent("renderable"));

         // playerCamera->Init(engine->inputSytem);
         
         return true; 
      }
      
      void OnUpdate(const float deltaTime)
      {
         // playerCamera->Update(deltaTime);

         return true
      }

      void OnDestroy()
      {
         
      }
   };
}

#endif