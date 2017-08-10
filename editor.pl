#!/usr/bin/perl
#
# This file demonstrates automatically generating an editor for the NVRAM
# variables using Doxygen <http://www.stack.nl/~dimitri/doxygen/> to extract
# type information from the C file in which the NVRAM variables are defined.
#
# An alternative way of doing this would be to use objdump
# <https://sourceware.org/binutils/docs/binutils/objdump.html> to find
# all variables in the NVRAM section. The command:
#
# 	objdump -j nvram -t nvram.elf
#
# Lists all of the variables in the section named "nvram" in the "nvram.elf"
# executable.
#
# Use:
# 	- Perl/Tk http://search.cpan.org/dist/Tk/pod/UserGuide.pod
# 	- TreePP http://search.cpan.org/dist/XML-TreePP/lib/XML/TreePP.pm
# 	- BigInts http://perldoc.perl.org/bigint.html
# 	- Pack/Unpack http://perldoc.perl.org/functions/pack.html
# 	- Getopts http://perldoc.perl.org/Getopt/Long.html
# 	- Datadumper https://perldoc.perl.org/Data/Dumper.html
# 
# @todo Make an XSD file to describe the generate XML file, and an XSLT file
# for displaying it as a webpage
# @todo The version number and format string should be checked, and the endianess
# determined. Initializers should also be decoded correctly and input allowed in
# decimal as well as hexadecimal? The format and the version fields should be protected
# from being edited.
# @todo The doxygen extraction of types needs to be improved to make it more
# robust.
#
use warnings;
use strict;
use Data::Dumper;
use Getopt::Long;
use XML::TreePP;
use Tk;

my $verbose   = 0;
my $noedit    = 0;
my $gui       = 0;
my $nv_xml    = "doxygen/xml/nvram_8c.xml";
my $nv_data   = "nvram.blk";
my $xml_out   = "$nv_data.xml";
my $alignment = 8; # @todo specify this in the nvram format
my $endian    = 1; # this can be determined by looking at the first variable
my $help      =<<HELP
This is an editor/data viewer for a binary format, the editor is 
automatically generated from an XML file generated by doxygen. Doxygen
is used to extract all of the variables within a special C program.

This script expects fixed width variables (int32_t, uint8_t, etecetera) 
to be declared with a 'NVRAM' macro, these variables are loaded and saved
automatically by the C program at statup and exit. The variables are
located within a special section within the executable that allows this.

	--xml,      -x  specify the XML file generated by doxygen
	--aligment, -a  set the alignment of the data
	--endian,   -e  specify the endianess of the data 1=little, 0=big
	--data,     -d  the file which contains the data
	--help,     -h  this help message
	--gui,      -g  run GUI editor instead of command line editor
	--output,   -o  specifiy output XML file, if desired
	--no-edit,  -n  do not launch the editor
	--verbose,  -v  turn on verbose mode

For more information, view the example C program at:
<https://github.com/howerj/nvram/blob/master/nvram.c> This script should 
be in the same repository. 

The Doxygen version used to create the XML file input was version "1.8.11",
future and previous versions of doxygen may not work.

HELP
;

GetOptions(
	"gui|g"          => \$gui,
	"xml|x=s"        => \$nv_xml,
	"alignment|a=i"  => \$alignment,
	"endian|e=i"     => \$endian,
	"data|d=s"       => \$nv_data,
	"verbose|v"      => \$verbose,
	"no-edit|n"      => \$noedit,
	"output|o"       => \$xml_out,
	"help|h"         => sub { print $help; exit; })
	or die "command line options error: try --help or -h for information";

my $settings =<<SETTINGS
XML:        '$nv_xml'
ALIGNMENT:  $alignment
ENDIAN:     $endian
DATA:       '$nv_data'
VEBOSITY:   $verbose
GUI MODE:   $gui
XML OUTPUT: '$xml_out'
NO EDIT:    $noedit
SETTINGS
;

