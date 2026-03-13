#!/usr/bin/perl -w

# keysyms.pl: generate keysyms.c from keysyms.dat
# Copyright (c) 2000-2013 Philip Kendall, Matan Ziv-Av, Russell Marks,
#			  Fredrick Meunier, Catalin Mihaila, Stuart Brady
# Copyright (c) 2015 Sergio Baldoví

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Author contact information:

# E-mail: philip-fuse@shadowmagic.org.uk

use strict;

use lib '../../perl';

use Fuse;

my $ui = shift;
$ui = 'sdl' unless defined $ui;

die "$0: unrecognised user interface: $ui\n"
    unless $ui eq 'sdl';

sub sdl_keysym ($) {

    my $keysym = shift;

    if ( $keysym =~ /[a-zA-Z][a-z]+/ ) {
	$keysym =~ tr/a-z/A-Z/;
    }
    $keysym =~ s/(.*)_L$/L$1/;
    $keysym =~ s/(.*)_R$/R$1/;
    
    # All the magic #defines start with `SDLK_'
    $keysym = "SDLK_$keysym";

    return $keysym;
}

sub sdl_unicode_keysym ($) {

    my $keysym = shift;

    if ( $keysym eq "'" ) {
        $keysym = "\\'";
    }
    $keysym = "'$keysym'";

    return $keysym;
}

# Parameters for each UI
my %ui_data = (

    sdl  => { headers => [ 'sdlcompat.h' ],
	      max_length => 18,
	      skips => { map { $_ => 1 } ( 'Hyper_L','Hyper_R','Caps_Lock',
                         'A' .. 'Z', 'asciitilde', 'bar', 'dead_circumflex',
                         'braceleft', 'braceright', 'percent' ) },
	      unicode_skips => { map { $_ => 1 } qw( Hyper_L Hyper_R Caps_Lock
                         Escape F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12
                         BackSpace Tab Caps_Lock Return Shift_L Shift_R
                         Control_L Control_R Alt_L Alt_R Meta_L Meta_R
                         Super_L Super_R Mode_switch Up Down Left Right
                         Insert Delete Home End Page_Up Page_Down KP_Enter
                         dead_circumflex ) },
	      translations => {
		  apostrophe  => 'QUOTE',
		  asciicircum => 'CARET',
		  bracketleft => 'LEFTBRACKET',
		  bracketright => 'RIGHTBRACKET',
		  exclam      => 'EXCLAIM',
		  Control_L   => 'LCTRL',	 
		  Control_R   => 'RCTRL',	 
		  equal       => 'EQUALS',
		  numbersign  => 'HASH',
		  Mode_switch => 'MENU',
		  Page_Up     => 'PAGEUP',
		  Page_Down   => 'PAGEDOWN',
		  parenleft   => 'LEFTPAREN',
		  parenright  => 'RIGHTPAREN',
	      },
	      unicode_translations => {
                  space       => ' ',
                  exclam      => '!',
                  dollar      => '$',
                  numbersign  => '#',
                  ampersand   => "&",
                  apostrophe  => "'",
                  asciitilde  => "~",
                  at          => "@",
                  backslash   => "\\\\",
                  braceleft   => "{",
                  braceright  => "}",
                  bracketleft => "[",
                  bracketright => "]",
                  parenleft   => "(",
                  parenright  => ")",
                  percent     => "%",
                  question    => "?",
                  quotedbl    => '\\"',
                  asterisk    => "*",
                  plus        => "+",
                  comma       => ',',
                  minus       => '-',
                  period      => '.',
                  slash       => '/',
                  colon       => ':',
                  semicolon   => ';',
                  less        => '<',
                  equal       => '=',
                  greater     => '>',
                  asciicircum => '^',
                  bar         => '|',
                  underscore  => '_',
	      },
	      function => \&sdl_keysym,
	      unicode_function => \&sdl_unicode_keysym,
	    },
);

