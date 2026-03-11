#!/usr/bin/perl -w

use strict;

my %config_excluded = (
  unittests => 1,
);

my( $settings_path, $format ) = @ARGV;

die "usage: $0 <settings.dat> <xml|ini>\n"
  unless defined $settings_path && defined $format;
die "unknown config format '$format'\n"
  unless $format eq 'xml' || $format eq 'ini';

open my $settings_fh, '<', $settings_path
  or die "unable to open '$settings_path': $!\n";

my %options;

while( <$settings_fh> ) {
  next if /^\s*$/;
  next if /^\s*#/;

  chomp;

  my( $name, $type, $default, $short, $commandline, $configfile ) =
    split /\s*,\s*/;

  if( !defined $commandline || $commandline eq '' ) {
    $commandline = $name;
    $commandline =~ s/_/-/g;
  }

  if( !defined $configfile || $configfile eq '' ) {
    $configfile = $commandline;
    $configfile =~ s/-//g;
  }

  $options{$name} = {
    type => $type,
    default => $default,
    configfile => $configfile,
  };
}

close $settings_fh;

sub normalize_string_default {
  my( $value ) = @_;

  return undef if !defined $value || $value eq 'NULL';

  if( $value =~ /^"(.*)"$/ ) {
    $value = $1;
  }

  $value =~ s/\\"/"/g;
  $value =~ s/\\\\/\\/g;

  return $value;
}

sub xml_escape {
  my( $value ) = @_;

  $value =~ s/&/&amp;/g;
  $value =~ s/</&lt;/g;
  $value =~ s/>/&gt;/g;

  return $value;
}

if( $format eq 'xml' ) {
  print "<?xml version=\"1.0\"?>\n";
  print "<settings>\n";

  foreach my $name ( sort keys %options ) {
    my $option = $options{$name};
    next if $config_excluded{$name};
    my $config_name = $option->{configfile};
    my $type = $option->{type};
    my $default = $option->{default};

    if( $type eq 'boolean' || $type eq 'numeric' ) {
      print "  <$config_name>$default</$config_name>\n";
    } elsif( $type eq 'string' ) {
      my $value = normalize_string_default( $default );
      next unless defined $value;
      print '  <', $config_name, '>', xml_escape( $value ), '</',
        $config_name, ">\n";
    }
  }

  print "</settings>\n";
} else {
  foreach my $name ( sort keys %options ) {
    my $option = $options{$name};
    next if $config_excluded{$name};
    my $config_name = $option->{configfile};
    my $type = $option->{type};
    my $default = $option->{default};

    if( $type eq 'boolean' || $type eq 'numeric' ) {
      print "$config_name=$default\n";
    } elsif( $type eq 'string' ) {
      my $value = normalize_string_default( $default );
      next unless defined $value;
      print "$config_name=$value\n";
    }
  }
}