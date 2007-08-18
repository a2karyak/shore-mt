# <std-header style='tcl' orig-src='shore'>
#
#  $Id: smsh.tcl,v 1.1 2007/05/18 21:46:25 nhall Exp $
#
# SHORE -- Scalable Heterogeneous Object REpository
#
# Copyright (c) 1994-99 Computer Sciences Department, University of
#                       Wisconsin -- Madison
# All Rights Reserved.
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
# OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
# "AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
# FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# This software was developed with support by the Advanced Research
# Project Agency, ARPA order number 018 (formerly 8230), monitored by
# the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
# Further funding for this work was provided by DARPA through
# Rome Research Laboratory Contract No. F30602-97-2-0247.
#
#   -- do not edit anything above this line --   </std-header>

set null_fid 0.0
set null_pid 0.0.0
set null_rid 0.0.0.0
set volid 0
set short_form %0100d
set short_form_len 100
set long_form %0200d
set long_form_len 200
set vlong_form_len 2000

set script_dir ./scripts

proc assert {x} {
    if {[__assert [uplevel eval $x]] == 0} {
        set level [info level]
        if [string length [info script]]  {
            set scriptName "script '[info script]'"
        }  else  {
            set scriptName "<no script>"
        }
  
        echo
        echo *** TCL assertion failed: $x  ***
        echo
        echo "at level $level in $scriptName."
        echo stack crawl:
        while {$level > 0}  {
            echo "  $level: [info level $level]"
            set level [expr $level - 1]
        }
        echo

        error "TCL assert {$x} failed."
    }
}

proc verbose2 {args} {
    #  ultra-verbose
    global verbose2_flag
    if {$verbose2_flag} { eval [concat echo $args] }
}

proc verbose {args} {
    global verbose_flag
    if {$verbose_flag} { eval [concat echo $args] }
}

proc is_set variable {
    global $variable
    if { "[info globals $variable]" == $variable } {
        return 1
    }

    return 0
}

proc set_config_info {} {
    global page_size max_small_rec lg_rec_page_space buffer_pool_size lid_cache_size max_btree_entry_size exts_on_page pages_per_ext object_cc multi_server serial_bits64 preemptive multi_threaded_xct logging 
    set config [sm config_info]

    foreach i { page_size max_small_rec lg_rec_page_space buffer_pool_size lid_cache_size max_btree_entry_size exts_on_page pages_per_ext object_cc multi_server serial_bits64 preemptive multi_threaded_xct logging } {
    	set $i [lindex $config [expr {[lsearch $config $i] + 1}] ]
    }

    addcleanupvars {page_size max_small_rec lg_rec_page_space buffer_pool_size lid_cache_size
				max_btree_entry_size exts_on_page pages_per_ext object_cc
				multi_server serial_bits64 preemptive multi_threaded_xct logging}
}

proc _restart { a } {
    global smsh_device_list
    global Use_logical_id
    sm restart $a
    foreach i $smsh_device_list {
	verbose remounting [lindex $i 0]
	if {$Use_logical_id} {
	    sm mount_dev [lindex $i 0]
	} else {
	    sm mount_dev [lindex $i 0] [lindex $i 2]
	}
   } 
}
proc restart { } {
    _restart false
}

proc cleanrestart { } {
    _restart true
}

#####################################
# Lock compatibility table
#
set compat(IS,IS) 	1
set compat(IS,IX) 	1
set compat(IS,SH) 	1
set compat(IS,SIX) 	1
set compat(IS,UD) 	0
set compat(IS,EX) 	0

set compat(IX,IS) 	1
set compat(IX,IX) 	1
set compat(IX,SH) 	0
set compat(IX,SIX) 	0
set compat(IX,UD) 	0
set compat(IX,EX) 	0

set compat(SH,IS) 	1
set compat(SH,IX) 	0
set compat(SH,SH) 	1
set compat(SH,SIX) 	0
set compat(SH,UD) 	1
set compat(SH,EX) 	0

set compat(SIX,IS) 	1
set compat(SIX,IX) 	0
set compat(SIX,SH) 	0
set compat(SIX,SIX) 	0
set compat(SIX,UD) 	0
set compat(SIX,EX) 	0

set compat(UD,IS) 	0
set compat(UD,IX) 	0
set compat(UD,SH) 	0
set compat(UD,SIX) 	0
set compat(UD,UD) 	0
set compat(UD,EX) 	0

set compat(EX,IS) 	0
set compat(EX,IX) 	0
set compat(EX,SH) 	0
set compat(EX,SIX) 	0
set compat(EX,UD) 	0
set compat(EX,EX) 	0

