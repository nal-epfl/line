#!/usr/bin/perl

# Note: you must rebuild remotely if you change deploycore-template.pl

# Note: if you add more variables here, you need to substitute them manually
# in make-remote.sh (with perl -pi -e).
my $netdev = "eth7";
my $gateway = "192.168.77.2";

sub command {
	my ($cmd) = @_;

	print "$cmd\n";
	system "$cmd";
}

# Flush the routing table - only the 10/8 routes
foreach my $route (`netstat -rn | grep '^10.'`) {
	my ($dest, $gate, $linuxmask) = split ' ',$route;
	command("/sbin/route delete -net $dest netmask $linuxmask");
}

# Insert pf_ring driver, if necessary
# Note: transparent_mode=1 is only possible with PF_RING-patched drivers
#command("sh -c \'lsmod | grep pf_ring || modprobe pf-ring transparent_mode=1 enable_tx_capture=0\'");
command("sh -c \'lsmod | grep pf_ring || modprobe pf-ring transparent_mode=0 enable_tx_capture=0\'");

# enable routing
command "sh -c \'echo 1 > /proc/sys/net/ipv4/ip_forward\'";

# disable stuff that might interfere with our simulation
command "sh -c \'echo 0 | tee /proc/sys/net/ipv4/conf/*/rp_filter\'";
command "sh -c \'echo 1 | tee /proc/sys/net/ipv4/conf/*/accept_local\'";
command "sh -c \'echo 0 | tee /proc/sys/net/ipv4/conf/*/accept_redirects\'";
command "sh -c \'echo 0 | tee /proc/sys/net/ipv4/conf/*/send_redirects\'";

if ($gateway ne "127.0.0.1") {
	print "Gateway: $gateway\n";
	command "/sbin/route add -net 10.0.0.0/9 gw $gateway";
}

command "sh -c \'ifconfig $netdev mtu 1500\'";
# Disable PMTUD and segmentation offload since they might introduce jumbo frames
command "sh -c \'echo 1 > /proc/sys/net/ipv4/ip_no_pmtu_disc\'";
command "sh -c \'ethtool --offload $netdev tso off\'";
command "sh -c \'ethtool --offload $netdev gso off\'";
command "sh -c \'ethtool --offload $netdev gro off\'";

# Move all network card interrupts to CPU 1 (1 => 2 in a bitmask)
# Move all other interrupts to CPU 0 (0 => 1 in a bitmask)
command "sh -c 'IFS=\"\" cat /proc/interrupts " .
        "| while read -r line ; do " .
        "    echo \$line | grep --quiet \"[0-9][0-9]*:\" && " .
        "      echo -n \$(echo \$line | cut -d : -f 1 | tr -d \" \") && " .
        "      echo 1 > /proc/irq/\$(echo \$line | cut -d : -f 1 | tr -d \" \")/smp_affinity && " .
        "      echo -n \" CPU0\" && " .
        "      echo \$line | grep --quiet \"$netdev\" && " .
        "        echo 10 > /proc/irq/\$(echo \$line | cut -d : -f 1 | tr -d \" \")/smp_affinity && " .
        "        echo -n \" reassign to CPU1\"; " .
        "    echo \"\" ; " .
        " done'";
# Move all RCU threads to CPU 0
command "sh -c 'for i in \$(pgrep rcu) ; do taskset -pc 0 \$i ; done'";
# Move other threads to CPU 0
command "sh -c 'echo 1 > /sys/bus/workqueue/devices/writeback/cpumask'";
command "sh -c 'echo 0 > /sys/bus/workqueue/devices/writeback/numa'";
