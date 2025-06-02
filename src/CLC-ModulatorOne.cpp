#include "plugin.hpp"


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

	CLC_ModulatorOne() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(MODE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(RATE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(ONESHOT_PARAM, 0.f, 1.f, 0.f, "");
		configInput(RATE_CV_INPUT, "");
		configInput(ONESHOT_CV_INPUT, "");
		configInput(RESET_INPUT, "");
		configOutput(MODULATORSIGNAL_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
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
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.808, 49.195)), module, CLC_ModulatorOne::ONESHOT_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.895, 66.113)), module, CLC_ModulatorOne::RATE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.533, 66.113)), module, CLC_ModulatorOne::ONESHOT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 87.809)), module, CLC_ModulatorOne::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.32, 110.034)), module, CLC_ModulatorOne::MODULATORSIGNAL_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(20.32, 19.394)), module, CLC_ModulatorOne::MODULATORLED_LIGHT));
	}
};


Model* modelCLC_ModulatorOne = createModel<CLC_ModulatorOne, CLC_ModulatorOneWidget>("CLC-ModulatorOne");