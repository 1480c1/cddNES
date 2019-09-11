These tests seem to be focussed on testing the behavior of $4017 writes around quarter/half step signals.

The main thing tested here is when the frame counter reset is delayed, clocking must not happen twice in 5-step mode if the $4017 write takes place near a natural clock (writing to $4017 in 5-step mode clocks immediately)

Tests 2, 7, and 11 seem to be dependent on the startup PPU/CPU alignment of the NES.

It's questionable whether these tests should be taken seriously. Even the author doesn't really remember what they do, why they are valuable, or even that they are correct:

https://forums.nesdev.com/viewtopic.php?f=3&t=13844&start=30#p170727
https://forums.nesdev.com/viewtopic.php?f=3&t=11174

Translated from Russian:

https://strangeway.org/2014/04/%D0%B7%D0%B0%D0%BF%D0%B8%D1%81%D0%BA%D0%B8-%D0%BE-nes-apu-c%D1%87%D0%B5%D1%82%D1%87%D0%B8%D0%BA-%D0%BA%D0%B0%D0%B4%D1%80%D0%BE%D0%B2/

"How well do emulators take into account all of the above? Most developers try to make their programs as accurate as possible, since many NES games are very dependent on this and rely on all sorts of hardware glitches. I wrote a simple test that checked the generation of QuarterFrame and HalfFrame on writing 0x4017 values 0x80 . All emulators easily passed the test. 
But what if the recording happens exactly when the frame counter itself has to generate these signals? The signal must be generated only once. And then all the emulators showered. And it's hard to blame them - absolutely nothing is said in the documentation about this."