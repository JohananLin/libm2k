/*
 * Copyright (c) 2020 Analog Devices Inc.
 *
 * This file is part of libm2k
 * (see http://www.github.com/analogdevicesinc/libm2k).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <include/libm2k/m2kexceptions.hpp>
#include "m2khardwaretrigger_v0.26_impl.hpp"
#include <algorithm>


const std::string libm2k::M2kHardwareTriggerV026Impl::m_source_attr = "trigger_src";
const std::string libm2k::M2kHardwareTriggerV026Impl::m_condition_attr = "trigger_condition";


std::vector<std::string> libm2k::M2kHardwareTriggerV026Impl::m_trigger_source = {
	"none",
	"trigger_i_0",
	"trigger_i_1",
	"trigger-adc",
	"trigger-la",
};

std::vector<std::string> libm2k::M2kHardwareTriggerV026Impl::m_trigger_cond = {
	"edge-rising",
	"edge-falling",
	"level-low",
	"level-high",
	"edge-any",
	"none"
};

libm2k::M2kHardwareTriggerV026Impl::M2kHardwareTriggerV026Impl(struct iio_context *ctx, bool init) :
	M2kHardwareTriggerV024Impl(ctx)
{
	bool hasTriggerSource;
	bool hasTriggerCondition;
	m_digital_trigger_device = make_shared<libm2k::utils::DeviceOut>(ctx, "m2k-logic-analyzer-tx");
	if (!m_digital_trigger_device) {
		throw_exception(EXC_INVALID_PARAMETER, "No digital TX trigger available");
	}
	hasTriggerSource = m_digital_trigger_device->hasGlobalAttribute(m_source_attr);
	hasTriggerCondition = m_digital_trigger_device->hasGlobalAttribute(m_condition_attr);
	if (!hasTriggerSource || !hasTriggerCondition) {
		throw_exception(EXC_INVALID_PARAMETER, "Analog TX trigger is not available");
	}

	if (init) {
		reset();
	}
}

void libm2k::M2kHardwareTriggerV026Impl::reset()
{
	M2kHardwareTriggerV024Impl::reset();
	setDigitalOutSource(SRC_OUT_NONE);
	setDigitalOutCondition(NO_TRIGGER_DIGITAL);

void libm2k::M2kHardwareTriggerV026Impl::setDigitalOutSource(libm2k::M2K_TRIGGER_OUT_SOURCE src)
{
	setTriggerOutSource(src, m_digital_trigger_device);
}

libm2k::M2K_TRIGGER_OUT_SOURCE libm2k::M2kHardwareTriggerV026Impl::getDigitalOutSource() const
{
	return getTriggerOutSource(m_digital_trigger_device);
}

void libm2k::M2kHardwareTriggerV026Impl::setDigitalOutCondition(libm2k::M2K_TRIGGER_CONDITION_DIGITAL cond)
{
	setTriggerOutCondition(cond, m_digital_trigger_device);
}

libm2k::M2K_TRIGGER_CONDITION_DIGITAL libm2k::M2kHardwareTriggerV026Impl::getDigitalOutCondition() const
{
	return getTriggerOutCondition(m_digital_trigger_device);
}
}

void libm2k::M2kHardwareTriggerV026Impl::setTriggerOutSource(libm2k::M2K_TRIGGER_OUT_SOURCE src,
							    const std::shared_ptr<libm2k::utils::DeviceOut>& device)
{
	if (src >= m_trigger_source.size()) {
		throw_exception(EXC_INVALID_PARAMETER, "M2kHardwareTrigger: "
						       "the provided source is not supported on "
						       "the current board; Check the firmware version.");
	}
	std::string src_str = m_trigger_source[src];
	device->setStringValue(m_source_attr, src_str);
}

libm2k::M2K_TRIGGER_OUT_SOURCE
libm2k::M2kHardwareTriggerV026Impl::getTriggerOutSource(const std::shared_ptr<libm2k::utils::DeviceOut>& device) const
{
	std::string buf = device->getStringValue(m_source_attr);

	auto it = std::find(m_trigger_source.begin(),
			    m_trigger_source.end(), buf.c_str());
	if  (it == m_trigger_source.end()) {
		throw_exception(EXC_OUT_OF_RANGE, "unexpected value read from attribute: " + m_source_attr);
	}

	return static_cast<libm2k::M2K_TRIGGER_OUT_SOURCE>(it - m_trigger_source.begin());
}

void libm2k::M2kHardwareTriggerV026Impl::setTriggerOutCondition(
	libm2k::M2K_TRIGGER_CONDITION_DIGITAL cond, const std::shared_ptr<libm2k::utils::DeviceOut>& device)
{
	if (cond >= m_trigger_cond.size()) {
		throw_exception(EXC_INVALID_PARAMETER, "M2kHardwareTrigger: "
						       "the provided condition is not supported on "
						       "the current board; Check the firmware version.");
	}
	std::string src_str = m_trigger_cond[cond];
	device->setStringValue(m_condition_attr, src_str);
}

libm2k::M2K_TRIGGER_CONDITION_DIGITAL
libm2k::M2kHardwareTriggerV026Impl::getTriggerOutCondition(const std::shared_ptr<libm2k::utils::DeviceOut>& device) const
{
	std::string buf = device->getStringValue(m_condition_attr);

	auto it = std::find(m_trigger_cond.begin(),
			    m_trigger_cond.end(), buf.c_str());
	if  (it == m_trigger_cond.end()) {
		throw_exception(EXC_OUT_OF_RANGE, "unexpected value read from attribute: " + m_condition_attr);
	}

	return static_cast<libm2k::M2K_TRIGGER_CONDITION_DIGITAL>(it - m_trigger_cond.begin());
}
