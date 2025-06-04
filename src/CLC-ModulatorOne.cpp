#include "plugin.hpp"
#include <cmath>
#include <random>

struct CLC_ModulatorOne : Module {
    enum ParamId {
        MODE_PARAM,
        RATE_PARAM,
        ONESHOT_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        RATE_CV_INPUT,
        ONESHOT_CV_INPUT,
        RESET_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        MODULATORSIGNAL_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        MODULATORLED_LIGHT,
        LIGHTS_LEN
    };

    enum Mode {
        POLYGON,
        FIBONACCI,
        GOLDEN_RATIO,
        HARMONIC,
        BOUNCING_BALL,
        CRESTING_OCEAN,
        SPIRAL_GALAXY,
        WIND_GUSTS,
        NUM_MODES
    };

    float phase = 0.0f;
    float sampleRate = 0.0f;
    int currentStep = 0;
    dsp::SchmittTrigger resetTrigger;
    bool isCycleActive = false;
    float noisePhase = 0.0f;
    float currentNoise = 0.0f;
    float nextNoise = 0.0f;
    float noiseUpdateRate = 10.0f;
    std::default_random_engine rng;
    std::uniform_real_distribution<float> dist{-180.0f, 180.0f};
    bool justTriggered = false; // New flag to ensure initial voltage is 0V

    static constexpr float MAX_RATE = 10.0f;
    static constexpr float MIN_RATE = 0.1f;
    static constexpr float BOUNCE_DAMPING = 0.8f;
    static constexpr float WAVE_SKEW = 1.5f;
    static constexpr float SPIRAL_SCALE = 180.0f;
    static constexpr int BOUNCE_CYCLE_STEPS = 20;
    static constexpr int MAX_SPIRAL_STEPS = 100;

