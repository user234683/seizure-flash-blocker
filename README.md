# Seizure Flash Blocker

# NOTE: This is currently an experimental prototype. It should not be relied on by people with epilepsy in its current state.

## About
When the executable is running, it monitors the Google Chrome window for regions that are changing color too quickly, and paints solid red over those regions to cover them. It is Windows only at the moment. It was made at Hacktech 2019 (devpost [here](https://devpost.com/software/seizure-flash-blocker)).

## Motivation
The condition where seizures can be induced by flashing colors or regular repeating patterns is known as photosensitive epilepsy. Some videos on Youtube with flashing lights in them have [SEIZURE-WARNING] or something similar in the title or description, but many don't, so it is not enough to rely on the goodwill of people posting on the internet to include warnings. Sometimes flashing content is deliberately sent to people with epilepsy. The goal of this software is to detect such content before it is displayed.

## Usage
 - Download the latest release.
 - Open chrome.
 - Run the exe.
 - Don't resize or move the chrome window

(In the next version, it will be system wide, not dependent on chrome)

On Windows 10, hardware acceleration must be disabled in Chrome for it to work.


## Software design
It is written in C using the windows api. Though it is set to detect Google Chrome for demonstration purposes, any program works with this strategy; one need only change the executable name to search for.

It divides the region into small rectangles. For each region, it computes the "aggregate color difference" between the current frame and the last frame. It counts how many times in the last 200 ms this aggregate exceeds a threshold. If it exceeds the threshold more than 3 times, it is considered to be flashing, and it uses windows graphics functions to draw a red rectangle over the region. This rectangle remains for as long as the region continues to flash.

The algorithm for computing the aggregate color difference is based on the idea of distance in 3 dimensional color space, which has a red dimension, green dimension, and blue dimension. Every color can be described as a point in this space. The distance between two colors can thus be found by the 3 dimensional pythagorean theorem. The bigger the distance in color space, the more extreme the change in color is considered to be. These color distances are summed over the entire region to compute an aggregate between two frames.

Future versions will use a better algorithm based on the Harding test.

## Roadmap (in rough order)

- [x] Make region partitioning work on any window size
- [x] Use a faster method for drawing over regions
- [x] Convert to C
- [x] Make it work on individual pixels
- [ ] Average the pixels instead of simply covering them
- [ ] Vectorize the algorithm, using `restrict` and possibly inline assembly or the GPU to improve the performance
- [ ] Once 30 fps or more is achieved, use a theoretical model of flashing/sampling (already created) to calculate the constants for that framerate. Add framerate monitoring to keep these constants right if performance drops.
- [ ] Clean up comments, improve code quality, and add error handling to move from hackathon quality to production quality
- [ ] Change the algorithm to one with a scientific basis, such as a Harding-like algorithm
- [ ] Make it work system wide ?
- [ ] Add a system tray, settings, and the option to run at startup

## Compiling

### MinGW
Use the command `gcc -static-libgcc -O3 src/*.c -lGdi32 -o seizure-flash-blocker.exe`

Note: The -l flag must come [after](http://www.mingw.org/wiki/specify_the_libraries_for_the_linker_to_use) the `src/*.c`

`-O3` tells it to apply optimizations. Use `-g` instead if you want to debug with gdb.

The static flags tell it to include gcc's C runtime in the executable, [since it's not present on most computers](https://stackoverflow.com/questions/4702732/the-program-cant-start-because-libgcc-s-dw2-1-dll-is-missing).

If compiling with MinGW isn't working, open an issue.
