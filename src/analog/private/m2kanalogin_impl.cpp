/*
 * Copyright 2018 Analog Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file LICENSE.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <libm2k/utils/devicegeneric.hpp>
#include <libm2k/utils/devicein.hpp>
#include <libm2k/analog/m2kanalogin.hpp>
#include <libm2k/m2kexceptions.hpp>
#include <libm2k/utils/utils.hpp>

#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <iio.h>
#include <string.h>

using namespace libm2k::analog;
using namespace libm2k::utils;
using namespace std;
using namespace std::placeholders;

#define HIGH_MAX 2.5
#define HIGH_MIN -2.5
#define LOW_MAX 25
#define LOW_MIN -25

class M2kAnalogIn::M2kAnalogInImpl : public DeviceIn {
public:
	M2kAnalogInImpl(iio_context * ctx, std::string adc_dev, bool sync) :
		DeviceIn(ctx, adc_dev),
		m_need_processing(false),
		m_trigger(nullptr)
	{
		m_m2k_fabric = make_shared<DeviceGeneric>(ctx, "m2k-fabric");
		m_trigger = new M2kHardwareTrigger(ctx);

		/* Filters applied while decimating affect the
		/ amplitude of the received  data */
		m_filter_compensation_table[1E8] = 1.00;
		m_filter_compensation_table[1E7] = 1.05;
		m_filter_compensation_table[1E6] = 1.10;
		m_filter_compensation_table[1E5] = 1.15;
		m_filter_compensation_table[1E4] = 1.20;
		m_filter_compensation_table[1E3] = 1.26;

		m_samplerate = 1E8;

		for (unsigned int i = 0; i < getNbChannels(); i++) {
			m_input_range.push_back(PLUS_MINUS_25V);
			m_adc_calib_offset.push_back(0);
			m_adc_calib_gain.push_back(1);
			m_adc_hw_offset.push_back(0);
			m_trigger->setCalibParameters(i, getScalingFactor(i), m_adc_hw_offset.at(i));
		}

		if (sync) {
			syncDevice();
		}

	}

	~M2kAnalogInImpl()
	{
		if (m_trigger) {
			delete m_trigger;
		}
	}

	void init()
	{
		setOversamplingRatio(1);
		setSampleRate(1E8);

		for (unsigned int i = 0; i < getNbChannels(); i++) {
			enableChannel(i, true);

			ANALOG_IN_CHANNEL ch = static_cast<ANALOG_IN_CHANNEL>(i);
			m_trigger->setAnalogMode(ch, ALWAYS);

			setRange(ch, PLUS_MINUS_25V);
			m_adc_calib_offset.push_back(0);
			m_adc_calib_gain.push_back(1);
			m_adc_hw_offset.push_back(0);
			m_trigger->setCalibParameters(i, getScalingFactor(i), m_adc_hw_offset.at(i));
		}
	}

	void syncDevice()
	{
		for (unsigned int i = 0; i < getNbChannels(); i++) {
			//enabled???
			ANALOG_IN_CHANNEL ch = static_cast<ANALOG_IN_CHANNEL>(i);
			auto range = getRangeDevice(ch);
			m_input_range[i] = range;
//			m_adc_hw_offset.push_back(0);
			m_trigger->setCalibParameters(i, getScalingFactor(i), m_adc_hw_offset.at(i));
			m_samplerate = getSampleRate();
		}
	}

	void setAdcCalibGain(ANALOG_IN_CHANNEL channel, double gain)
	{
		m_adc_calib_gain[channel] = gain;
		m_trigger->setCalibParameters(channel, getScalingFactor(channel), m_adc_hw_offset.at(channel));
	}

	double convertRawToVolts(int sample, double correctionGain,
					      double hw_gain, double filterCompensation, double offset)
	{
		// TO DO: explain this formula
		return ((sample * 0.78) / ((1 << 11) * 1.3 * hw_gain) *
			correctionGain * filterCompensation) + offset;
	}

	int convertVoltsToRaw(double voltage, double correctionGain,
					   double hw_gain, double filterCompensation, double offset)
	{
		// TO DO: explain this formula
		return ((voltage - offset) / (correctionGain * filterCompensation) *
			(2048 * 1.3 * hw_gain) / 0.78);
	}

	double getCalibscale(unsigned int index)
	{
		return getDoubleValue(index, "calibscale");
	}

	double setCalibscale(unsigned int index, double calibscale)
	{
		return setDoubleValue(index, calibscale, "calibscale");
	}

	M2kHardwareTrigger *getTrigger()
	{
		return m_trigger;
	}

	std::vector<std::vector<double>> getSamples(unsigned int nb_samples,
						bool processed = false)
	{
		if (processed) {
			m_need_processing = true;
		}
		m_samplerate = getSampleRate();
		auto fp = std::bind(&M2kAnalogInImpl::processSample, this, _1, _2);
		auto samps = DeviceIn::getSamples(nb_samples, fp);
		if (processed) {
			m_need_processing = false;
		}
		return samps;
	}

	double processSample(int16_t sample, unsigned int channel)
	{
		if (m_need_processing) {
			return convertRawToVolts(sample,
						 m_adc_calib_gain.at(channel),
						 getValueForRange(m_input_range.at(channel)),
						 getFilterCompensation(m_samplerate),
						 m_adc_hw_offset.at(channel));
		} else {
			return (double)sample;
		}
	}

	short getVoltageRaw(unsigned int ch)
	{
		ANALOG_IN_CHANNEL chn = static_cast<ANALOG_IN_CHANNEL>(ch);
		return getVoltageRaw(chn);
	}

	short getVoltageRaw(ANALOG_IN_CHANNEL ch)
	{
		unsigned int num_samples = 100;
		M2K_TRIGGER_MODE mode;
		bool enabled;

		if (ch >= getNbChannels()) {
			throw_exception(EXC_INVALID_PARAMETER, "M2kAnalogIn: no such channel");
		}

		mode = m_trigger->getAnalogMode(ch);
		m_trigger->setAnalogMode(ch, ALWAYS);

		enabled = isChannelEnabled(ch);
		enableChannel(ch, true);

		auto samps = getSamples(num_samples, false);
		double avg = Utils::average(samps.at(ch).data(), num_samples);

		m_trigger->setAnalogMode(ch, mode);
		enableChannel(ch, enabled);
		return (short)avg;
	}

	std::vector<short> getVoltageRaw()
	{
		unsigned int num_samples = 100;
		std::vector<short> avgs;
		std::vector<M2K_TRIGGER_MODE> modes = {};
		std::vector<bool> enabled = {};

		for (unsigned int i = 0; i < getNbChannels(); i++) {
			enabled.push_back(isChannelEnabled(i));
			enableChannel(i, true);
			modes.push_back(m_trigger->getAnalogMode(i));
			m_trigger->setAnalogMode(i, ALWAYS);
		}
		auto samps = getSamples(num_samples, false);
		for (unsigned int i = 0; i < getNbChannels(); i++) {
			short avg = (short)(Utils::average(samps.at(i).data(), num_samples));
			avgs.push_back(avg);
			m_trigger->setAnalogMode(i, modes.at(i));
			enableChannel(i, enabled.at(i));
		}
		return avgs;
	}

	double getVoltage(unsigned int ch)
	{
		ANALOG_IN_CHANNEL chn = static_cast<ANALOG_IN_CHANNEL>(ch);
		return getVoltage(chn);
	}

	double getVoltage(ANALOG_IN_CHANNEL ch)
	{
		unsigned int num_samples = 100;
		M2K_TRIGGER_MODE mode;
		bool enabled;

		if (ch >= getNbChannels()) {
			throw_exception(EXC_OUT_OF_RANGE, "M2kAnalogIn: no such channel");
		}
		mode = m_trigger->getAnalogMode(ch);
		m_trigger->setAnalogMode(ch, ALWAYS);

		enabled = isChannelEnabled(ch);
		enableChannel(ch, true);

		auto samps = getSamples(num_samples, true);
		double avg = Utils::average(samps.at(ch).data(), num_samples);

		m_trigger->setAnalogMode(ch, mode);
		enableChannel(ch, enabled);
		return avg;
	}

	std::vector<double> getVoltage()
	{
		size_t num_samples = 100;
		std::vector<double> avgs;
		std::vector<M2K_TRIGGER_MODE> modes = {};
		std::vector<bool> enabled = {};

		for (unsigned int i = 0; i < getNbChannels(); i++) {
			enabled.push_back(isChannelEnabled(i));
			enableChannel(i, true);
			modes.push_back(m_trigger->getAnalogMode(i));
			m_trigger->setAnalogMode(i, ALWAYS);
		}
		auto samps = getSamples(num_samples, true);
		for (unsigned int i = 0; i < getNbChannels(); i++) {
			avgs.push_back(Utils::average(samps.at(i).data(), num_samples));
			m_trigger->setAnalogMode(i, modes.at(i));
			enableChannel(i, enabled.at(i));
		}
		return avgs;
	}

	double getScalingFactor(ANALOG_IN_CHANNEL ch)
	{
		return (0.78 / ((1 << 11) * 1.3 *
				getValueForRange(m_input_range.at(ch))) *
			m_adc_calib_gain.at(ch) *
			getFilterCompensation(getSampleRate()));
	}

	double getScalingFactor(unsigned int ch)
	{
		ANALOG_IN_CHANNEL channel = static_cast<ANALOG_IN_CHANNEL>(ch);
		return getScalingFactor(channel);
	}

	std::pair<double, double> getHysteresisRange(ANALOG_IN_CHANNEL chn)
	{
		std::pair<double, double> m2k_range = getRangeLimits(getRange(chn));
		return std::pair<double, double>(0, m2k_range.second / 10);
	}

	M2K_TRIGGER_CONDITION getAnalogCondition(ANALOG_IN_CHANNEL chnIdx) const
	{
		M2K_TRIGGER_CONDITION res = NO_TRIGGER;
		__try {
			res = m_trigger->getAnalogCondition(chnIdx);
		} __catch (exception_type &e) {
			throw_exception(EXC_RUNTIME_ERROR, "M2KAnalogIn trigger condition error: "  +
					string(e.what()));
		}
		return res;
	}

	void setAnalogCondition(ANALOG_IN_CHANNEL chnIdx, M2K_TRIGGER_CONDITION cond)
	{
		m_trigger->setAnalogCondition(chnIdx, cond);
	}

	M2K_TRIGGER_CONDITION getDigitalCondition(ANALOG_IN_CHANNEL chnIdx) const
	{
		M2K_TRIGGER_CONDITION res = NO_TRIGGER;
		__try {
			res = m_trigger->getDigitalCondition(chnIdx);
		} __catch (exception_type &e) {
			throw_exception(EXC_INVALID_PARAMETER, "M2KAnalogIn trigger condition error: "  +
					string(e.what()));
		}
		return res;
	}

	void setDigitalCondition(ANALOG_IN_CHANNEL chnIdx, M2K_TRIGGER_CONDITION cond)
	{
		m_trigger->setDigitalCondition(chnIdx, cond);
	}

	void setRange(ANALOG_IN_CHANNEL channel, M2K_RANGE range)
	{
		const char *str_gain_mode = "";

		m_input_range[channel] = range;
		if (range == PLUS_MINUS_2_5V) {
			str_gain_mode = "high";
		} else {
			str_gain_mode = "low";
		}

		m_m2k_fabric->setStringValue(channel, "gain", std::string(str_gain_mode));
		m_trigger->setCalibParameters(channel, getScalingFactor(channel), m_adc_hw_offset.at(channel));
	}

	void setRange(ANALOG_IN_CHANNEL channel, double min, double max)
	{

		if ((min >= HIGH_MIN) && (max <= HIGH_MAX)) {
			setRange(channel, PLUS_MINUS_2_5V);
		} else {
			setRange(channel, PLUS_MINUS_25V);
		}
	}

	M2K_RANGE getRange(ANALOG_IN_CHANNEL channel)
	{
		return m_input_range[channel];
	}

	M2K_RANGE getRangeDevice(ANALOG_IN_CHANNEL channel)
	{
		M2K_RANGE range = PLUS_MINUS_25V;
		auto gain = m_m2k_fabric->getStringValue(channel, "gain");
		if (gain == "high") {
			range = PLUS_MINUS_2_5V;
		} else {
			range = PLUS_MINUS_25V;
		}
		return range;
	}

	std::pair<double, double> getRangeLimits(M2K_RANGE range)
	{
		if (range == PLUS_MINUS_25V) {
			return std::pair<double, double>(LOW_MIN, LOW_MAX);
		} else {
			return std::pair<double, double>(HIGH_MIN, HIGH_MAX);
		}
	}

	std::vector<M2K_RANGE> getAvailableRanges()
	{
		std::vector<M2K_RANGE> ranges = {};
		return ranges;
	}

	double getOversamplingRatio()
	{
		return getDoubleValue("oversampling_ratio");
	}

	double getOversamplingRatio(unsigned int chn_idx)
	{
		return getDoubleValue(chn_idx, "oversampling_ratio");
	}

	double setOversamplingRatio(double oversampling_ratio)
	{
		return setDoubleValue(oversampling_ratio, "oversampling_ratio");
	}

	double setOversamplingRatio(unsigned int chn_idx, double oversampling_ratio)
	{
		return setDoubleValue(chn_idx, oversampling_ratio, "oversampling_ratio");
	}


	double getSampleRate()
	{
		return getDoubleValue("sampling_frequency");
	}

	double getSampleRate(unsigned int chn_idx)
	{
		return getDoubleValue(chn_idx, "sampling_frequency");
	}

	double setSampleRate(double samplerate)
	{
		return setDoubleValue(samplerate, "sampling_frequency");
		m_trigger->setCalibParameters(0, getScalingFactor(0), m_adc_hw_offset.at(0));
		m_trigger->setCalibParameters(1, getScalingFactor(1), m_adc_hw_offset.at(1));
	}

	double setSampleRate(unsigned int chn_idx, double samplerate)
	{
		return setDoubleValue(chn_idx, samplerate, "sampling_frequency");
		m_trigger->setCalibParameters(0, getScalingFactor(0), m_adc_hw_offset.at(0));
		m_trigger->setCalibParameters(1, getScalingFactor(1), m_adc_hw_offset.at(1));
	}

	double getFilterCompensation(double samplerate)
	{
		return m_filter_compensation_table.at(samplerate);
	}

	double getValueForRange(M2K_RANGE range)
	{
		if (range == PLUS_MINUS_25V) {
			return 0.02017;
		} else if (range == PLUS_MINUS_2_5V) {
			return 0.21229;
		} else {
			return 0;
		}
	}

private:
	std::shared_ptr<DeviceGeneric> m_m2k_fabric;
	bool m_need_processing;

	double m_samplerate;
	libm2k::analog::M2kHardwareTrigger *m_trigger;
	std::vector<M2K_RANGE> m_input_range;

	std::vector<double> m_adc_calib_gain;
	std::vector<double> m_adc_calib_offset;
	std::vector<double> m_adc_hw_offset;
	std::map<double, double> m_filter_compensation_table;
};
