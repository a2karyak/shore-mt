#!/bin/csh  -f
if ($#argv<1) then
	echo usage: $0 \<directory containing .ps files\>
	exit 1;
endif
if (-d $1) then
	cd $1
	foreach i (shore sdl cxxlb oc svas ssm sthread common fc)
		echo printing $i man pages...
		cat *.$i.ps | psnup -p4 -R | lpr 
	end
else
	echo usage: $0 \<directory containing .ps files\>
	echo Relies on PRINTER environment variable.
endif
