#! /usr/bin/perl -w

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# This program is distributed with GNU Go, a Go program.        #
#                                                               #
# Write gnugo@gnu.org or see http://www.gnu.org/software/gnugo/ #
# for more information.                                         #
#                                                               #
# Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005 and 2006   #
# by the Free Software Foundation.                              #
#                                                               #
# This program is free software; you can redistribute it and/or #
# modify it under the terms of the GNU General Public License   #
# as published by the Free Software Foundation - version 2.     #
#                                                               #
# This program is distributed in the hope that it will be       #
# useful, but WITHOUT ANY WARRANTY; without even the implied    #
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR       #
# PURPOSE.  See the GNU General Public License in file COPYING  #
# for more details.                                             #
#                                                               #
# You should have received a copy of the GNU General Public     #
# License along with this program; if not, write to the Free    #
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor,   #
# Boston, MA 02111, USA.                                        #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

package GTP;

use strict;

my $debug = 0;

sub exec_cmd {
    my $hin = shift;
    my $hout = shift;
    my $cmd = shift;

# send the command to the GTP program

    print $hin "$cmd\n";

# parse the response of the GTP program

    my $line;
    my $repchar;
    my $result = "ERROR";
    $line = <$hout>;
    print STDERR "$hin 1:$line" if ($debug);
    return "ERROR" unless (defined $line);
    $line =~ s/\s*$//;
    ($repchar, $result) = split(/\s*/, $line, 2);
    print STDERR "$hin 2:repchar $repchar\n" if ($debug);
    print STDERR "$hin 3:result $result\n" if ($debug);

    $line = <$hout>;
    while (!($line =~ /^\s*$/)) {
		$result .= $line;
		$line = <$hout>;
    }
    print STDERR "$hin 4:$line" if ($debug);
    if ($repchar eq '?') {
		return "ERROR";
    }
    return $result;
}

sub standard_to_sgf {

    my $size = shift;
    my $board_coords = shift;
    $board_coords =~ tr/A-Z/a-z/;
    return "" if ($board_coords eq "pass");
    my $first  = substr($board_coords, 0, 1);
    my $number = substr($board_coords, 1);
    my $sgffirst;
    if ($first gt 'i') {
      $sgffirst = chr(ord($first) - 1);
    } else {
      $sgffirst = $first;
    }
    my $sgfsecond = chr(ord('a') + $size - $number);
#   print "$board_coords, $sgffirst, $number, $sgfsecond\n";
    return $sgffirst . $sgfsecond;
}

package GTP::Player;

use strict;
use Class::Struct;
use FileHandle;
use IPC::Open2;

struct('GTP::Player' => {
    'in'	    => 'FileHandle',
    'out'	    => 'FileHandle',
    'gtp_version'   => '$',
}
);

sub init {
    my $self = shift;
    return $self;
}

sub initialize {
    my $self = shift;
    my $cmd = shift;

    my $pid = open2($self->{out}, $self->{in}, $cmd);
    $self->{gtp_version} = GTP::exec_cmd($self->{in},
	$self->{out}, "protocol_version");
    $self->{gtp_version} eq 1 or $self->{gtp_version} eq 2 or
	die "Unsupported gtp version $self->{gtp_version}\n";
    return $pid;
}

sub genmove {
    my $self = shift;
    my $color = shift;

    my $cmd;
    if ($self->{gtp_version} eq 1) {
		$cmd = "genmove_";
    } else {
		$cmd = "genmove ";
    }
    if ($color =~ /^b/i) {
    	$cmd .= "black";
    } elsif ($color =~ /^w/i) {
    	$cmd .= "white";
    } else {
    	die "Illegal color $color\n";
    }
    my $move = GTP::exec_cmd($self->{in}, $self->{out}, $cmd);
}

sub black {
    my $self = shift;
    my $move = shift;
    my $cmd;
    if ($self->{gtp_version} eq 1) {
		$cmd = "black ";
    } else {
		$cmd = "play black ";
    }

    GTP::exec_cmd($self->{in}, $self->{out}, $cmd . $move);
}

sub white {
    my $self = shift;
    my $move = shift;
    my $cmd;
    if ($self->{gtp_version} eq 1) {
		$cmd = "white ";
    } else {
		$cmd = "play white ";
    }

    GTP::exec_cmd($self->{in}, $self->{out}, $cmd . $move);
}

sub komi {
    my $self = shift;
    my $komi = shift;

    GTP::exec_cmd($self->{in}, $self->{out}, "komi $komi");
}

sub boardsize {
    my $self = shift;
    my $size = shift;

    GTP::exec_cmd($self->{in}, $self->{out}, "boardsize $size");
}

sub clear_board {
    my $self = shift;

    GTP::exec_cmd($self->{in}, $self->{out}, "clear_board");
}

sub handicap {
    my $self = shift;
    my $handicap = shift;

    my $stones;
    $stones = GTP::exec_cmd($self->{in}, $self->{out}, "handicap $handicap");
    return split(' ', $stones);
}

sub fixed_handicap {
    my $self = shift;
    my $handicap = shift;

    my $stones;
    $stones = GTP::exec_cmd($self->{in}, $self->{out}, "fixed_handicap $handicap");
    return split(' ', $stones);
}

