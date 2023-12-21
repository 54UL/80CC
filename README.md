# ALPHA_V1
`NO NAME YET` backlog

# Dependencies

* Box 2D 
* ENet
* SDL (opengl renderer)
* SDL Audio
* IMGUI

# ARCHITECURE:

* Threading
    - Thread pool

* Input
    - SDL input wrapper

* Network 
    - ENet and other netcode wrappers

* Scene
    - Scene manager (handles rendering, physics,netcode and audio for scene objects)
        - Spatially computed (for ultra larger scenes)
        - Serializable
        - Prefab loader 
        - Level of detail
            - Dinamic asset loader/unloader (from disk or ram)

* Renderer
    - sdl/software / opengles and open gl 3.0+ wrapper

* Physics
    - box 2d wrapper

* UI
    - Inmediate mode ui (IMGUI WRAPPER)

* Game module
    - Implements the propper interfaces to use all the systems above and they are linked statically or dinamically to the Game entry point

* Game entry
    - Initialize all the systems and loads the game modules to execute the game, if debbuger enabled the game modules are gonna run inside of the develoment engine (statically linked and executed)
    - Development Engine (all systems must be compile independent (dynamic libraries or static)
        - DEBUG UI
        - PREFAB EDITOR
        - SPRITE EDITOR

# ALPHA FEATURES:

* Camara controller
    - pan
    - zoom in zoom out

* Build editor
    - Place/extrude
    - Vertex selector *optional*
    - Remove
    - Select primitive (rectangular/cuadrangular selector)
    - Move primitive (when selected...)

* Build primitives (astethic details)
    - Concrete (for building on surfaces)
    - Metal square (spaceship like)
    - Thrusters
    - Turrets
    
* Player/s controller
    - Select unit/s
    - Control unit/s
        - Ship control
            - Movement
            - Weapons
        - EVA(Extra vehicular) control
            - Repairs
            - Weapons

* Celestial bodies generator
    - Planets
        - Rocks/metalics
            - With atmosphere
        - Gas

* 2D Physics objects
    - Ships
    - Characters
    - Planets orbits 

* Main game rules/esscense
    * Claim planets as you wish
    * Create infrastructure and aquire resources
        - Via primitive editor (to build space ships and buildings)
        - Ore system as main economic system
            - Ore extraction items/machines
                - Construction materials (to build infrastucture like space ships or buildings)
                - rare materials that can be sell? * idea *
    * Defend or attack other players
        - Ship weapons
        - Character/s weapons
        - Explosives and massive destruction weapons like "death star laser"

    * Expand...
    * RTS Like (All game systems are simulated at real time at any place of the scene)

* Asthetics
    * 2D sprites and light
    * kinda pixel art
    * softbodies and large body physics (gravity)

# Tasks v0.1 alpha...
- Implement `Threading` *optional but required*
- Implement `Scene`
    - Scene object and component system
- Implement `Renderer`
- Implement `Input`
- Implement `Game Module`
- Implement `UI`
- Implement `Game Entry` *IMPORTANT*

- Implement `Game module` "Camera controller"
- Implement `Game module` "Ship editor"

# Tasks v0.2 alpha
- Implement `Physics` 
- Implement `Game module` "Player controller"
- Implement `Game module` "Celestial bodies generator"
- Implement `Game module` "Game settings"

# Taskt v0.3 alpha
- All unfinished task and improvements
- To be defined...
