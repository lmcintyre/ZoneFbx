# ZoneFbx

ZoneFbx is a driver program and small C++/CLI library utilizing 
the FBX SDK to export FFXIV zones to FBX format, complete with textures
and proper hierarchy/object grouping.

If normal maps look weird in Blender, which they will, set the Normal node
to sRGB color space, and set the Normal/Map to World Space.

# Usage
https://streamable.com/tjg45n

- download the file
- extract the file
- shift click on empty space in the folder
- click Open Powershell Window Here
- enter ".\zonefbx.exe", the path to your XIV install, the path to the zone you want to extract (in the video i grab it from godbert), and the path you want the FBX to be saved to, in the exact format the video does it. when stuff is filled in automatically, that's me pressing "tab" - you don't have to type the full thing, only enough for windows to fill the rest in
- press enter
- import into blender!

note: the program is really bad with slashes and spaces in file paths, type it exactly like the video
