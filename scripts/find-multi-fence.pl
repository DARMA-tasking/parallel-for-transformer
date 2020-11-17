#!/usr/bin/env perl

use strict;
use warnings;

my $file = shift @ARGV;

my $last = -1;
my $i = 0;
open my $fd, "<", $file;
foreach (<$fd>) {
    if (/Kokkos::fence\(\);/) {
	if ($last == $i-1) {
	    chomp;
	    print "Found multi in file \"$file\" on line $i: \"$_\"\n";
	}
	$last = $i;
    }
    $i++;
}    
close $fd;