# Translation table for any UI which uses keyboard mode K_MEDIUMRAW
my @cooked_keysyms = (
    # 0x00
    undef, 'ESCAPE', '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', 'MINUS', 'EQUAL', 'BACKSPACE', 'TAB',
    # 0x10
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', 'BRACKETLEFT', 'BRACKETRIGHT', 'RETURN', 'CONTROL_L', 'A', 'S',
    # 0x20
    'D', 'F', 'G', 'H', 'J', 'K', 'L', 'SEMICOLON',
    'APOSTROPHE', 'GRAVE', 'SHIFT_L', 'NUMBERSIGN', 'Z', 'X', 'C', 'V',
    # 0x30
    'B', 'N', 'M', 'COMMA', 'PERIOD', 'SLASH', 'SHIFT_R', 'KB_MULTIPLY',
    'ALT_L', 'SPACE', 'CAPS_LOCK', 'F1', 'F2', 'F3', 'F4', 'F5',
    # 0x40
    'F6', 'F7', 'F8', 'F9', 'F10', 'NUM_LOCK', 'SCROLL_LOCK', 'KP_7',
    'KP_8', 'KP_9', 'KP_MINUS', 'KP_4', 'KP_5', 'KP_6', 'KP_PLUS', 'KP_1',
    # 0x50
    'KP_2', 'KP_3', 'KP_0', 'KP_DECIMAL', undef, undef, 'BACKSLASH', 'F11',
    'F12', undef, undef, undef, undef, undef, undef, undef,
    # 0x60
    'KP_ENTER', 'CONTROL_R', 'KP_DIVIDE', 'PRINT', 'ALT_R', undef, 'HOME','UP',
    'PAGE_UP', 'LEFT', 'RIGHT', 'END', 'DOWN', 'PAGE_DOWN', 'INSERT', 'DELETE',
    # 0x70
    undef, undef, undef, undef, undef, undef, undef, 'BREAK',
    undef, undef, undef, undef, undef, 'WIN_L', 'WIN_R', 'MENU'
);

my @keys;
while(<>) {

    next if /^\s*$/;
    next if /^\s*\#/;

    chomp;
    s/\r$//;

    my( $keysym, $key1, $key2 ) = split /\s*,\s*/;

    push @keys, [ $keysym, $key1, $key2 ]

}

my $define = uc $ui;

print Fuse::GPL(
    'keysyms.c: UI keysym to Fuse input layer keysym mappings',
    "2000-2007 Philip Kendall, Matan Ziv-Av, Russell Marks\n" .
    "                           Fredrick Meunier, Catalin Mihaila, Stuart Brady" ),
    << "CODE";

/* This file is autogenerated from keysyms.dat by keysyms.pl.
   Do not edit unless you know what you're doing! */

#include <config.h>

CODE

# Comment to unbreak Emacs' perl mode

print << "CODE";

#include "input.h"
#include "keyboard.h"

CODE

# Comment to unbreak Emacs' perl mode

foreach my $header ( @{ $ui_data{$ui}{headers} } ) {
    print "#include <$header>\n";
}

print "\nkeysyms_map_t keysyms_map[] = {\n\n";

KEY:
foreach( @keys ) {

    my( $keysym ) = @$_;

    next if $ui_data{$ui}{skips}{$keysym};

    my $ui_keysym = $keysym;

    $ui_keysym = $ui_data{$ui}{translations}{$keysym} if
	$ui_data{$ui}{translations}{$keysym};

    $ui_keysym = $ui_data{$ui}{function}->( $ui_keysym );

	printf "  { %-$ui_data{$ui}{max_length}s INPUT_KEY_%-12s },\n",
            "$ui_keysym,", $keysym;

}

print << "CODE";

  { 0, 0 }			/* End marker: DO NOT MOVE! */

};

CODE

if( $ui eq 'sdl' ) {

print "\nkeysyms_map_t unicode_keysyms_map[] = {\n\n";

    foreach( @keys ) {

        my( $keysym ) = @$_;

        next if $ui_data{$ui}{unicode_skips}{$keysym};

        my $ui_keysym = $keysym;

        $ui_keysym = $ui_data{$ui}{unicode_translations}{$keysym} if
            $ui_data{$ui}{unicode_translations}{$keysym};

        $ui_keysym = $ui_data{$ui}{unicode_function}->( $ui_keysym );

	printf "  { %-$ui_data{$ui}{max_length}s INPUT_KEY_%-12s },\n",
            "$ui_keysym,", $keysym;

    }

print << "CODE";

  { 0, 0 }			/* End marker: DO NOT MOVE! */

};

CODE
}
