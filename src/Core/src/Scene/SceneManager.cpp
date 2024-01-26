/* BRIEF
    MAIN GOAL:

    TickScene(){
        for object each
            {
                object.step()
            }
    }
    ... on object class: 

    object::step(){
        for each component
            component.update()
    }
    ..
    Component is an interface...

    RenderingEntity : IComponent {
        update(object_ctx){
          transform = object_ctx->getComponent<Transform>();

          // Process because it can be the camera calculations and other rendering stuff..
          RenderingEntity->Process(transform);
        }
    }

    RigidBody : IComponent {
        update(object_ctx){
            transform = object_ctx->getComponent<Transform>();
            updatePhysics(transform);
        }
    }

    Networking : IComponent {
        update(object_ctx){
            transform = object_ctx->getComponent<Transform>();
            updateNetwork(transform);  // mutates the transforms 
        }
    }

    AudioSource : IComponent {
        update(object_ctx){
            transform = object_ctx->getComponent<Transform>();
            updateAudio(transform);  // transform for positional audio??
        }
    }

*/
