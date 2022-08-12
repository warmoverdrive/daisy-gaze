// NOT TESTED ON SEED HARDWARE

#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
ReverbSc DSY_SDRAM_BSS verb;
Chorus DSY_SDRAM_BSS chorus;

u_int8_t counter;

enum controls
{
	feedback,
	depth,
	blend,
	vol,
	fun,
	END_CONTROLS
};

AnalogControl knobs[END_CONTROLS];
Parameter param[END_CONTROLS];

/**
 * @brief
 * Helper function for processing all inputs for later use.
 * Use at top of AudioCallback.
 */
void ProcessAnalogControls()
{
	for (int i = 0; i < END_CONTROLS; i++)
		param[i].Process();
}

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
				   AudioHandle::InterleavingOutputBuffer out,
				   size_t size)
{
	float dry, wet, mix;

	if (counter == 0)
	{
		ProcessAnalogControls();
		verb.SetFeedback(param[feedback].Value());
		chorus.SetFeedback(param[depth].Value());
		chorus.SetLfoFreq(param[fun].Value());
	}
	counter++;
	if (counter > 15)
		counter = 0;

	// loop through each sample in block
	for (size_t i = 0; i < size; i += 2)
	{
		dry = in[i];

		verb.Process(chorus.Process(dry), 0, &wet, 0);

		wet *= 1.5f; // chorus quiets the dry signal heavily

		// blend the dry and wet by multiplying by the 0.0f -> 1.0f value to get
		// a percentage total. max dry is at a 0.0f value of the blend param, while
		// max wet is at 1.0f.
		mix = (dry * (1 - param[blend].Value())) + (wet * param[blend].Value());

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
	cfg[fun].InitSingle(hw.GetPin(17));		 // pin 24
	hw.adc.Init(cfg, END_CONTROLS);

	for (int i = 0; i < END_CONTROLS; i++) // Wrap in AnalogControl objects
		knobs[i].Init(hw.adc.GetPtr(i), hw.AudioCallbackRate());

	// Wrap in Parameter objects, with mapped values and interpolation curves
	param[vol].Init(knobs[vol], 0.0f, 3.0f, Parameter::LINEAR);
	param[blend].Init(knobs[blend], 0.0f, 1.0f, Parameter::LINEAR);
	param[feedback].Init(knobs[feedback], 0.75f, 0.999f, Parameter::LINEAR);
	param[depth].Init(knobs[depth], 0.0f, 1.0f, Parameter::LINEAR);
	param[fun].Init(knobs[fun], 0.00001f, 1.0f, Parameter::LINEAR);

	hw.adc.Start();
}

int main(void)
{
	counter = 0;
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	verb.Init(hw.AudioSampleRate());

	chorus.Init(hw.AudioSampleRate());
	chorus.SetDelay(1.0f);
	chorus.SetLfoDepth(1.0f);

	InitAnalogControls();

	hw.StartAudio(AudioCallback);
	while (1)
	{
	}
}