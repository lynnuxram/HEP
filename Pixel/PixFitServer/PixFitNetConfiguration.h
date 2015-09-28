/** @file PixFitNetConfiguration.h
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz
 */

#ifndef PIXFITNETCONFIGURATION_H_
#define PIXFITNETCONFIGURATION_H_

#include <string>
#include <memory>

#include <netinet/in.h> // for in_addr, in_port_t
#include <arpa/inet.h> // for inet_pton

namespace PixLib {

class PixFitNet;

/** Identifies a histogramming unit in the IBL sense by sepcifying the crate, ROD, slave FPGA, and
 * which histogramming unit is being used on the slave. */
struct HistoUnit
{
	int crate;
	int rod;
	int slave;
	int histo;
        std::string crateletter;

	/* We need to be able to compare HistoUnits in the assembler. operator< is needed so that the
	 * HistoUnit can be used as a key in a std::map. */
	bool operator==(const HistoUnit& rhs) const;
	bool operator!=(const HistoUnit& rhs) const;
	bool operator<(const HistoUnit& rhs) const;

	/** Outputs the member variables that identify the histo unit to std::cout. */
	void printHistoUnit() const;

	/** Returns a string containing the histogrammer identification. */
	std::string makeHistoString() const;
};

/** Holds the configuration settings that are needed for every instance of PixFitNet. Objects of
 * this type are created by a helper function in PixFitManager and assigned to their corresponding
 * PixFitNet objects. */
class PixFitNetConfiguration
{
public:
    PixFitNetConfiguration();
    virtual ~PixFitNetConfiguration();

    /* Getter and setter methods. */
    in_addr getLocalIpAddress() const;
    int setLocalIpAddress(std::string ipAddress);

    in_addr getRemoteIpAddress() const;
    int setRemoteIpAddress(std::string ipAddress);

	in_port_t getLocalPort() const;
	int setLocalPort(in_port_t port);

	const HistoUnit* getHistogrammer() const;
	void setHistogrammer(HistoUnit histogrammer);

	/** Enables the corresponding networking thread. */
    void enable();

    /** Disables the corresponding networking thread (i.e. no corresponding network object is created). */
    void disable();

    /** Checks if network interface for the histogramming unit is enabled. */
	bool isActive() const;

private:
	/** IPv4 address of the local network endpoint. */
	in_addr m_localIpAddress;

	/** Local TCP listening port. */
	in_port_t m_localPort;

	/** Remote IPv4 address of the ROD slave histogramming unit.
	 * @todo Could be used for cross-checking that connection is correctly set up. */
	in_addr m_remoteIpAddress;

	/** The histogramming unit that is delivering data to this network endpoint */
	HistoUnit m_histogrammer;

	/** Sets the histogramming unit/network thread as active so that the FitServer can receive data. */
	bool m_active;
};
} /* end of namespace PixLib */

#endif /* PIXFITNETCONFIGURATION_H_ */
