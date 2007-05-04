// Copyright (C) 2000, International Business Machines
// Corporation and others.  All Rights Reserved.
#ifndef _BCP_MESSAGE_H
#define _BCP_MESSAGE_H

// This file is fully docified.

#include <utility>

#include "BCP_string.hpp"
#include "BCP_message_tag.hpp"
#include "BCP_vector.hpp"


//#############################################################################

class BCP_buffer;
class USER_initialize;

//#############################################################################

/** This class holds an array of processes.  */

class BCP_proc_array {
private:
    /**@name Disabled methods */
    /*@{*/
    /** The default constructor is declared but not defined to disable it. */
    BCP_proc_array(const BCP_proc_array&);
    /** The assignment operator is declared but not defined to disable it. */
    BCP_proc_array& operator=(const BCP_proc_array&);
    /*@}*/
private:
    /**@name Data members */
    /*@{*/
    /** Vector of all processes. */
    BCP_vec<int> _procs;
    /** Vector of free processes (subset of all processes). */
    BCP_vec<int> _free_procs;
    /*@}*/
public:
    /**@name Constructor and destructor */
    /*@{*/
    /** The default constructor creates an empty process array */
    BCP_proc_array() : _procs(), _free_procs() {}
    /** The destructor needs to do nothing */
    ~BCP_proc_array() {}
    /*@}*/

    /**@name Query methods */
    /*@{*/
    /** Determine if the process indexed by the argument is free or not. */
    inline bool is_free_proc(const int index) {
	const int proc = _procs[index];
	for (int i = _free_procs.size() - 1; i >= 0; --i)
	    if (_free_procs[i] == proc)
		return true;
	return false;
    }
    /** Return the index of the process given by its process id in the
	vector of all processes. */
    inline int index_of_proc(const int proc) {
	int i;
	for (i = _procs.size() - 1; i >= 0; --i)
	    if (_procs[i] == proc)
		break;
	return i;
    }
    /** Return the vector of all processes (const version). */
    inline const BCP_vec<int>& procs() const { return _procs; }
    /** Return the id of the i-th process. */
    inline int process(const int i) const { return _procs[i]; }
    /** Return the total number of processes. */
    inline int size() const { return _procs.size(); }
    /** Return the number of free processes. */
    inline int free_num() const { return _free_procs.size(); }
    /** Return the number of busy (not free) processes. */
    inline int busy_num() const { return _procs.size() - _free_procs.size(); }
    /** Get the process id of a free process. Return 0 if there are none. */
    int get_free_proc() {
	if (_free_procs.size() > 0) {
	    int proc = _free_procs.back();
	    _free_procs.pop_back();
	    return proc;
	}
	return 0;
    }
    /*@}*/

    /**@name Modifying methods */
    /*@{*/
    /** Purge all process ids from the process array. */
    inline void clear() {
	_procs.clear();
	_free_procs.clear();
    }
    /** Append a process id to the end of the vector of all processes and mark
	it as free. */
   inline void add_proc(int proc_id) {
       _procs.push_back(proc_id);
       // the new proc is free to begin with
       _free_procs.push_back(proc_id);
   }
    /** Append the processes in <code>[first,last)</code> to the end of
	the vector of all processes and mark them as free. */
   inline void add_procs(BCP_vec<int>::const_iterator first,
			 BCP_vec<int>::const_iterator last) {
       _procs.insert(_procs.end(), first, last);
       // the new procs are free to begin with
       _free_procs.insert(_free_procs.end(), first, last);
   }
    /** Delete the process indexed by the argument from the vector of
	all processes (and also from the vector of free processes if
	applicable) */
    inline void delete_proc(const int index) { 
	const int proc = _procs[index]; 
	int i; 
	for (i = _free_procs.size() - 1; i >= 0; --i)
	    if (_free_procs[i] == proc)
		break;
	if (i >= 0)
	    _free_procs.erase(_free_procs.entry(i));
	_procs.erase(_procs.entry(index));
    }

    // *FIXME* check if the process is in _procs! 
    /** Append the process to the end of the vector of free processes.
	The process to be set free must already be in the vector of all
	processes. */
    void set_proc_free(int proc) {
	_free_procs.push_back(proc);
    }
    /*@}*/
};

//#############################################################################

const int BCP_AnyProcess = -1;

/** This is an abstract base class that describes the message passing
    environment. The implementation of the message passing protocol
    must implement this class. 

    All methods are pure virtual, enforcing the correct overriding of
    the methods. */

class BCP_message_environment{
public:
    /**@name Destructor */
    /*@{*/
    /** Being virtual, the destructor invokes the destructor for the real 
	type of the object being deleted. */
    virtual ~BCP_message_environment() {}
    /*@}*/
    
    /**@name Registering a process */
    /*@{*/
    /** A process can register (receive its process id) with the message
	passing environment. */
    virtual int register_process(USER_initialize* user_init) = 0;
    /*@}*/
    /**@name Identify parent process */
    /*@{*/
    /** Return the process id of the parent process (the process that
	spawned the currnet process. Returns null if the process does
	not have a parent.  */
    virtual int parent_process() = 0;
    /*@}*/