###################################
# Lock supremum table
#
set supremum(IS,IS)        IS
set supremum(IS,IX)        IX
set supremum(IS,SH)        SH
set supremum(IS,SIX)       SIX
set supremum(IS,UD)        UD
set supremum(IS,EX)        EX

set supremum(IX,IS)        IX
set supremum(IX,IX)        IX
set supremum(IX,SH)        SIX
set supremum(IX,SIX)       SIX
set supremum(IX,UD)        EX
set supremum(IX,EX)        EX

set supremum(SH,IS)        SH
set supremum(SH,IX)        SIX
set supremum(SH,SH)        SH
set supremum(SH,SIX)       SIX
set supremum(SH,UD)        UD
set supremum(SH,EX)        EX

set supremum(SIX,IS)       SIX
set supremum(SIX,IX)       SIX
set supremum(SIX,SH)       SIX
set supremum(SIX,SIX)      SIX
set supremum(SIX,UD)       SIX
set supremum(SIX,EX)       EX

set supremum(UD,IS)        UD
set supremum(UD,IX)        EX
set supremum(UD,SH)        UD
set supremum(UD,SIX)       SIX
set supremum(UD,UD)        UD
set supremum(UD,EX)        EX

set supremum(EX,IS)        EX
set supremum(EX,IX)        EX
set supremum(EX,SH)        EX
set supremum(EX,SIX)       EX
set supremum(EX,UD)        EX
set supremum(EX,EX)        EX

set in_st_trans 0	


# proc for printing only the non-zero disk  stats
proc dstats {volid} {
    global verbose_flag
    set x [sm xct]
    if {$x == 0} {
	sm begin_xct
    }
    if { [catch {sm get_du_statistics $volid nopretty audit} n] != 0} {
	if {$x == 0} {
	    sm commit_xct
	}
	echo  " got error in get_du_statistics: " $n
	return 1
    } else {
	set l [llength $n]
	set l [expr $l/2]
	for {set i 0} { $i <= $l} {incr i} {
	    set j [expr {$i * 2 + 1}]
	    if {[lindex $n $j] != 0} {
		if {$verbose_flag == 1} {
		    echo [lindex $n [expr {$j-1}]] [lindex $n $j]
		}
	    }
	}
    }
    if {$x == 0} {
	sm commit_xct
    }
    return 0
}
# proc for printing stats w/o auditing
proc dstatsnoaudit {volid} {
    global verbose_flag
    set x [sm xct]
    if {$x == 0} {
	verbose dstatsnoaudit starts tx
	sm begin_xct
    }
    verbose get_du_statts $volid nopretty noaudit
    set n [sm get_du_statistics $volid nopretty noaudit]
    set l [llength $n]
    set l [expr $l/2]
    for {set i 0} { $i <= $l} {incr i} {
	set j [expr {$i * 2 + 1}]
	if {[lindex $n $j] != 0} {
	    if {$verbose_flag == 1} {
		echo [lindex $n [expr {$j-1}]] [lindex $n $j]
	    }
	}
    }
    if {$x == 0} {
	verbose dstatsnoaudit commits tx [sm xct]
	sm commit_xct
    }
}
# print non-zero xct stats
proc pxstats { reset } {
    global verbose_flag
    if {$verbose_flag == 1} {
	set x [sm xct]
	if {$x == 0} {
	    echo pxstats: No transaction
	} else {
	    set n [sm gather_xct_stats $reset]
	    pnzstats $n
	}
    }
}

# print non-zero (global) stats
# whether or not verbose is on
proc pnzstats { n } {
    set result ""
    set l [llength $n]
    set l [expr $l/2]
    for {set i 0} { $i <= $l} {incr i} {
	set j [expr {$i * 2 + 1}]
	if {[lindex $n $j] != 0} {
	    append result [lindex $n [expr {$j-1}]] "\t " [lindex $n $j] "\n"
	}
    }
    return $result
}

proc pstats {} {
    global verbose_flag
    if {$verbose_flag == 1} {
	set x [sm xct]
	if {$x == 0} {
	    sm begin_xct
	}
	set n [sm gather_stats]
	pnzstats $n
	if {$x == 0} {
	    sm commit_xct
	}
    }
}

# proc for printing the given stat only (zero or non-zero)
proc select_stat {n whichstat} {
    set l [llength $n]
    for {set i 0} { $i <= $l} {incr i} {
        if {[string compare [lindex $n $i] $whichstat] == 0} {
	    set j [expr {$i + 1}]
            return [list [lindex $n $i] [lindex $n $j]]
        }
    }
    return [list $whichstat 0]
}

