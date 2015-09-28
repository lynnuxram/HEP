/* @file PixFitThread.cpp
 *
 *  Created on: Jul 2, 2014
 *      Author: mkretz
 */

#include <boost/thread.hpp>

#include <string>

#include <ers/ers.h>

#include "PixFitThread.h"

using namespace PixLib;

PixFitThread::PixFitThread() {
	m_status = 0;
	m_threadName = "NONE";
	m_instanceConfig = nullptr;
}

PixFitThread::~PixFitThread() {
	// @todo call join here?
}

void PixFitThread::start() {
	m_thread = boost::thread(&PixFitThread::setupThread, this);
}

void PixFitThread::stop() {
}

void PixFitThread::join() {
	m_thread.join();
}

boost::thread::id PixFitThread::getThreadId() {
	return m_threadId;
}

boost::thread* PixFitThread::getThread() {
	return &m_thread;
}

int PixFitThread::getStatus() {
	return m_status;
}

void PixFitThread::setupThread() {
	m_threadId = boost::this_thread::get_id();
	ERS_DEBUG(1, "Started " << m_threadName << " thread with ID " << m_threadId)
	loop();
}
