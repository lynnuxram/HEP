/* @file PixFitThread.h
 *
 *  Created on: Jul 2, 2014
 *      Author: mkretz
 */

#ifndef PIXFITTHREAD_H_
#define PIXFITTHREAD_H_

#include <string>

#include <boost/thread.hpp>

namespace PixLib {

class PixFitInstanceConfig;

/** An abstract class that contains common functionality for managing threads.
 * It uses boost::thread as a means of spawning and joining threads.
 * Classes that spawn threads such as PixFitNet or PixFitAssembler inherit from this class.
 * @todo Implement ability to signal conditions to the thread.
 * @todo Implement status codes.
 * @todo Errors when no thread has been started yet, but stop/join etc. are being called. */
class PixFitThread {
public:
	PixFitThread();
	virtual ~PixFitThread();

	/** Spawns a thread and eventually calls the loop function. */
	void start();

	/** @todo Stop the thread. */
	void stop();

	/** Joins the thread. */
	void join();

	/** @returns Unique identifier for the thread. */
	boost::thread::id getThreadId();

	/** @return Handle on the thread. */
	boost::thread* getThread();

	/** @returns Status code of the thread. */
	virtual int getStatus();

protected:
	/** Name of the thread (type) used during printouts for easier identification. */
	std::string m_threadName;

	/** @todo Status of the thread. */
	int m_status;

	/** @todo Stopping the thread. */
	int m_stopFlag;

	/** The main loop of the thread that needs to be implemented by the inheriting class. */
	virtual void loop() = 0;

	/** Pointer to PixFitInstanceConfig object. */
	PixFitInstanceConfig *m_instanceConfig;

private:
	/** Gathers thread information and prints out basic info. */
	void setupThread();

	/** Identifier for the associated thread. */
	boost::thread::id m_threadId;

	/** The associated thread. */
	boost::thread m_thread;
};
} /* end of namespace PixLib */

#endif /* PIXFITTHREAD_H_ */
