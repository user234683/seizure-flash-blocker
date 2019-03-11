# Seizure Flash Blocker

# NOTE: This is currently an experimental prototype. It should not be relied on by people with epilepsy in its current state.

## About
When the executable is running, it monitors the Google Chrome window for regions that are changing color too quickly, and paints solid red over those regions to cover them. It is Windows only at the moment. It was made at Hacktech 2019 (devpost [here](https://devpost.com/software/seizure-flash-blocker)).

## Motivation
Some videos on Youtube with flashing lights in them have [SEIZURE-WARNING] or something similar in the title or description. Most don't, and it's baffling how easy it would be for someone with photosensitive epilepsy to stumble on one of these videos with no warning at all. Modern operating systems have various accessibility features, but nothing to protect people with this condition. Protection from flashing images on the screen ought to be built in and battle tested, so our hope was to build a proof of concept prototype that will make developers aware of the concept, so they can make more advanced and comprehensive versions.

## Usage
 - Download the latest release.
 - Open chrome.
 - Run the exe.
 - Don't resize or move the chrome window

(In the next version, it will be system wide, not dependent on chrome)

There is currently an issue that prevents it from running on windows 10 in most (all?) cases. This will be fixed.


## How we built it
We wrote it in C++ using the windows api. Though it is set to detect Google Chrome for demonstration purposes, any program works with this strategy; one need only change the executable name to search for.

It divides the region into small rectangles. For each region, it computes the "aggregate color difference" between the current frame and the last frame. It counts how many times in the last 200 ms this aggregate exceeds a threshold. If it exceeds the threshold more than 3 times, it is considered to be flashing, and it uses windows graphics functions to draw a red rectangle over the region. This rectangle remains for as long as the region continues to flash.

The algorithm for computing the aggregate color difference is based on the idea of distance in 3 dimensional color space, which has a red dimension, green dimension, and blue dimension. Every color can be described as a point in this space. The distance between two colors can thus be found by the 3 dimensional pythagorean theorem. The bigger the distance in color space, the more extreme the change in color is considered to be. These color distances are summed over the entire region to compute an aggregate between two frames.


## What's next for Seizure Flash Blocker
As we continue building the program, we'll do more research on the exact triggers for photosensitive epilepsy to develop improved detection algorithms with fewer false positives. We'll have to spend a lot of time researching the windows api in more depth to overcome the current limitations we have, such as by leveraging DirectX for improved performance writing to the screen. There are likely many performance optimizations still waiting to be found. One that we have in mind is using inline assembly SIMD instructions which can perform the same operation on large arrays in parallel.

In the future, we hope to also look into the difficult problem of protecting users from specially crafted videos/gifs specifically intended to induce seizures (there have been known cases of this happening). Attacks can potentially be crafted to bypass algorithms used to detect seizure-inducing images, making it a much harder thing to defend against than unintentional cases.

We're excited about the future developments that could spring from this, such as dedicated VGA/DVI/HDMI adapters that could protect any monitor at no performance cost to the OS and also protect televisions.

## Roadmap (in rough order)

- [x] Make region partitioning work on any window size
- [ ] Make it work system wide
- [ ] Use a faster method for drawing over regions
- [ ] Convert to C
- [ ] Vectorize the algorithm, using `restrict` and possibly inline assembly or the GPU to improve the performance
- [ ] Once 30 fps or more is achieved, use a theoretical model of flashing/sampling (already created) to calculate the constants for that framerate. Add framerate monitoring to keep these constants right if performance drops.
- [ ] Clean up comments, improve code quality, and add error handling to move from hackathon quality to production quality
- [ ] Read the literature on automatic software detection of seizure-inducing content and change the algorithm to give it a scientific basis
- [ ] Add a system tray, settings, and the option to run at startup

## Compiling

### MinGW
Use the command `g++ -O2 src/*.cpp -lGdi32 -lGdiplus -o seizure-flash-blocker.exe`

Note: The -l flags must come [after](http://www.mingw.org/wiki/specify_the_libraries_for_the_linker_to_use) the `src/*.cpp`

`-O2` tells it to apply optimizations. Use `-g` if you want to debug with gdb. If compiling with MinGW isn't working, open an issue.

### Visual Studio
Ugh, will work it out
