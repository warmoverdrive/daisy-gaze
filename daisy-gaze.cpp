// NOT TESTED ON SEED HARDWARE

#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
ReverbSc verb;

enum controls
{
	feedback = 0,
	lpf,
	blend,
	vol,
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

	ProcessAnalogControls();

	verb.SetFeedback(param[feedback].Value());
	verb.SetLpFreq(param[lpf].Value());

	// loop through each sample in block
	for (size_t i = 0; i < size; i += 2)
	{
		dry = in[i];

		verb.Process(dry, 0, &wet, 0);

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

	cfg[vol].InitSingle(hw.GetPin(15));		 // pin 22
	cfg[blend].InitSingle(hw.GetPin(16));	 // pin 23
	cfg[feedback].InitSingle(hw.GetPin(21)); // pin 28
	cfg[lpf].InitSingle(hw.GetPin(22));		 // pin 29
	hw.adc.Init(cfg, END_CONTROLS);

	for (int i = 0; i < END_CONTROLS; i++) // Wrap in AnalogControl objects
		knobs[i].Init(hw.adc.GetPtr(i), hw.AudioCallbackRate());

	// Wrap in Parameter objects, with mapped values and interpolation curves
	param[vol].Init(knobs[vol], 0.0f, 3.0f, Parameter::LOGARITHMIC);
	param[blend].Init(knobs[blend], 0.0f, 1.0f, Parameter::LINEAR);
	param[feedback].Init(knobs[feedback], 0.6f, 0.999f, Parameter::LOGARITHMIC);
	param[lpf].Init(knobs[lpf], 500.0f, 12000.0f, Parameter::LOGARITHMIC);

	hw.adc.Start();
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	verb.Init(hw.AudioSampleRate());

	InitAnalogControls();

	hw.StartAudio(AudioCallback);
	while (1)
	{
	}
}