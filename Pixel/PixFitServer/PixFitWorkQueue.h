/* @file PixFitWorkQueue.h
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz
 */

#ifndef PIXFITWORKQUEUE_H_
#define PIXFITWORKQUEUE_H_

#include <deque>		// for deque m_workQueue
#include <string>		// for queue name
#include <memory>
#include <functional>

#include <ers/ers.h>
#include <boost/thread.hpp>


namespace PixLib {

/** Used to supply minimalist thread-safe work queues for the multiple
 * producer/consumer use cases in the FitFarm software. Global locks are used during en/dequeueing.
 * @tparam T The type of objects that are put in the queue. */
template <class T> class PixFitWorkQueue {

public:
	/** @param queueName Name of the queue used for printouts. */
	PixFitWorkQueue(std::string queueName) {
		m_queueName = queueName;

		/* Use no blacklist by default. */
		isBlacklisted = [](std::shared_ptr<T>) -> bool {return false;};
	};

	virtual ~PixFitWorkQueue() {;};

	/** Associates a blacklisting function with the queue.
	 * @param fnct Predicate function that investigates an object from the queue. */
	void setBlackList(std::function<bool(std::shared_ptr<T>)> fnct) {
		isBlacklisted = fnct;
	}

	/** Adds a work item to the queue.
	 * @param workObject Pointer to an item. */
	int addWork(std::shared_ptr<T> workObject) {
		/* First check if item might be blacklisted. */
		if (isBlacklisted(workObject)) {
			if (s_doverbose) ERS_LOG(m_queueName << ": Thread " << boost::this_thread::get_id() <<
					" tried to add work item. Rejected due to blacklist. Queue size: " << getSize())
			return 0;
		}

		boost::lock_guard<boost::mutex> lock(m_mutex);
		m_workQueue.push_front(workObject);
		if (s_doverbose) ERS_LOG(m_queueName << ": Thread " << boost::this_thread::get_id() << " added work item. Queue size: " << getSize())
		m_cond.notify_one();
		return 0;
	}

	/** Calls non-blocking version of getWork().
	 * @returns Empty ptr if no data present in queue. */
	std::shared_ptr<T> getWorkNb() {
		return getWork(true);
	}

	/** Calls blocking version of getWork(). */
	std::shared_ptr<T> getWork() {
		return getWork(false);
	}

	/** Get the number of items in the queue. Watch out for thread-safety when using this. */
	int getSize() {
		return m_workQueue.size();	// is locking necessary here?
	}

	/** Clears the queue. */
	void clear() {
		boost::lock_guard<boost::mutex> lock(m_mutex);
		m_workQueue.clear();
	}

private:
	/** Removes and returns the last element of the queue if existent. Blocks if queue is locked or
	 * does not contain any elements for blocking calls.
	 * @param nonBlocking Choose blocking or non-blocking access.
	 * @returns Last element of the queue. */
	std::shared_ptr<T> getWork(bool nonBlocking) {
		std::shared_ptr<T> temp;
		boost::lock_guard<boost::mutex> lock(m_mutex);

		do {
			/* Wait for available work items in queue, return if nonBlocking. */
			while (0 == m_workQueue.size()) {
				if (nonBlocking) {
					ERS_LOG(m_queueName << ": No work item ready, returning non-blocking call. ")
					return std::shared_ptr<T>();
				}
				if(s_doverbose) ERS_LOG(m_queueName << ": Waiting for work item.")
				m_cond.wait(m_mutex);
			}

			temp = m_workQueue.back();
			m_workQueue.pop_back();

			if(s_doverbose && isBlacklisted(temp)) {
				ERS_LOG(m_queueName << ": Thread " << boost::this_thread::get_id() <<
						" tried to remove work item. REJECTED due to blacklist. Queue size: "
						<< getSize())
			}
		} while (isBlacklisted(temp));

		if(s_doverbose) {
			ERS_LOG(m_queueName << ": Thread " << boost::this_thread::get_id()
				<< " removed work item. Queue size: " << getSize())
		}
		return temp;
	}

	/** Contains the elements of the queue. */
	std::deque<std::shared_ptr<T> > m_workQueue;

	/** Mutex for synchronization. */
	boost::mutex m_mutex;

	/** Condition variable for synchronization. */
	boost::condition_variable_any m_cond;

	/** Queue identifier. */
	std::string m_queueName;

	/** Pointer to function that checks for aborted scans. */
	std::function<bool(std::shared_ptr<T>)> isBlacklisted;

	/** Controls the verbosity of the queue. */
	static const bool s_doverbose = true;
};

} /* end of namespace PixLib */

#endif /* PIXFITWORKQUEUE_H_ */
