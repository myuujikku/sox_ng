#! /bin/sh

# Run all the tests on all the sox versions
#
# To test a different executable than the system "sox",
# set environment variable "sox" to the name or path to the executable.

args="$*"
if [ -z "$args" ]
then
    args="$(ls)"
fi

# 14.0.1		$HOME/SoX/sox-14.0.1/src/sox
# 14.2.0		$HOME/SoX/sox-14.2.0/src/sox
# 14.4.2A		$HOME/SoX/sox-14.4.2A/src/sox
# 42b355A		$HOME/SoX/sox-code-42b355A/src/sox
# bookworm		$HOME/SoX/sox-14.4.2+git20190427-3.5/src/sox
# bookwormA		$HOME/SoX/sox-14.4.2+git20190427-3.5A/src/sox
# trixieA		$HOME/SoX/sox-14.4.2+git20190427-4A/src/sox
# sox_ngA		$HOME/SoX/sox_ngA/src/sox_ng
versions="\
14.4.2		$HOME/SoX/sox-14.4.2/src/sox
trixie		$HOME/SoX/sox-14.4.2+git20190427-4/src/sox
42b355		$HOME/SoX/sox-code-42b355/src/sox
sox-code	$HOME/SoX/sox-code/src/sox
sox_ng		$HOME/SoX/sox_ng/src/sox_ng
sox_ngA		$HOME/SoX/sox_ngA/src/sox_ng"

# We don't want memory leaks to say it failed.
ASAN_OPTIONS=detect_leaks=0
export ASAN_OPTIONS

# Printf formats for bug names and for versions/results
hformat="%-16s"
vformat="%-9s"	# Now variable-width columns - see below

# Make sure all the executables exist
fail=false
echo "$versions" | while read version exe
do
    if [ ! -x "$exe" ]
    then
	echo "${version}'s exe $exe doesn't exist"
	fail=true
    fi
done
$fail && exit 1

errs=/tmp/checkall$$

strlen() {
    printf "$1" | wc -c
}

# Print the legend
cat << \EOF
Legend
OK   The test succeeded and sox succeeded or failed as it should
SUCC sox "succeeded" (exit 0) when it should have failed (exit 2)
ABRT sox Aborted (core dumped)
SEGV sox got a Segmentation fault (core dumped)
FPE  sox got a Floating Point Exception (core dumped)
LOOP sox ran for more than one minute of CPU.
EXEC Can't execute sox. Missing shared libraries also provoke this.
ASAN The Address Sanitizer reports problems other than memory leaks:
     buffer overflows, freeing free memory, running out of VM.
     It means exit(1), which sox only gives for command-line errors.
     With the Address Sanitizer you get ASAN instead of SEGV and FPE.

EOF

# Print the header
printf $hformat "BUG"
echo "$versions" | while read version exe
do
    # Variable-width column formatting.
    # We print the bug name without a following space and the results with
    # a preceding one so it doesn't overflow 80 columns.
    vformat=" %-$(strlen $version)s"
    printf "$vformat" "$version"
done
printf '\n'

# Print the test results
for bug in $args
do
    if [ -d "$bug" -a -f "$bug"/run ]
    then
	printf $hformat "$bug"
	echo "$versions" | while read version exe
        do (
	    vformat=" %-$(strlen $version)s"
	    cd "$bug"
	    # cfarm185 takes 3m09 to run CVE-2019-8357 so max 4m
	    ulimit -t 240
	    sox="$exe" sh run > $errs 2>&1
	    status=$?
	    case $status in
	    0)   result=OK ;;
	    1)   result=ASAN ;;
	    127) result=EXEC ;;
	    134) result=ABRT ;;
	    136) result=FPE ;;
	    137) result=LOOP ;;
	    139) result=SEGV ;;
	    255) result=SUCC ;;
	    *) result="$status" ;;
	    esac
	    printf "$vformat" "$result"
	    rm $errs
	)
	done
	printf '\n'
    fi
done
