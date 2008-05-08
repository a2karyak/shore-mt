/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*******************************************************************

	Hello World Value Added Server

This is a "hello world" program for the a value-added server
written using the Shore Storage Manager.

This program is rather long since it shows how to store, persistently,
the words "hello world".  The analogous program for Unix-like OS would
be one that fsck's all file sytems, partitions a new disk, creates a
file system on the disk, create a file holding the words "hello world"
and then prints the contents of the file.

*******************************************************************/

#include "sm_vas.h"
#include <iostream.h>
#include <sm_vas.h>

ss_m* ssm = 0;

// device for storing data
const char* device_name = "./device.hello";

// shorten error code type name
typedef w_rc_t rc_t;

rc_t
init_config_options(option_group_t& options)
{
    W_COERCE(options.add_class_level("example")); 	// program type
    W_COERCE(options.add_class_level("server"));  	// server or client
    W_COERCE(options.add_class_level("hello"));		// program name

    rc_t rc;	// return code
    const char* opt_file = "exampleconfig"; 	// option config file

    // have the SSM add its options to the group
    W_DO(ss_m::setup_options(&options));

    // read the config file to set options
    {
	w_ostrstream      err_stream;
	option_file_scan_t opt_scan(opt_file, &options);

	// scan the file and override any current option settings
	// options names must be spelled correctly
	rc = opt_scan.scan(true /*override*/, err_stream, true);
	if (rc) {
	    char* errmsg = err_stream.str();
	    cerr << "Error in reading option file: " << opt_file << endl;
	    cerr << "\t" << errmsg << endl;
	    if (errmsg) delete errmsg;
	    return rc;
	}
    }

    // check required options
    {
	w_ostrstream      err_stream;
	rc = options.check_required(&err_stream);
        char* errmsg = err_stream.str();
        if (rc) {
            cerr << "These required options are not set:" << endl;
            cerr << errmsg << endl;
	    if (errmsg) delete errmsg;
	    return rc;
        }
    } 

    return RCOK;
}

rc_t 
init_device_and_volume(bool _init_device, lvid_t& lvid, vid_t &handle)
{
    u_int	vol_cnt;
    devid_t	devid;

    vid_t 	vid(1);
    if(_init_device) {
	cout << "Formatting and mounting device " 
	    << device_name << "..." << endl;
	// format a 1000KB device for holding a volume
	W_DO(ssm->format_dev(device_name, 1000, true));
	cout << " ... " << device_name << " formatted." << endl;


	cout << "Generating a new long volume id..." << endl;
	W_DO(ssm->generate_new_lvid(lvid));
	cout << " ... long volume id =" << lvid << endl;
	//
	// Just for the sake of simplicity, we'll mount the "main"  or "root"
	// (and, in this case, only) volume as volume 1
	//
	cout << "Mounting device " << endl;
	W_DO(ssm->mount_dev(device_name, vol_cnt, devid));
	cout << " ... " << device_name << " contains " << vol_cnt
		    << " volumes." << endl;


	// create the new volume (1000KB quota)
	cout << "Creating a new volume on the device ..." << endl;
	W_DO(ssm->create_vol(device_name, lvid, 1000, false/*skip raw init*/, 
		    vid ));
	cout << " ... volume " << vid << " created. " << endl;

    } else {
	cout << "Using already existing device: " << device_name << endl;
	// mount already existing device
	w_rc_t rc = ssm->mount_dev(device_name, vol_cnt, devid, vid);
	if (rc) {
	    cerr << "Error: could not mount device: " << device_name << endl;
	    cerr << "   Did you forget to run with -i the first time?" 
		<< endl;
	    return rc;
	}
	cout << " ... " << device_name << " contains " << vol_cnt
		    << " volumes." << endl;
    }

    handle = vid;
    return RCOK;
}

