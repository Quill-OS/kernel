#!/bin/sh

spin() {
	trap 'exit 0' TERM
	i=0
	while :; do
		for c in / - \\ \|; do
			printf '%s\b' "${c}"
			sleep 0.1
			i=$((i+10))
		done
	done
}

echo
spin &
SPIN_PID=${!}
read -rsn1 -t 5 -p "Hit any key to stop auto-boot ... "
EXITCODE=${?}
kill ${SPIN_PID}
echo
exit ${EXITCODE}
