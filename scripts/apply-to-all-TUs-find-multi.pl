#!/usr/bin/env perl

use strict;
use warnings;

my $file = shift @ARGV;

my $t = `mktemp`;
chomp $t;

print "Compile commands: file=$file\nTemp file: $t\n";

print `grep '\"file\":' $file | cut -d: -f 2 > $t`;

my $lines = `wc -l $t | cut -d' ' -f1`;
chomp $lines;

my $i = 0;
open my $fd, "<", $t;
foreach (<$fd>) {
    chomp;
    print "Processing [$i/$lines] file $_\n";
    print `/ascldap/users/jliffla/parallel-for-transformer/scripts/find-multi-fence.pl $_`;
    $i++;
}
close $fd;
