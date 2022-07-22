# daisy-gaze

#### Author

Hank Atherton (Warm Overdrive)

## Shoegaze in a Box

#### A DSP guitar pedal based on the Electro-Smith Daisy Seed platform.

As of July 22nd, 2022:  
Daisy Seed Reverb test with wrapping parameters, adapting from Daisy Petal Verb example. Porting general concepts of parameter wrapping with mapped values, looking into utility header functions I can impliment on other projects, and just a general test of my understanding of the library. Untested on Seed hardware.

## Future Plans

- Expansion from a simple reverb to a modulated reverb with momentary/latch controls for infinite feedback
- Possible tremolo addition.
- Routing into an analog fuzz with a blend control in series with Seed output
- Utility header file to make setup of other projects easier
- KiKad project file for schematic and PCB

### Current Dependencies

- [DaisyExamples repo](https://github.com/electro-smith/DaisyExamples) (uses daisylib and DaisySP)

## Hardware Setup (7.22.22)

All power and ground rails are set normally for the Seed.  
Potentiometers are connected to Seed by lug 2.

| Control         | Description            | Routing            |
| --------------- | ---------------------- | ------------------ |
| Audio In        | Audio Source Input     | AUDIO IN -> PIN16  |
| Audio Out       | Processed Audio Output | PIN18 -> AUDIO OUT |
| Volume          | Output Volume Control  | B10k -> PIN22      |
| Blend           | Dry/Wet Blend          | B10k -> PIN23      |
| Feedback        | Reverb Decay           | B10k -> PIN28      |
| Low Pass Filter | Reverb Low Pass        | B10k -> PIN29      |