    /**@name Testing processes */
    /*@{*/
    /** Test if the process given by the argument is alive or
	not. Return true if alive, false otherwise. */
    virtual bool alive(const int pid) = 0;
    /** Test if the processes given by the process array in the argument
	are alive or not. Return an iterator to the first dead process,
	or to the end of the array if there are no dead processes. */
    virtual BCP_vec<int>::const_iterator alive(const BCP_proc_array& parray)=0;
    /*@}*/

    /**@name Send to one process */
    /*@{*/
    /** Send an empty message (message tag only) to the process given
	by the frist argument. */
    virtual void send(const int target, const BCP_message_tag tag) = 0;
    /** Send the message in the buffer with the given message tag to the
	process given by the first argument. */
    virtual void send(const int target, const BCP_message_tag tag,
		      const BCP_buffer& buf) = 0;
    /*@}*/

    /**@name Broadcasting */
    /*@{*/
    /** Send an empty message (message tag only) to all the processes in
	the process array. */
    virtual void multicast(const BCP_proc_array& target,
			   const BCP_message_tag tag) = 0;
    /** Send the message in the buffer with the given message tag to all
	processes in the process array. */
    virtual void multicast(const BCP_proc_array& target,
			   const BCP_message_tag tag,
			   const BCP_buffer& buf) = 0;
    /*@}*/

    /**@name Selective broadcasting */
    /*@{*/
    /** Send an empty message (message tag only) to the processes in
	<code>[beg, end)</code>. */
    virtual void multicast(BCP_vec<int>::const_iterator beg,
			   BCP_vec<int>::const_iterator end,
			   const BCP_message_tag tag) = 0;
    /** Send the message in the buffer with the given message tag to the
	processes in <code>[beg, end)</code>. */
    virtual void multicast(BCP_vec<int>::const_iterator beg,
			   BCP_vec<int>::const_iterator end,
			   const BCP_message_tag tag,
			   const BCP_buffer& buf) = 0;

    /** Send an empty message (message tag only) to every process in
	<code>procs</code>. */
    inline void multicast(const BCP_vec<int>& procs,
			  const BCP_message_tag tag)
    {
	multicast(procs.begin(), procs.end(), tag);
    }
    /** Send the message in the buffer with the given message tag to every 
	process in <code>procs</code>. */
    inline void multicast(const BCP_vec<int>& procs,
			  const BCP_message_tag tag,
			  const BCP_buffer& buf)
    {
	multicast(procs.begin(), procs.end(), tag, buf);
    }
    /*@}*/

    // blocking receive w/ timeout (ms??) from given source given msgtag (can
    // give wildcard anymsg, anyproc)

    /**@name Receiving */
    /*@{*/
    /** Blocking receive with timeout. Wait until a message is received from
	the given process with the given message tag within the given
	timelimit. If <code>timeout</code> is positive then the receive routine
	times out after this many seconds. If it is negative then the
	method blocks until a message is received. The received message is
	saved into the buffer. With a 0 pointer as the first argument (or the
	predefined <code>BCP_AnyProcess</code> process id can be used) a
	message from any process will be accepted. Messages with any message
	tag are accepted if the second argument is
	<code>BCP_Msg_AnyMessage</code>. */
    virtual void receive(const int source,
			 const BCP_message_tag tag, BCP_buffer& buf,
			 const double timeout) = 0;
    /** Probe if there are any messages from the given process with the
	given message tag. Return true if such a message is found, false
	otherwise. Note that the message is not "read", only its
	existence is checked. Similarly as above, the wild cards
	<code>BCP_AnyProcess</code> and <code>BCP_Msg_AnyMessage</code>
	can be used. */
    virtual bool probe(const int source, const BCP_message_tag tag) = 0; 
    /*@}*/

    /**@name Starting a process */
    /*@{*/
    /** Spawn a new process. The first argument contains the path to the
	executable to be spawned. If the second argument is set to true, then
	the executable is spawned under a debugger. */
    virtual int start_process(const BCP_string& exe,
			      const bool debug) = 0;
    /** Spawn a new process on the machine specified by the second argument. */
    virtual int start_process(const BCP_string& exe,
			      const BCP_string& machine,
			      const bool debug) = 0;
    /** Spawn <code>proc_num</code> processes, all with the same executable. */
    virtual BCP_proc_array* start_processes(const BCP_string& exe,
					    const int proc_num,
					    const bool debug) = 0;
    /** Spawn <code>proc_num</code> processes on the machines given by
	the third argument, all with the same executable. */
    virtual BCP_proc_array* start_processes(const BCP_string& exe,
					    const int proc_num,
					    const BCP_vec<BCP_string>& machines,
					    const bool debug) = 0;
    /*@}*/
  
    //    virtual void stop_process(int process) = 0;
    //    virtual void stop_processes(BCP_proc_array& processes) = 0;
};

#endif
