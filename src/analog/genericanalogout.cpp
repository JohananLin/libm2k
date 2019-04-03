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

#include <libm2k/analog/genericanalogout.hpp>
#include <libm2k/m2kexceptions.hpp>
#include <libm2k/utils/utils.hpp>
#include <iostream>

using namespace std;
using namespace libm2k::analog;
using namespace libm2k::utils;

GenericAnalogOut::GenericAnalogOut(iio_context *ctx, std::string dac_dev):
	Device(ctx, dac_dev)
{
}

GenericAnalogOut::~GenericAnalogOut()
{

}
double GenericAnalogOut::getSamplerate()
{
	return getDoubleValue("sampling_frequency");
}

double GenericAnalogOut::getSamplerate(unsigned int chn_idx)
{
	return getDoubleValue(chn_idx, "sampling_frequency", true);
}

double GenericAnalogOut::setSamplerate(double sampleRate)
{
	return setDoubleValue(sampleRate, "sampling_frequency");
}

double GenericAnalogOut::setSamplerate(unsigned int chn_idx, double sampleRate)
{
	return setDoubleValue(chn_idx, sampleRate,
			      "sampling_frequency", true);
}

std::vector<double> GenericAnalogOut::getAvailableSamplerates()
{
	return Device::getAvailableSamplerates();
}

void GenericAnalogOut::setCyclic(bool en)
{
	for (unsigned int i = 0; i < getNbChannels(); i++) {
		m_cyclic.at(i) = en;
	}
}

void GenericAnalogOut::setCyclic(bool en, unsigned int chn)
{
	if (chn >= getNbChannels()) {
		throw_exception(EXC_INVALID_PARAMETER, "Generic Analog Out: No such channel");
	}
	m_cyclic.at(chn) = en;
}

bool GenericAnalogOut::getCyclic(unsigned int chn)
{
	if (chn >= getNbChannels()) {
		throw_exception(EXC_INVALID_PARAMETER, "Generic Analog Out: No such channel");
	}
	return m_cyclic.at(chn);
}

void GenericAnalogOut::push(std::vector<short> &data, unsigned int chn_idx)
{
	Device::push(data, chn_idx, getCyclic(chn_idx));
}

void GenericAnalogOut::push(std::vector<double> &data, unsigned int chn_idx)
{
	Device::push(data, chn_idx, getCyclic(chn_idx));
}

void libm2k::analog::GenericAnalogOut::openAnalogOut()
{

}

void libm2k::analog::GenericAnalogOut::closeAnalogOut()
{

}

void GenericAnalogOut::stop()
{
	Device::stop();
}