rc_t
write_hello(const vid_t& handle, const vec_t& key, rid_t &rid)
{
    W_DO(ssm->begin_xct());

    cout << "Creating a file for holding the hello record ..." << endl;
    // create a file for holding the hello world record
    stid_t fid;	
    W_DO(ssm->create_file(handle, fid, ss_m::t_regular));
    cout << " ... file " << fid << " created." << endl;


    cout << "Creating the hello record ..." << endl;
    // just contains "Hello" in the body.
    const char* hello = "Hello";
    vec_t header_vec;
    vec_t body_vec(hello, strlen(hello));

    W_DO(ssm->create_rec(fid, header_vec, 0, body_vec, rid));
    cout << " ... record " << rid << " created."  << endl;

    // create a 2 part vector for "world" and "!" and append the 
    // vector to the new record
    vec_t world(" World", 6);
    world.put("!", 1);
    cout << "Appending to record ..." << world << endl;
    W_DO(ssm->append_rec(rid, world));
    cout << " ... " << world << "appended " << endl;

    // Stuff the record id into the root index
    stid_t rootIID;
    W_DO(ssm->vol_root_index(handle, rootIID));
    cout << "Volume root index is " << rootIID << endl; 


    cout << "Creating index entry for for key " << key 
	<< " --> " << rid << endl;
    vec_t el(&rid, sizeof(rid_t));
    W_DO(ssm->create_assoc(rootIID, key, el));

    W_DO(ssm->commit_xct());
    return RCOK;
}

rc_t
find_hello(const vid_t& handle, const vec_t &key, rid_t &rid)
{
    W_DO(ssm->begin_xct());
    
    // Find the record id from the root index
    stid_t rootIID;
    W_DO(ssm->vol_root_index(handle, rootIID));

    bool found(false);
    smsize_t elen(sizeof(rid_t));
    cout << "Finding record id for key " << key << endl;
    W_DO(ssm->find_assoc(rootIID, key, &rid, elen, found));
    if(found) {
	cout << " ... rid for key " << key << " is " << rid << endl;
    } else {
	W_COERCE(write_hello(handle, key, rid));
    }

    W_DO(ssm->commit_xct());
    return RCOK;
}

rc_t
say_hello(const rid_t& rid)
{
    W_DO(ssm->begin_xct());

    //  pin the vector so we can print it
    // open new scope so that pin_i destructor is called before commit
    {
	cout << "Pinning the record for printing ..." << endl;
	pin_i handle;

	// pin record starting at byte 0
	W_DO(handle.pin(rid, // record
		    	0    // starting byte
			//, lock mode = SH
			));

	cout << "Printing record  ..." << endl;
	// iterate over the bytes in the record body
	// to print "hello world!"
	cout << endl;
	for (smsize_t i = 0; i < handle.length(); i++) {
	    cout << handle.body()[i];
	}
	cout << endl;
    }

    W_DO(ssm->commit_xct());

    return RCOK;
}

class startup_smthread_t : public smthread_t
{
private:
        bool _init_device;

public:
        startup_smthread_t(bool init);
        ~startup_smthread_t() {}
        void run();
};

startup_smthread_t::startup_smthread_t(bool init) : smthread_t(
	t_regular, // priority
	"startup" // name
	//,  lock timeout default == WAIT_FOREVE
	//,  stack size ==default_stack 
	), _init_device(init) {}

void
startup_smthread_t::run()
{
    rc_t rc;
    cout << "Starting SSM and performing recovery ..." << endl;
    ssm = new ss_m();
    if (!ssm) {
	cerr << "Error: Out of memory for ss_m" << endl;
	return;
    }

    lvid_t lvid;  // long ID of volume for storing hello world data
    vid_t handle;  // short ID of same volume
    W_COERCE(init_device_and_volume(_init_device, lvid, handle));

    rid_t rid;
    vec_t KEY("HI",2);
    if(_init_device) {
	W_COERCE(write_hello(handle, KEY, rid));
    } else {
	W_COERCE(find_hello(handle, KEY, rid));
    }
    W_COERCE(say_hello(rid));

    cout << "\nShutting down SSM ..." << endl;
    delete ssm;
}


void usage(option_group_t &options)
{
    cerr << "Usage: hello [-i] [options]" << endl;
    cerr << "   -i will re-initialize the volume" << endl;
    options.print_usage(true, cerr);
}

int
main(int argc , char* argv[])
{
    cout << "processing configuration options ..." << endl;
    const int option_level_cnt ( 3 ); 
    option_group_t options(option_level_cnt);
    W_COERCE(init_config_options(options));

    bool init_device(true);
    if(argc > 2)
    {
	usage(options);
	::exit(1);
    } else if(argc==2) {
	// -r for read-only (already exists)
	if (strcmp(argv[1], "-r") == 0) {
	    init_device = false;
	} else {
	    usage(options);
	    ::exit(1);
	}
    }


    startup_smthread_t *doit = new startup_smthread_t(init_device);

    if(!doit) {
	W_FATAL(fcOUTOFMEMORY);
    }

    cerr << "forking ..." << endl;

    W_COERCE(doit->fork());

    cerr << "waiting ..." << endl;
    W_COERCE(doit->wait());
    delete doit;
    cout << "Finished!" << endl;
}
