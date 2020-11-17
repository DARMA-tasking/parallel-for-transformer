#!/usr/bin/env perl

use strict;
use warnings;

my $file = shift @ARGV;

my $t = `mktemp`;
chomp $t;

print "Compile commands: file=$file\nTemp file: $t\n";

print `grep '\"file\":' $file | cut -d: -f 2 > $t`;

open my $fd, "<", $t;
foreach (<$fd>) {
    chomp;
    print "Processing file $_\n";
    # print "/ascldap/users/jliffla/parallel-for-transformer-build/transform -p ~/transform-empire/build2/compile_commands.json $_ -I /sierra/sntools/SDK/compilers/clang/9.0-RHEL7/lib/clang/9.0.1/include/ -f \.\*transform-empire\.\* -x transformed.out";
    print `/ascldap/users/jliffla/parallel-for-transformer-build/transform -p ~/transform-empire/build2/compile_commands.json $_ -I /sierra/sntools/SDK/compilers/clang/9.0-RHEL7/lib/clang/9.0.1/include/ -f \.\*transform-empire\.\* -x transformed.out`;
}
close $fd;
