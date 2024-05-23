# openvr-screengrab

Example using C to use the OpenVR GetMirrorTextureD3D11 and process it.

In general:
1. You get the system layer, so things like chaperone will draw on top, and you won't be pixel perfect from what the game gives you.
2. Oddly enough, it doesn't seem like this gets reprojected with motion.
3. It consumes a nontrival amount of CPU resources.
4. Now here's an example for how to grab what the user is seeing with OpenVR.
5. I finally have an example doing DirectX stuff in C.


