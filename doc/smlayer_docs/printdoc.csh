#!/bin/csh  -f
if ($#argv<1) then
	echo usage: $0 \<directory containing .ps files\>
	exit 1;
endif
if (-d $1) then
	cd $1
	foreach i (betarelease installation plans sourceguide\
		running overview glossary \
		sdlman stree shrc dirscan poolscan\
		ssmvas ssmapi )
		if (-f $i.ps) then
			echo printing $i ...
			lpr $i.ps
#			cat $i.ps | psnup -p2 -R | lpr 
		else
			echo no such document: $i.ps
		endif
	end
else
	echo usage: $0 \<directory containing .ps files\>
	echo Relies on PRINTER environment variable.
endif
