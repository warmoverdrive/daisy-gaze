// NOT TESTED ON SEED HARDWARE

#include "daisy_seed.h"
#include "daisysp.h"

// max delay time, set by multiplying the sample rate (48khz) by the seconds.
// currently set to a max of 1 second.
#define MAX_DELAY static_cast<size_t>(48000 * 1.f)

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
ReverbSc DSY_SDRAM_BSS verb;
Chorus DSY_SDRAM_BSS chorus;
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayMem;
Oscillator lfo;
// Final mix crossfad
CrossFade cFadeFinal;
// Modulation/Dry Reverb crossfade
CrossFade cFadeMod;
// Delay/Chorus crossfade
CrossFade cFadeModFX;

u_int8_t counter;

enum controls
{
	feedback,
	depth,
	blend,
	vol,
	modFreq,
	modBlend,
	END_CONTROLS
};

// Pulled from tnatoli's rhythm delay code to cleanly affect the delay time modulation
// May refactor into a class to fit the rest of the API's paradigm
struct Delay
{
	DelayLine<float, MAX_DELAY> *delay;
	float currentDelay;
	float delayTarget;
	float feedback;

	// Reads/writes to delay line and adjusts delay time to interpolate changes in time
	float Process(float in)
	{
		// smooths out the delay time delta when the target is changed
		fonepole(currentDelay, delayTarget, 0.0002f);
		delay->SetDelay(currentDelay);
		float read = delay->Read();
		delay->Write((feedback * read) + in);
		return read;
	}
};

Delay delay;
float delBaseFeedback = 0.8f;
float delChaosFeedback = 1.2f;
float delBaseTime = MAX_DELAY * 0.25f;
float lfo_val = 0.0f;
float delayMaxDepth = 0.05f;
float delayDepth = 0.0f;

AnalogControl knobs[END_CONTROLS];
Parameter param[END_CONTROLS];

Switch modSwitch;	 // TEMP SWITCH FOR MODULATION TESTING
bool isDelay = true; // TEMP BOOL FOR MODULATION TESTING

/**
 * @brief
 * Helper function for processing all control inputs and applying values.
 * Sequentially polls an input per callback.
 * Use at top of AudioCallback.
 */
void ProcessAnalogControls()
{
	// check to reset counter
	counter = counter > controls::END_CONTROLS ? 0 : counter;

	// every sample a new param is processed to split duties
	switch (counter)
	{
	case controls::feedback:
		verb.SetFeedback(param[feedback].Process());
		delay.feedback = param[feedback].Value() * delBaseFeedback;
		break;
	case controls::depth:
		chorus.SetLfoDepth(param[depth].Process());
		delayDepth = param[depth].Value() * delayMaxDepth;
		break;
	case controls::blend:
		cFadeFinal.SetPos(param[blend].Process());
		break;
	case controls::vol:
		param[vol].Process();
		break;
	case controls::modFreq:
		chorus.SetLfoFreq(param[modFreq].Process());
		lfo.SetFreq(param[modFreq].Value());
		break;
	case controls::modBlend:
		cFadeMod.SetPos(param[modBlend].Process());
		break;
	case controls::END_CONTROLS: // using this to process the delay time modulation
		delay.delayTarget = delBaseTime * (1 + (lfo_val * delayDepth));

		// temporary switch to test delay modulation
		// modSwitch.Debounce();
		// isDelay = modSwitch.Pressed();
		// cFadeModFX.SetPos(isDelay ? 1.0f : 0.0f);
		break;
	}

	counter++;
}

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
				   AudioHandle::InterleavingOutputBuffer out,
				   size_t size)
{
	float dry, wetRvb, wetChr, wetDel, mix;

	ProcessAnalogControls();

	// loop through each sample in block
	for (size_t i = 0; i < size; i += 2)
	{
		lfo_val = lfo.Process();
		dry = in[i];

		verb.Process(dry, 0, &wetRvb, 0);

		wetChr = chorus.Process(wetRvb);
		wetDel = delay.Process(wetRvb);

		// Mix modulation FX
		mix = cFadeModFX.Process(wetChr, wetDel);

		// crossfade modulation amount
		// todo set up modulating delay line
		mix = cFadeMod.Process(wetRvb, mix);

		// use a crossfade to blend evenly between the dry and wet signals without changing volume
		mix = cFadeFinal.Process(dry, mix);

		// mono output, with volume controler as a boost/cut.
		// mess with the Parameter min/max values here cause
		// it might be too loud.
		out[i] = mix * param[vol].Value();
	}
}

/**
 * @brief
 * Set up control wrappers for knobs by establishing an ADC config,
 *	wrapping pointers to the analog inputs to AnalogControl objects,
 *	and then further wrapping them in Parameter objects to make
 *	scaling and mapping values significantly easier.
 *	Starts ADC.
 */
void InitAnalogControls()
{
	AdcChannelConfig cfg[END_CONTROLS];

	cfg[vol].InitSingle(hw.GetPin(15));		 // pin 22 blue
	cfg[blend].InitSingle(hw.GetPin(16));	 // pin 23 red
	cfg[feedback].InitSingle(hw.GetPin(21)); // pin 28 white
	cfg[depth].InitSingle(hw.GetPin(22));	 // pin 29 yellow
	cfg[modFreq].InitSingle(hw.GetPin(17));	 // pin 24
	cfg[modBlend].InitSingle(hw.GetPin(18)); // pin 25
	hw.adc.Init(cfg, END_CONTROLS);

	for (int i = 0; i < END_CONTROLS; i++) // Wrap in AnalogControl objects
		knobs[i].Init(hw.adc.GetPtr(i), hw.AudioCallbackRate());

	// Wrap in Parameter objects, with mapped values and interpolation curves
	param[vol].Init(knobs[vol], 0.0f, 3.0f, Parameter::LINEAR);
	param[blend].Init(knobs[blend], 0.0f, 1.0f, Parameter::LINEAR);
	param[feedback].Init(knobs[feedback], 0.75f, 0.999f, Parameter::LINEAR);
	param[depth].Init(knobs[depth], 0.0f, 1.0f, Parameter::LINEAR);
	param[modFreq].Init(knobs[modFreq], 0.001f, 1.0f, Parameter::LINEAR);
	param[modBlend].Init(knobs[modBlend], 0.0f, 0.75f, Parameter::LINEAR);

	hw.adc.Start();
}

// Set up Delay and LFO
void InitDelay()
{
	delayMem.Init();
	delay.delay = &delayMem;
	delay.feedback = delBaseFeedback;
	delay.currentDelay = delBaseTime;

	lfo.Init(hw.AudioSampleRate());
	lfo.SetWaveform(lfo.WAVE_SIN);
	lfo.SetFreq(1.0f);
	lfo.SetAmp(1.0f);
}

int main(void)
{
	counter = 0;
	hw.Init();
	hw.SetAudioBlockSize(1); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	verb.Init(hw.AudioSampleRate());

	chorus.Init(hw.AudioSampleRate());
	chorus.SetFeedback(0.4f);

	InitDelay();

	// init crossfades with constant power curves
	cFadeFinal.Init(CROSSFADE_CPOW);
	cFadeMod.Init(CROSSFADE_CPOW);
	cFadeModFX.Init(CROSSFADE_CPOW);

	InitAnalogControls();

	modSwitch.Init(hw.GetPin(10)); // PIN 11

	hw.StartAudio(AudioCallback);
	while (1)
	{
	}
}