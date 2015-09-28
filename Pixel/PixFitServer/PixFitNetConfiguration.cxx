/** @file PixFitNetConfiguration.cxx
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz, marx
 */

#include <iostream>
#include <memory>
#include <string>
#include <sstream>

#include "PixFitNetConfiguration.h"

using namespace PixLib;

PixFitNetConfiguration::PixFitNetConfiguration() {
}

PixFitNetConfiguration::~PixFitNetConfiguration() {
}

in_addr PixFitNetConfiguration::getLocalIpAddress() const {
	return m_localIpAddress;
}

int PixFitNetConfiguration::setLocalIpAddress(std::string ipAddress) {
	return inet_pton(AF_INET, ipAddress.c_str(), &(m_localIpAddress));
}

in_addr PixFitNetConfiguration::getRemoteIpAddress() const {
	return m_remoteIpAddress;
}

int PixFitNetConfiguration::setRemoteIpAddress(std::string ipAddress) {
	return inet_pton(AF_INET, ipAddress.c_str(), &(m_remoteIpAddress));
}

void PixFitNetConfiguration::enable() {
	m_active = true;
}

void PixFitNetConfiguration::disable() {
	m_active = false;
}

in_port_t PixFitNetConfiguration::getLocalPort() const {
	return m_localPort;
}

/* Sets the local listening port and checks that it is above the privileged port range. */
int PixFitNetConfiguration::setLocalPort(in_port_t port) {
	if (port > 1023) {
		m_localPort = htons(port);
		return 0;
	}
	else {
		return 1;
	}
}

const HistoUnit* PixFitNetConfiguration::getHistogrammer() const {
	return &m_histogrammer;
}

void PixFitNetConfiguration::setHistogrammer(HistoUnit histogrammer) {
	this->m_histogrammer = histogrammer;
}

bool PixFitNetConfiguration::isActive() const {
	return m_active;
}


bool HistoUnit::operator==(const HistoUnit& rhs) const {
	if (this->histo == rhs.histo && this->slave == rhs.slave && this->rod == rhs.rod && this->crate == rhs.crate) {
		return true;
	}
	else {
		return false;
	}
}

bool HistoUnit::operator!=(const HistoUnit& rhs) const {
	return !(*this == rhs);
}

/* Define the < operator so that HistoUnit type can be used as a key in std::map container. */
bool HistoUnit::operator <(const HistoUnit& rhs) const{
	if (crate < rhs.crate) return true;
	if (crate > rhs.crate) return false;
	if (rod < rhs.rod) return true;
	if (rod > rhs.rod) return false;
	if (slave < rhs.slave) return true;
	if (slave > rhs.slave) return false;
	if (histo < rhs.histo) return true;
	if (histo > rhs.histo) return false;

	/* Identical otherwise. */
	return false;
}

/* Print HistoUnit details to cout. */
void HistoUnit::printHistoUnit() const {
	std::cout << "(Crate / ROD / Slave / HistoUnit) : ("
			<< this->crate << " / "
			<< this->rod << " / "
			<< this->slave << " / "
			<< this->histo << ")" << std::endl;
}

std::string HistoUnit::makeHistoString() const {
	std::stringstream buf;
	buf << "[" << crate << " / " << rod << " / " << slave << " / " << histo << "]";
	return buf.str();
}