sub quit {
    my $self = shift;

    $self->{in}->print("quit\n");
}

sub showboard {
    my $self = shift;
    my $board;

    $board = GTP::exec_cmd($self->{in}, $self->{out}, "showboard");

    if ($self->{gtp_version} eq 2) {
	print $board;
    }
}

sub get_program_name {
    my $self = shift;

    my $name = GTP::exec_cmd($self->{in}, $self->{out}, "name");
    my $version = GTP::exec_cmd($self->{in}, $self->{out}, "version");
    return "$name $version";
}

sub score {
    my $self = shift;

    return GTP::exec_cmd($self->{in}, $self->{out}, "score");
}

sub final_score {
    my $self = shift;

    my $ret = GTP::exec_cmd($self->{in}, $self->{out}, "final_score");
    my ($result, $rest) = split(' ', $ret, 2);
    return $result;
}

sub timeset {
	my $self = shift;
	
	GTP::exec_cmd($self->{in}, $self->{out}, "time_settings 300 0 0");
	
	return;
}

package GTP::Game::Result;

use strict;
use Class::Struct;
use FileHandle;

struct('GTP::Game::Result' => {
    'resultw'	=> '$',
    'resultb'	=> '$'
}
);

package GTP::Game;

use strict;
use Class::Struct;
use FileHandle;

struct('GTP::Game' => {
    'black'		=> 'GTP::Player',
    'white'		=> 'GTP::Player',
    'size'		=> '$',
    'komi'		=> '$',
    'handicap'		=> '$',
    'handicap_stones'	=> '@',
    'moves'		=> '@',
    'result'		=> 'GTP::Game::Result'
}
);

my $verbose = 0;

sub verbose {
    my $self = shift;
    my $verbose_arg = shift;

    $verbose = $verbose_arg;
}

sub writesgf {
    my $self    = shift;
    my $sgffile = shift;

    my $size = $self->size;

    my $handle = new FileHandle;
    $handle->open(">$sgffile") or die "Can't write to $sgffile\n";
    my $black_name = $self->black->get_program_name;
    my $white_name = $self->white->get_program_name;    
    my $handicap = $self->handicap;
    my $komi = $self->komi;
    my $result = $self->{result}->resultw;

    print $handle "(;GM[1]FF[4]RU[Chinese]SZ[$size]HA[$handicap]KM[$komi]RE[$result]\n";
    print $handle "PW[$white_name]PB[$black_name]\n";
    if ($handicap > 1) {
		for my $stone (@{$self->handicap_stones}) {
	    	printf $handle "AB[%s]", GTP::standard_to_sgf($self->size, $stone);
		}
		print $handle "\n";
    }
    my $toplay = $self->handicap < 2 ? 'B' : 'W';
    for my $move (@{$self->moves}) {
		my $sgfmove = GTP::standard_to_sgf($size, $move);
		print $handle ";$toplay" . "[$sgfmove]\n";
		$toplay = $toplay  eq 'B' ? 'W' : 'B';
    }
    print $handle ")\n";
    $handle->close;
}



sub play {

    my $self = shift;
    my $sgffile = shift;    

    my $size     = $self->size;
    my $handicap = $self->handicap;
    my $komi     = $self->komi;

    print "Setting boardsize and komi for black\n" if $verbose;
    $self->black->boardsize($size);
    $self->black->clear_board();
    $self->black->komi($komi);
    $self->black->timeset();

    print "Setting boardsize and komi for white\n" if $verbose;
    $self->white->boardsize($size);
    $self->white->clear_board();
    $self->white->komi($komi);
    $self->white->timeset();

    my $pass = 0;
    my $resign = 0;
    my ($move, $toplay, $sgfmove);

    $pass = 0;
    $#{$self->handicap_stones} = -1;
    if ($handicap < 2) {

      $toplay = 'B';

    } else {

      @{$self->handicap_stones} = $self->white->fixed_handicap($handicap);
      for my $stone (@{$self->handicap_stones}) {
         $self->black->black($stone);
      }
      $toplay = 'W';

    }

    $#{$self->moves} = -1;
    while ($pass < 2 and $resign eq 0) {

		if ($toplay eq 'B') {

	    	$move = $self->black->genmove("black");
	    	if ($move eq "ERROR") {
				$self->writesgf($sgffile) if defined $sgffile;
				die "No response!";
	    	}
	    
	    	$resign = ($move =~ /resign/i) ? 1 : 0;
	    
	    	if ($resign) {
				print "Black resigns\n" if $verbose;
	    	} else {
				push @{$self->moves}, $move;
				print "Black plays $move\n" if $verbose;
				$pass = ($move =~ /PASS/i) ? $pass + 1 : 0;
				$self->white->black($move);
	    	}	    
	    	
	    	if ($verbose == 2) {
				$self->white->showboard;
	    	}

	    	$toplay = 'W';

        } else {

	    	$move = $self->white->genmove("white");
	    
	    	if ($move eq "ERROR") {
				$self->writesgf($sgffile) if defined $sgffile;
				die "No response!";
	    	}
	    	
	    	$resign = ($move =~ /resign/i) ? 1 : 0;
	    
	    	if ($resign) {
				print "White resigns\n" if $verbose;
	    	} else {
				push @{$self->moves}, $move;
				print "White plays $move\n" if $verbose;
				$pass = ($move =~ /PASS/i) ? $pass + 1 : 0;
				$self->black->white($move);
			}	    
	    	if ($verbose == 2) {
				$self->black->showboard;
	    	}
	    	
	    	$toplay = 'B';

        }
    }

    my $resultb;
    my $resultw;
    
    if ($resign) {
		$resultb = $toplay eq 'B' ? 'B+R' : 'W+R';
		$resultw = $resultb;
    } else {
		$resultw = $self->white->final_score;
		$resultb = $self->black->final_score;
    }
    
    if ($resultb eq $resultw) {
		print "Result: $resultw\n";
    } else {
		print "Result according to W: $resultw\n";
		print "****** according to B: $resultb\n";
    }
    $self->{result} = new GTP::Game::Result;
    $self->{result}->resultw($resultw);
    $self->{result}->resultb($resultb);
    $self->writesgf($sgffile) if defined $sgffile;
}

