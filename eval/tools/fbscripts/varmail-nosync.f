#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Prevent creation/deletion overheads by simply opening one created file. Although the file is large since many appends result to it, we refer to varmail.f and change readwholefile to read meanappendsize.

set $dir=/mnt/
set $nfiles=10000
set $meandirwidth=5000
set $meanfilesize=32k
set $nthreads=8
set $iosize=1m
set $meanappendsize=4k

define fileset name=bigfileset,path=$dir,size=$meanfilesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile1,filesetname=bigfileset,fd=1
    flowop appendfilerand name=appendfilerand2,iosize=$meanappendsize,fd=1
    flowop closefile name=closefile2,fd=1
    flowop openfile name=openfile3,filesetname=bigfileset,fd=1
    flowop read name=readfile3,fd=1,iosize=$meanappendsize
    flowop appendfilerand name=appendfilerand3,iosize=$meanappendsize,fd=1
    flowop closefile name=closefile3,fd=1
    flowop openfile name=openfile4,filesetname=bigfileset,fd=1
    flowop read name=readfile4,fd=1,iosize=$meanappendsize
    flowop closefile name=closefile4,fd=1
  }
}

run 10
#psrun -10 7200

echo  "Varmail Version 3.0 personality successfully loaded"
usage "Usage: set \$dir=<dir>"
usage "       set \$meanfilesize=<size>    defaults to $meanfilesize"
usage "       set \$nfiles=<value>     defaults to $nfiles"
usage "       set \$nthreads=<value>   defaults to $nthreads"
usage "       set \$meanappendsize=<value> defaults to $meanappendsize"
usage "       set \$iosize=<size>  defaults to $iosize"
usage "       set \$meandirwidth=<size> defaults to $meandirwidth"
usage "       run runtime (e.g. run 60)"
