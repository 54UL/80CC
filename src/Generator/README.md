## Game executable generator (LibTooling based)
quick brief, 
- the generator reads all sources from the assets/src folder, 
- tries to find all <name>GameModule file name prefixes, then copies all the source into Entry/temp.
- Reads `EntryPoint.cpp` source file to put all game modules initialization on the tag `//_80CC_USER_CODE`

*** important, Entry cmake project needs to be a temporal mirror, because the source will be modified by the generator

Game engine(it should be on the editor side....) will produce the build config to be compiled, like:
`cmake .. && cmake --build ./ --config Debug --target Entry`

### dependencies

- ubuntu
```bash
  # todo insert dependencies lol
```