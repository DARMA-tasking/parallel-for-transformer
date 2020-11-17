#!/usr/bin/env perl

use strict;
use warnings;

my $file = shift @ARGV;

my $t = `mktemp`;
chomp $t;

print "Running on file \"$file\"; outputting to file $t\n";

open my $wd, ">", $t;

my $last = -1;
my $i = 0;
open my $fd, "<", $file;
foreach (<$fd>) {
    if (/Kokkos::fence\(\);/) {
	if ($last == $i-1) {
	    chomp;
	    print "Found multi in file \"$file\" on line $i: \"$_\"\n";
	} else {
	    print $wd "$_";
	}
	$last = $i;
    } else {
	print $wd "$_";
    }
    $i++;
}    
close $fd;
close $wd;

print `echo mv $t $file`;
print `mv $t $file`;

print "Finished writing to file $t\n";