print $settings if $verbose;

# @todo Use force_array/force_hash to make sure the data is how this function
# expects it. The error checking of this function could be improved.
sub unxml($)
{
	my ($file) = @_;
	my $tpp = XML::TreePP->new();
	my $nv_tree = $tpp->parsefile($file);
	my $members = $nv_tree->{doxygen}->{compounddef}->{sectiondef};
	die "no data" if not defined $members;

	my $vars  = undef;
	foreach my $member (@{$members}) {
		if($member->{-kind} eq "var") {
			$vars = $member;
			last;
		}
	}
	die "no variables in file" if not defined $vars;

	my @nv_vars;
	my $location = 0;

	# @todo Make sure all NVRAM variables are contiguously defined
	foreach my $var (@{$vars->{memberdef}}) {
		my $name   = $var->{name};
		my $type   = $var->{type};
		my $init   = $var->{initializer};
		my $brief  = $var->{briefdescription};
		my $detail = $var->{detaileddescription};
		next unless ($type =~ m/NVRAM/);
		my %info;
		$type =~ s/\s*NVRAM\s*//;
		unless ($type =~ m/(u)?int(8|16|32|64)_t/) {
			die "can only process fixed width types (got '$name' of type '$type')";
		}
		my $signed = defined $1 ? 0 : 1 ;
		my $bytes  = $2/8;
		$init =~ s/^\s*=\s*//;

		$info{name}     = $name;
		$info{signed}   = $signed;
		$info{bytes}    = $bytes;
		$info{init}     = $init;
		$info{location} = $location;
		# $info{brief}  = $brief;
		# $info{detail} = $detail;
		push @nv_vars, \%info;
		$location += $alignment;
	}

	return (\@nv_vars, $location);
}

sub load($$$$$) {
	my ($vars, $file, $size, $endian, $alignment) = @_;
	my $data;
	open FH, "<", $file or die "could not open $file for reading: $!";
	binmode FH;
	read FH, $data, $size;
	close FH;
	die "read too few bytes: $data != $size" if length $data != $size;

	my $i = 0;
	foreach my $var (@{$vars}) {
		my $skip = $i * $alignment;
		my $get  = $alignment * 2;
		$var->{data} = unpack "x$skip h$get", $data;
		$var->{data} = reverse $var->{data} if $endian;
		$i++;
	}
}

sub save($$$$$) {
	my ($vars, $file, $size, $endian, $alignment) = @_;
	my $data;

	foreach my $var (@{$vars}) {
		my $get  = $alignment * 2;
		my $value = reverse $var->{data};
		$data .= pack "h$get", $value;
	}

	open FH, ">", $file or die "could not open $file for writing: $!";
	binmode FH;
	print FH $data;
	close FH;
}

sub xml($$)
{
	my ($vars, $file) = @_;
	my %x;
	my $i = 0;
	foreach my $var(@{$vars}) {
		$x{nvram}->{variable}->[$i++] = $var;
	}

	my $tpp = XML::TreePP->new();
	$tpp->set(indent => 2);
	my $out = $tpp->write(\%x);

	open FH, ">", $file or die "could not open $file for writing: $!";
	print FH $out;
	close FH;
	return undef;
}

