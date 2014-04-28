#!/usr/bin/perl

sub command {
	my ($cmd) = @_;

	print "$cmd\n";
	system "$cmd";
}

print "Looking for leftover processes...\n";
foreach my $process (`ps -e --no-headers`) {
	my ($pid, $tty, $time, $cmd) = split(' ',$process);
	system "grep '/usr/lib/libipaddr.so' /proc/$pid/maps 1>/dev/null 2>/dev/null";
	if ($? == -1) {
		# print "failed to execute: $!\n";
	} elsif ($? & 127) {
		# printf "child died with signal %d, %s coredump\n", ($? & 127),  ($? & 128) ? 'with' : 'without';
	} else {
		# printf "%d: child $cmd exit value\n", $? >> 8;
		my $exitvalue = $? >> 8;
		if ($exitvalue == 0) {
			print "Killing process $cmd...\n";
			command("kill -9 $pid");
		}
	}
}
