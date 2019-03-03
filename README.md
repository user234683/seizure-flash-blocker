# Seizure Flash Blocker

# NOTE: This is currently an experimental prototype intended as a proof of concept. It should not be relied on by people with epilepsy.

## Notes on running the software
It is not guarranteed to work on Windows 10

## Why we made it
Some videos on Youtube with flashing lights in them have [SEIZURE-WARNING] or something similar in the title or description. Most don't, and it's baffling how easy it would be for someone with photosensitive epilepsy to stumble on one of these videos with no warning at all. Modern operating systems have various accessibility features, but nothing to protect people with this condition. Protection from flashing images on the screen ought to be built in and battle tested, so our hope was to build a proof of concept prototype that will make developers aware of the concept, so they can make more advanced and comprehensive versions.

## What it does
When the executable is running, it monitors the Google Chrome window for regions that are changing color too quickly, and paints solid red over those regions to cover them. It is Windows only at the moment.

## How we built it
We wrote it in C++ using the windows api. Though it is set to detect Google Chrome for demonstration purposes, any program works with this strategy; one need only change the executable name to search for.

It divides the region into small rectangles. It computes the "aggregate color difference" over 200 ms time intervals, and if this aggregate exceeds a threshold, it is considered to be changing color too rapidly/intensely, and it uses windows graphics functions to draw a red rectangle over the region. This rectangle remains for as long as the region continues to flash.

The algorithm for computing the aggregate color difference is based on the idea of distance in 3 dimensional color space, which has a red dimension, green dimension, and blue dimension. Every color can be described as a point in this space. The distance between two colors can thus be found by the 3 dimensional pythagorean theorem. The bigger the distance in color space, the more extreme the change in color is considered to be. These color distances are summed over the entire region to compute an aggregate between two frames. The total aggregate over 200 ms (6 frames at 30 fps) is computed by summing up these aggregates over those 6 frames.


## What's next for Seizure Flash Blocker
As we continue building the program, we'll do more research on the exact triggers for photosensitive epilepsy to develop improved detection algorithms with fewer false positives. We'll have to spend a lot of time researching the windows api in more depth to overcome the current limitations we have, such as by leveraging DirectX for improved performance writing to the screen. There are likely many performance optimizations still waiting to be found. One that we have in mind is using inline assembly SIMD instructions which can perform the same operation on large arrays in parallel.

In the future, we hope to also look into the difficult problem of protecting users from specially crafted videos/gifs specifically intended to induce seizures (there have been known cases of this happening). Attacks can potentially be crafted to bypass algorithms used to detect seizure-inducing images, making it a much harder thing to defend against than unintentional cases.

We're excited about the future developments that could spring from this, such as dedicated VGA/DVI/HDMI adapters that could protect any monitor at no performance cost to the OS and also protect televisions.