sub cli($)
{
	sub command($$)
	{
		my ($vars, $command) = @_;
		my $command_help = <<COMMANDS
This is the editor loop, type a variable name to edit it. Editing
is done in hexadecimal only (this is just a demonstation program and not
meant for serious use). Press CTRL-D to exit the edit loop.

All commands are prefixed with '-'. A list of commands:

	-list     List all variables that can be edited
	-help     This help text
	-end      Finish the edit loop (same as CTRL-D)
	-quit     Quit without saving
	-settings Print settings information

COMMANDS
;

		if($command eq "-help") {
			print $command_help;
		} elsif($command eq "-list") {
			foreach my $var (@{$vars}) {
				my $name = $var->{name};
				my $data = $var->{data};
				print "$name -> $data\n";
			}
		} elsif($command eq "-end") {
			return 1;
		} elsif($command eq "-quit") {
			print "exiting without saving\n";
			exit;
		} elsif($command eq "-settings") {
			print $settings;
		} elsif($command =~ m/^[ \t]*$/) {
			return 0;
		} else {
			print "unknown command $command\n";
			return undef;
		}
		return 0;
	}

	sub find($$) {
		my ($vars, $id) = @_;

		foreach my $var (@{$vars}) {
			return $var if($id eq $var->{name});
		}

		return undef;
	}

	sub edit($)
	{
		my ($var) = @_;
		print "value> ";
		my $value = <>;
		if (not defined $value) {
			print "invalid value\n";
			return;
		}
		chomp $value;
		$value =~ s/[ \t]+//g;
			
		if(not ($value =~ m/^[0-9a-fA-F]+$/)) {
			print "not a hexadecimal value: $value";
			return;
		}
		my $l = (2 * $alignment) - length $value;
		$value = ("0" x $l) . $value;
		$var->{data} = $value;
		return undef;
	}

	my ($vars) = @_;
	print "type '-help' for a list of commands\n";
	print "edit> ";
	while(<>) {
		chomp;
		my $name = $_;
		$name =~ s/[ \t]+//g;
		my $var  = &find($vars, $name);
		if(not defined $var) {
			my $r = &command($vars, $name);
			print "not a variable or command: '$name'\n" if not defined $r;
			last if defined $r and $r == 1;
		} else {
			print "$name -> " . $var->{data} . "\n";
			&edit($var);
		}

		print "edit> ";
	}
	print "\n";
}

sub gui($$$$$)
{
	my ($vars, $file, $size, $endian, $alignment) = @_;

	my $mw = new MainWindow;
	$mw->title("Perl/Tk NVRAM block editor");
	#$mw->geometry($mw->screenwidth . "x" . $mw->screenheight . "+0+0");
	my $form = $mw->Frame();
	$form->pack();

	my @labels;
	my @entries;
	foreach my $var(@{$vars}) {
		my $data = $var->{data};
		my $label = $form->Label(-text => $var->{name});
		my $entry = $form->Entry();
		$entry->configure(-state => "normal", -textvariable => \$data);
		$label->pack(-fill => 'both');
		$entry->pack(-fill => 'both');
		push @entries, $entry;
		push @labels, $label;
	}

	my $gui_save = sub {
		my $i = 0;
		foreach my $var(@{$vars}) {
			my $entry = $entries[$i];
			my $value = $entry->get();

			if ((not defined $value) or (not ($value =~ m/^[0-9a-fA-F]+$/))) {
				my $data = $var->{data};
				$entry->configure(-textvariable => \$data);
				next;
			}
			my $l = (2 * $alignment) - length $value;
			$value = ("0" x $l) . $value;
			$var->{data} = $value;
			$i++;
		}
		&save($vars, $file, $size, $endian, $alignment);
		# $form->messageBox(-icon => "info", -message => "Data Saved", -title => "Saved", -type => "Ok");
	};

	my $save_button = $form->Button(-text => 'Save', -command => $gui_save);
	$save_button->pack(-fill => 'both');
	$mw->bind('<KeyRelease-Return>' => $gui_save);
	$mw->bind('<KeyRelease-Escape>' => sub{ exit });

	MainLoop;
}

my ($nv_vars, $size) = &unxml($nv_xml);
&load($nv_vars, $nv_data, $size, $endian, $alignment);

if(not $noedit) {
	if($gui) {
		&gui($nv_vars, $nv_data, $size, $endian, $alignment);
	} else {
		&cli($nv_vars);
	}
	&save($nv_vars, $nv_data, $size, $endian, $alignment);
}

&xml($nv_vars, $xml_out) if defined $xml_out;

print Dumper($nv_vars) if $verbose;