proc _checkstats {n string} {
    verbose checking stats ... $string
    set rec_pin_cnt [lindex [select_stat $n rec_pin_cnt] 1]
    set rec_unpin_cnt [lindex [select_stat $n rec_unpin_cnt] 1]
    set page_fix_cnt [lindex [select_stat $n page_fix_cnt] 1]
    set page_refix_cnt [lindex [select_stat $n page_refix_cnt] 1]
    set page_unfix_cnt [lindex [select_stat $n page_unfix_cnt] 1]

    # permit excess unpins and unfixes

    assert {expr $rec_pin_cnt <= $rec_unpin_cnt}
    assert {expr $page_fix_cnt + $page_refix_cnt <= $page_unfix_cnt}
}

proc checkstats {string} {
    set x [sm xct]
    if {$x == 0} {
	sm begin_xct
    }
    set n [sm gather_stats]
    if {$x == 0} {
	sm commit_xct
    }
    _checkstats $n $string
}

proc clearstats {} {
    set x [sm xct]
    if {$x == 0} {
	sm begin_xct
    }
    set n [sm gather_stats reset]
    if {$x == 0} {
	sm commit_xct
    }
    verbose checking stats ... clearstats
    _checkstats $n "clearstats"

    return $n
}

proc addcleanupvars { theVars } {
# add the list of variables in the theVars to "ok" variables
# only add things if variables is set since cleanup will not gripe if it's not
   global variables

   if [is_set variables]  {
       set variables [concat $variables $theVars]
   }
}

proc deletecleanupvars { theVars } {
# delete the list of variables in theVars if they exist in "ok" variables
   global variables

   foreach var $theVars  {
      while {1} {
         set i [lsearch $variables $var]
         if { $i != -1 } {
            set variables [lreplace $variables $i $i]
         } else {
	    break
	 }
      }
   }
}

proc cleanup { fileid } {
   global variables
   if [is_set variables] {
      # echo NOT first time : [info globals]
   } else { 
      #  echo first time
      set variables [info globals]
      # do a second time in order to have it include "variables"
      set variables [info globals]
      return 0
   }
   foreach j [info globals] {
	set found 0
	set l [llength $variables]
	for {set i 0} { $i <= $l} {incr i} {
	    if {[string compare [lindex $variables $i] $j] == 0} {
	       set found 1
	    }
	}
	if { $found == 0 } {
	   puts $fileid [concat GARBAGE: $j]
	}
   }
   set v [info globals]
   foreach j $variables {
	set found 0
	set l [llength $v]
	for {set i 0} { $i <= $l} {incr i} {
	    if {[string compare [lindex $v $i] $j] == 0} {
	       set found 1
	    }
	}
	if { $found == 0 } {
	   puts $fileid [concat MISSING OLD: $j]
	}
   }
  set variables [info globals]
  return 0
}

proc error_is { e y } {
    if {[string compare $e $y] == 0} {
	return 1
    }
    if {[string length $e] == 0} {
	if {[string compare $y E_OK] == 0} {
	    return 1
	}
    }
    return 0
}

proc get_key_type { st } {
    set list [sm get_store_info $st]
    return [ lindex $list 2 ]
}

proc get_cc_mode { st } {
    set list [sm get_store_info $st]
    return [ lindex $list 3 ]
}

set runscripts {}
proc runscripts { scr } {
    global runscripts
    set save_runscripts([info level]) $runscripts
    set runscripts $scr
    namespace eval :: {
	foreach script $runscripts {
	    echo

	    pecho "-->RUNNING: $script_dir/$script" 
	    set save_script([info level])  $script
	    set tm [time {source $script_dir/$script} 1]
	    # source $script_dir/$script
	    set script $save_script([info level])
	    pecho "-->TIME for $script:"  $tm
	    unset tm

	    if { $Use_logical_id == 0 } {
		if {$volid != 0} {
		    sm begin_xct
		    set vms [sm get_volume_meta_stats $volid]
		    sm commit_xct

		    echo -->VOLUME_META_STATS after $script: $vms
		    unset vms
		}
	    }
     
	    #echo MEM_STATS after $script: [sm mem_stats]

	    #checkstats  $script
	    puts stdout [concat -->CLEANUP AFTER  $script  : ]
	    cleanup stdout
	}
    }
    set runscripts $save_runscripts([info level])
}

proc formatlong { prefix mplier fmt x } {
    set res [format $fmt $x]
    for {set i 0} { $i <= $mplier } {incr i} {
         set res $prefix$res
    }
    return $res
}



