(WIP)
download and add this folder to the plugin folder of ue5 project
compile and run the editor

How to use it:
1) Drop a nav volume into your level
2) set the dimension limits of your dungeon on the navmesh bound volume
3) Drop in the Dungeon Generator Actor class into the level
4) set up basic configuration(room size, seed, etc
5) createa DungeonRoomData to specify wall thikness material and door asset
6) Assign the asset to the dungeon generator Actor
7) create a DungeonHallwayData asset to specifyu wallthikness and hallway dimensions.
8) Assigne asset to the Dungeon generator actor
9) click the buttons in this order
    GenerateDungeonRooms, ConnectRooms, Collapse(wait till finished), SimplifyConnections, CreateHallways, RenderDungeon.

Pending work
 imrpve hallway generation to avoid wird set ups when rooms areconected and has to generate a steep vertical section.
 generate collisions properly
 better rendering of hallways to avoid overlaping hallways bloking each other
 generate navmesh correctly inside dungeon.