    CLC_ModulatorOne() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configSwitch(MODE_PARAM, 0.f, NUM_MODES - 1, 0.f, "Modulation Mode", {
            "Polygon", "Fibonacci", "Golden Ratio", "Harmonic",
            "Bouncing Ball", "Cresting Ocean", "Spiral Galaxy", "Wind Gusts"
        });
        configParam(RATE_PARAM, 0.f, 1.f, 0.5f, "Rate", " Hz", 0.f, MAX_RATE - MIN_RATE, MIN_RATE);
        configSwitch(ONESHOT_PARAM, 0.f, 1.f, 0.f, "One-Shot", {"Off", "On"});
        configInput(RATE_CV_INPUT, "Rate CV");
        configInput(ONESHOT_CV_INPUT, "One-Shot CV");
        configInput(RESET_INPUT, "Reset/Trigger");
        configOutput(MODULATORSIGNAL_OUTPUT, "Modulator Output");
        sampleRate = APP->engine->getSampleRate();
        rng.seed(std::random_device()());
        currentNoise = dist(rng);
        nextNoise = dist(rng);
        resetTrigger.reset();
    }

    void onSampleRateChange() override {
        sampleRate = APP->engine->getSampleRate();
    }

    bool isSmoothMode(int mode) {
        return mode == CRESTING_OCEAN || mode == WIND_GUSTS;
    }

    int getCycleLength(int mode) {
        switch (mode) {
            case POLYGON:
                return 5; // Cycle through n=3 to 7
            case FIBONACCI:
                return 13; // 13 steps based on fib array
            case GOLDEN_RATIO:
                return 8; // One full rotation
            case HARMONIC:
                return 7; // Cycle through n=2 to 8
            case BOUNCING_BALL:
                return BOUNCE_CYCLE_STEPS; // 20 steps
            case CRESTING_OCEAN:
                return 1; // One phase cycle
            case SPIRAL_GALAXY:
                return MAX_SPIRAL_STEPS; // 100 steps
            case WIND_GUSTS:
                return 1; // One phase cycle
            default:
                return 1;
        }
    }

    float getPhaseAngle(float mode, int step, float phase) {
        switch (static_cast<int>(mode)) {
            case POLYGON: {
                int n = 3 + (step % 5);
                return (n - 2) * 180.0f / n;
            }
            case FIBONACCI: {
                static const float fib[] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233};
                return fmodf(fib[step % 13] * 10.0f, 360.0f);
            }
            case GOLDEN_RATIO: {
                static const float goldenAngle = 360.0f / 1.61803398875f;
                return fmodf(step * goldenAngle, 360.0f);
            }
            case HARMONIC: {
                int n = 2 + (step % 7); // n=2 to 8
                return 360.0f / n;
            }
            case BOUNCING_BALL: {
                float angle = 180.0f * std::pow(BOUNCE_DAMPING, step % BOUNCE_CYCLE_STEPS);
                return fmodf(angle, 360.0f);
            }
            case CRESTING_OCEAN: {
                float sine = std::sin(2.0f * M_PI * phase);
                float skewedPhase = std::copysign(std::pow(std::abs(sine), WAVE_SKEW), sine);
                return 180.0f * (skewedPhase + 1.0f);
            }
            case SPIRAL_GALAXY: {
                int clampedStep = step % MAX_SPIRAL_STEPS;
                float angle = SPIRAL_SCALE * std::log1p(clampedStep);
                return fmodf(angle, 360.0f);
            }
            case WIND_GUSTS: {
                return currentNoise + (nextNoise - currentNoise) * phase;
            }
            default:
                return 0.0f;
        }
    }

    void process(const ProcessArgs& args) override {
        float mode = params[MODE_PARAM].getValue();
        float rateCV = inputs[RATE_CV_INPUT].isConnected() ? inputs[RATE_CV_INPUT].getVoltage() / 10.0f : 0.0f;
        float rateParam = clamp(params[RATE_PARAM].getValue() + rateCV, 0.0f, 1.0f);
        float rate = rateParam * (MAX_RATE - MIN_RATE) + MIN_RATE;
        bool oneShotSwitch = params[ONESHOT_PARAM].getValue() > 0.5f;
		float oneShotCVParm = inputs[ONESHOT_CV_INPUT].isConnected() ? inputs[ONESHOT_CV_INPUT].getVoltage() : 0.0f;
        bool isSmooth = isSmoothMode(static_cast<int>(mode));
        float cv = 0.0f;

		// enable control ov oneshot switch via CV
		//DEBUG("oneShoteCVparm = %f", oneShotCVParm);
		oneShotSwitch = oneShotSwitch || (static_cast<bool>(oneShotCVParm > 0.5f));
		
        // Detect reset/trigger
        if (inputs[RESET_INPUT].isConnected()) {
            if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.0f, 1.0f)) {
                phase = 0.0f;
                currentStep = 0;
                noisePhase = 0.0f;
                isCycleActive = true;
                justTriggered = true; // Set flag to force initial 0V output
                if (static_cast<int>(mode) == WIND_GUSTS) {
                    currentNoise = nextNoise;
                    nextNoise = dist(rng);
                }
            }
        }

        // Handle output voltage
        if (oneShotSwitch && !isCycleActive) {
            // Output 0V when cycle is complete in one-shot mode
            cv = 0.0f;
        } else if (oneShotSwitch && justTriggered) {
            // Output 0V immediately after trigger in one-shot mode
            cv = 0.0f;
            justTriggered = false; // Reset flag after first sample
        } else {
            // Update phase or step
            bool shouldUpdate = !oneShotSwitch || (oneShotSwitch && isCycleActive);
            if (shouldUpdate) {
                if (isSmooth) {
                    // Smooth modes: increment phase
                    phase += rate / sampleRate;
                    if (phase >= 1.0f) {
                        phase = 0.0f;
                        currentStep++;
                        if (static_cast<int>(mode) == WIND_GUSTS) {
                            currentNoise = nextNoise;
                            nextNoise = dist(rng);
                        }
                        // In one-shot mode, stop after one full phase cycle
                        if (oneShotSwitch && currentStep >= getCycleLength(static_cast<int>(mode))) {
                            isCycleActive = false;
                        }
                    }
                } else {
                    // Stepped modes: increment phase and advance step
                    phase += rate / sampleRate;
                    if (phase >= 1.0f) {
                        phase = 0.0f;
                        currentStep++;
                        // In one-shot mode, stop after reaching mode-specific cycle length
                        if (oneShotSwitch && (currentStep >= getCycleLength(static_cast<int>(mode)))) {
                            isCycleActive = false;
                        }
                    }
                }

                // Handle noise updates for Wind Gusts mode
                if (static_cast<int>(mode) == WIND_GUSTS) {
                    noisePhase += noiseUpdateRate / sampleRate;
                    if (noisePhase >= 1.0f) {
                        noisePhase = 0.0f;
                        currentNoise = nextNoise;
                        nextNoise = dist(rng);
                    }
                }

                // Compute output voltage
                float angle = getPhaseAngle(mode, currentStep, phase);
                cv = angle / 360.0f * 10.0f;
            }
        }

        // Set output and light
        outputs[MODULATORSIGNAL_OUTPUT].setVoltage(cv);
        lights[MODULATORLED_LIGHT].setBrightness(cv / 10.0f);
    }
};

struct CLC_ModulatorOneWidget : ModuleWidget {
    CLC_ModulatorOneWidget(CLC_ModulatorOne* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/CLC-ModulatorOne.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.32, 27.484)), module, CLC_ModulatorOne::MODE_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(9.991, 49.107)), module, CLC_ModulatorOne::RATE_PARAM));
        addParam(createParamCentered<BefacoSwitch>(mm2px(Vec(30.808, 49.195)), module, CLC_ModulatorOne::ONESHOT_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.895, 66.113)), module, CLC_ModulatorOne::RATE_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.533, 66.113)), module, CLC_ModulatorOne::ONESHOT_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 87.809)), module, CLC_ModulatorOne::RESET_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.32, 110.034)), module, CLC_ModulatorOne::MODULATORSIGNAL_OUTPUT));

        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(20.32, 19.394)), module, CLC_ModulatorOne::MODULATORLED_LIGHT));
    }
};

Model* modelCLC_ModulatorOne = createModel<CLC_ModulatorOne, CLC_ModulatorOneWidget>("CLC-ModulatorOne");