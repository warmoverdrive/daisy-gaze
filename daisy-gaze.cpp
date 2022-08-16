// NOT TESTED ON SEED HARDWARE

#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
ReverbSc DSY_SDRAM_BSS verb;
Chorus DSY_SDRAM_BSS chorus;
CrossFade cFadeFinal, cFadeMod;

u_int8_t counter;

enum controls
{
	feedback,
	depth,
	blend,
	vol,
	chrsFreq,
	modBlend,
	END_CONTROLS
};

AnalogControl knobs[END_CONTROLS];
Parameter param[END_CONTROLS];

/**
 * @brief
 * Helper function for processing all control inputs and applying values.
 * Sequentially polls an input per callback.
 * Use at top of AudioCallback.
 */
void ProcessAnalogControls()
{
	// check to reset counter
	counter = counter >= controls::END_CONTROLS ? 0 : counter;

	// every sample a new param is processed to split duties
	switch (counter)
	{
	case controls::feedback:
		verb.SetFeedback(param[feedback].Process());
		break;
	case controls::depth:
		chorus.SetLfoDepth(param[depth].Process());
		break;
	case controls::blend:
		cFadeFinal.SetPos(param[blend].Process());
		break;
	case controls::vol:
		param[vol].Process();
		break;
	case controls::chrsFreq:
		chorus.SetLfoFreq(param[chrsFreq].Process());
		break;
	case controls::modBlend:
		cFadeMod.SetPos(param[modBlend].Process());
		break;
	}

	counter++;
}

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
				   AudioHandle::InterleavingOutputBuffer out,
				   size_t size)
{
	float dry, wetRvb, wetChr, mix;

	ProcessAnalogControls();

	// loop through each sample in block
	for (size_t i = 0; i < size; i += 2)
	{
		dry = in[i];

		verb.Process(dry, 0, &wetRvb, 0);

		wetChr = chorus.Process(wetRvb);

		// crossfade modulation amount
		// todo set up modulating delay line
		mix = cFadeMod.Process(wetRvb, wetChr);

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
	cfg[chrsFreq].InitSingle(hw.GetPin(17)); // pin 24
	cfg[modBlend].InitSingle(hw.GetPin(18)); // pin 25
	hw.adc.Init(cfg, END_CONTROLS);

	for (int i = 0; i < END_CONTROLS; i++) // Wrap in AnalogControl objects
		knobs[i].Init(hw.adc.GetPtr(i), hw.AudioCallbackRate());

	// Wrap in Parameter objects, with mapped values and interpolation curves
	param[vol].Init(knobs[vol], 0.0f, 3.0f, Parameter::LINEAR);
	param[blend].Init(knobs[blend], 0.0f, 1.0f, Parameter::LINEAR);
	param[feedback].Init(knobs[feedback], 0.75f, 0.999f, Parameter::LINEAR);
	param[depth].Init(knobs[depth], 0.0f, 1.0f, Parameter::LINEAR);
	param[chrsFreq].Init(knobs[chrsFreq], 0.00001f, 1.0f, Parameter::LINEAR);
	param[modBlend].Init(knobs[modBlend], 0.0f, 0.75f, Parameter::LINEAR);

	hw.adc.Start();
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

	// init crossfades with constant power curves
	cFadeFinal.Init(CROSSFADE_CPOW);
	cFadeMod.Init(CROSSFADE_CPOW);

	InitAnalogControls();

	hw.StartAudio(AudioCallback);
	while (1)
	{
	}
}