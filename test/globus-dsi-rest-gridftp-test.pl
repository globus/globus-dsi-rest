#! /usr/bin/perl

use strict;
use Test::More;
use URI::Escape;
use File::Temp qw(tempfile);

my $subject = $ENV{GLOBUS_FTP_CLIENT_TEST_SUBJECT};
my $base_uri = "gsiftp://$ENV{FTP_TEST_SOURCE_HOST}";
my $tmpdir = $ENV{TEMPDIR};

sub write_test
{
    my $size = $_[0];
    my $name = $_[1];
    my ($fh, $filename) = tempfile( DIR => $tmpdir );
    my $copy_ok;
    my $check_ok;
    my $check_uri = "$tmpdir/".uri_escape($filename);

    for (1..$size)
    {
        print $fh pack("c", int(rand(255)));
    }

    close($fh);

    my @copy_args = (
            "globus-url-copy",
            "-s", $subject,
            "file://$filename", "$base_uri$filename");
    $copy_ok = system(@copy_args);
    my @cmp_args = (
            "cmp",
            $filename,
            $check_uri);
    $check_ok = system(@cmp_args);

    ok ($copy_ok == 0 && $check_ok == 0, $name);
}

sub read_test
{
    my $size = $_[0];
    my $name = $_[1];
    my ($fh, $filename) = tempfile( DIR => $tmpdir );
    my $copy_ok;
    my $check_ok;
    my $check_uri = "$tmpdir/".uri_escape($filename);

    open($fh, ">$check_uri");
    for (1..$size)
    {
        print $fh pack("c", int(rand(255)));
    }
    close($fh);

    my @copy_args = (
            "globus-url-copy",
            "-s", $subject,
            "$base_uri$filename", $filename);
    $copy_ok = system(@copy_args);
    my @cmp_args = (
            "cmp",
            $filename,
            $check_uri);
    $check_ok = system(@cmp_args);

    ok ($copy_ok == 0 && $check_ok == 0, $name);
}

sub write_small_test
{
    write_test(12, "write_small_test");
}

sub write_boundary_test
{
    write_test(2*256*1024, "write_boundary_test");
}

sub write_big_test
{
    write_test(5000000, "write_big_test");
}

sub read_small_test
{
    read_test(12, "read_small_test");
}

sub read_boundary_test
{
    read_test(2*256*1024, "read_boundary_test");
}
sub read_big_test
{
    read_test(5000000, "read_big_test");
}


my @tests=qw( write_small_test write_boundary_test write_big_test read_small_test read_boundary_test read_big_test);

plan tests => scalar(@tests);

foreach (@tests) {
    eval "&$_";
}
