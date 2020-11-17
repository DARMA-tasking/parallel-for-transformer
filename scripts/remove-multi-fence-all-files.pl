#!/usr/bin/env perl

use strict;
use warnings;

my $file = shift @ARGV;

open my $fd, "<", $file;
foreach (<$fd>) {
    chomp;
    print "Processing file: $_\n";
    print `/ascldap/users/jliffla/parallel-for-transformer/scripts/remove-multi-fence.pl $_`;
}    
close $fd;
