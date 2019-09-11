DPCM Split

This NES program demonstrates abusing the NES's sampled sound
playback hardware as a scanline timer to split the screen twice
without needing to use the MMC3's IRQ.

== How it works ==

First the program sets up VRAM and starts a looping 1-byte sample at
the fastest rate.  Then the main loop does this:

Main loop:
1. Wait for NMI count to change
2. Set scroll
3. Wait for sprite 0 hit
4. Change the scroll to create the top of the region
5. Start the IRQ count at 0 and set the sample to play with IRQ at end
6. Count cycles until an IRQ occurs
7. Wait for 10 more IRQs to occur
8. Disable IRQs
9. Count a few cycles minus the number counted in step 6
10. Change the scroll to create the bottom of the region

The IRQ handler increments the IRQ count and restarts the sample.

The alignment between IRQ and NMI differs per frame.  But because the
number of cycles in steps 6 and 9 add up to a constant, and steps
5, 7, and 8 are also constant, there is a constant time for steps 5
through 9 and thus a constant height of the middle scrolling region.

This ROM is a proof of concept.  There are still occasional glitches
when I run it on my PowerPak, but I bet blargg can troubleshoot them.

== Legal ==

The following license applies to the source code, binary code, and
this manual:

Copyright 2010 Damian Yerrick
Copying and distribution of this file, with or without modification,
are permitted in any medium without royalty provided the copyright
notice and this notice are preserved.  This file is offered as-is,
without any warranty.
