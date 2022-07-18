APPLICATION="./obj/$(hostname)/t_dmxrtuart"
if [ -f "$APPLICATION" ] ; then
#    chrt -r 99 $APPLICATION pc16c550 /dev/uio0 $*
    $APPLICATION pc16c550 /dev/uio0 $*
fi
