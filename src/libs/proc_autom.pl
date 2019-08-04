#!/usr/bin/perl

sub init_state {
    my ($i);
    for ($i = 0; $i < 128; $i++) {
	$autom[$i] = 255;
    }
}

sub output_state {
    my ($state,@autom) = @_;
    my ($i); 
    print "static unsigned char ",$base,"_elem",$state,"[] = {\n  ";
    for ($i = 0; $i < 128; $i++) {
	print $autom[$i],",";
	if (($i+1)%16 == 0) {
	    print "\n  ";
	}
    }
    print "};\n";
}

if ($#ARGV < 0) {
    die "usage: $0 file.x";
}

$file = $ARGV[0];
($base = $file) =~ s/\.\w+//;

open(F,$file) || die "Can't open $file\n";
$state = -1;
@autom = ();
while (<F>) {
    next if /^#/ || /^\s*$/;
    if (/^state/) {
	chop;
	@x = split;
	if ($state >= 0) {
	    output_state($state,@autom);
	}
	$state = $x[1];
	init_state;
    }
    else {
	next unless /^\s*'(.*)'\s+(\d+)\s*(\w*)/;
	$in_char = $1;
	$next_status = $2;
	$skip = $3;
	if ($in_char eq '\t') { $char = ord("\t"); }
	elsif ($in_char eq '\n') { $char = ord("\n"); }
	elsif ($in_char eq '\r') { $char = ord("\r"); }
	else {
	    $char = ord($in_char);
	}
	if ($skip eq "skip") {
	    $next_status |= 0x80;
	}
	$autom[$char] = $next_status;
    }
}
if ($state >= 0) {
    output_state($state,@autom);
}
print "static unsigned char *",$base,"[] = {\n";
for ($i = 0; $i <= $state; $i++) {
    print "  ",$base,"_elem",$i,",\n";
}
print "};\n";