package GTP::Match;

use strict;
use Class::Struct;
use FileHandle;

struct('GTP::Match' => {
    'first'	=> 'GTP::Player',
    'second'	=> 'GTP::Player',
    'size'	=> '$',
    'komi'	=> '$',
    'handicap'	=> '$'
}
);

sub play {
    my $self = shift;
    my $games = shift;
    my $sgffile = shift;
    
    my $firstwins = 0;
    my $secondwins = 0;
    
    my @results;
    (my $sgffile_base = $sgffile) =~ s/\.sgf$//;
    
    for my $i (1..$games) {
    	my $game = new GTP::Game;    
    
	    $game->size($self->size);
	    $game->komi($self->komi);
	    $game->handicap($self->handicap);	    
	    
	    if (($i % 2) == 0) {
	    	$game->black($self->first);
	    	$game->white($self->second);
		} else {
			$game->black($self->second);
	    	$game->white($self->first);
		}	    		
    
        my $sgffile_game = sprintf "testgames\%s%03d.sgf", $sgffile_base, $i;
    	$game->play($sgffile_game);
		my $result = new GTP::Game::Result;
		$result->resultb($game->{result}->resultb);
		$result->resultw($game->{result}->resultw);
		
		if ($result->resultb =~ m/B/) {
			if (($i % 2) == 0) {
	    		$firstwins++;	    		
			} else {
				$secondwins++;
			}	    
		} else {
			if (($i % 2) == 0) {
	    		$secondwins++;	    		
			} else {
				$firstwins++;
			}	
		}
		
		print "--------------------------------------------------------\n";
		printf "======>Games: %d\tFirst wins: %d\tSecond wins: %d\n", $i, $firstwins, $secondwins;
		print "--------------------------------------------------------\n";
		
    	push @results, $result;
    }
    return @results;
}

package main;

use strict;
use Getopt::Long;
use FileHandle;

my $first;
my $second;
my $size = 19;
my $games = 1;
my $komi;
my $handicap = 0;
my $sgffile = "twogtp.sgf";

GetOptions(
	"first|w=s"	=> \$first,
	"second|b=s"	=> \$second,
	"verbose|v=i"	=> \$verbose,
	"komi|km=f"	=> \$komi,
	"handicap|ha=i"	=> \$handicap,
	"games|g=i"	=> \$games,
    "sgffile|f=s"	=> \$sgffile,
	"boardsize|size|s=i"	=> \$size
);

GTP::Game->verbose($verbose);

my $helpstring = "

Run with:

twogtp --first \'<path to program 1> --mode gtp [program options]\' \\
       --second \'<path to program 2> --mode gtp [program options]\' \\
       [twogtp options]

Possible twogtp options:

  --verbose 1 (to list moves) or --verbose 2 (to draw board)
  --komi <amount>
  --handicap <amount>
  --size <board size>                     (default 19)
  --games <number of games to play>       (-1 to play forever)
  --sgffile <filename>
";

die $helpstring unless defined $first and defined $second;

if (!defined $komi) {
    if ($handicap > 0) {
        $komi = 0.5;
    } else {
        $komi = 5.5;
    }
}

# create GTP players

my $first_pl = new GTP::Player;
$first_pl->initialize($first);
print "Created first GTP player\n" if $verbose;

my $second_pl = new GTP::Player;
$second_pl->initialize($second);
print "Created second GTP player\n" if $verbose;

my $match = new GTP::Match;
$match->first($first_pl);
$match->second($second_pl);
$match->size($size);
$match->komi($komi);
$match->handicap($handicap);
my @results = $match->play($games, $sgffile);

my $i=0;
for my $r (@results) {
    $i++;
    if ($r->resultb eq $r->resultw) {
		printf "Game $i: %s\n", $r->resultw;	
    }
    else {
		printf "Game $i: %s %s\n", $r->resultb, $r->resultw;
    }
}

$first_pl->quit;
$second_pl->quit;


